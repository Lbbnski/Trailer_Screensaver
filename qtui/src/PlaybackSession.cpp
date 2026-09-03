#include "PlaybackSession.h"

#include "MpvGLWidget.h"
#include "config/ConfigPaths.h"
#include "sources/gog/GogTrailerSource.h"
#include "sources/igdb/IgdbTrailerSource.h"
#include "sources/steam/SteamTrailerSource.h"
#include "sources/steam/YoutubeFallbackResolver.h"
#include "util/Logging.h"

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QTimer>

#include <algorithm>

namespace ssv {

namespace {

// Picks the rendition matching the user's resolution cap. Steam CDN
// candidates carry explicit "480"/"max" renditions to choose between;
// YouTube-fallback candidates carry exactly one rendition (approxHeight 0,
// container "youtube") whose actual cap is enforced separately via
// MpvPlayer::setYoutubeHeightCap at playback time.
TrailerRendition selectRendition(const QList<TrailerRendition>& renditions, MaxResolution cap)
{
    if (renditions.isEmpty())
        return {};

    if (cap == MaxResolution::P480) {
        for (const auto& r : renditions)
            if (r.approxHeight == 480 && r.container == QStringLiteral("mp4"))
                return r;
        for (const auto& r : renditions)
            if (r.approxHeight == 480)
                return r;
    } else {
        for (const auto& r : renditions)
            if (r.approxHeight == 0 && r.container == QStringLiteral("mp4"))
                return r;
        for (const auto& r : renditions)
            if (r.approxHeight == 0)
                return r;
    }
    return renditions.first();
}

// Delay before doing the (blocking) prefetch resolution for the next
// trailer — gives the current one a moment to actually start rendering
// first, so the brief stutter this causes lands mid-playback rather than
// right at the cut where it'd be most noticeable.
constexpr int kPrefetchDelayMs = 3000;

} // namespace

PlaybackSession::PlaybackSession(QObject* parent) : QObject(parent) {}

PlaybackSession::~PlaybackSession() = default;

const Config& PlaybackSession::config() const
{
    return m_config;
}

bool PlaybackSession::start(std::optional<Config> configOverride)
{
    m_config = configOverride ? *configOverride : Config::load();
    m_cacheDb = CacheDatabase::open(ConfigPaths::cacheDatabasePath());
    if (!m_cacheDb.isOpen()) {
        logError(QStringLiteral("failed to open cache database — aborting"));
        return false;
    }
    m_repo = std::make_unique<CacheRepository>(m_cacheDb);

    m_networkManager = new QNetworkAccessManager(this);
    // Qt's default "use the system proxy configuration" lookup can block
    // for a very long time on Windows (WinHTTP PAC-script auto-detection),
    // seemingly indefinitely for some hosts/network setups — observed
    // directly: the very first store.steampowered.com request would hang
    // well past its own 10s request timeout with zero CPU activity, while
    // steamspy.com requests (evaluated against the same PAC script/proxy
    // list) completed instantly. This app has no need for proxy support
    // sophistication, so just skip system proxy resolution entirely.
    m_networkManager->setProxy(QNetworkProxy::NoProxy);

    // Shared by every registered source that needs a YouTube-search
    // fallback (Steam always; IGDB only for the rare candidate with no
    // curated video of its own) — one resolver, one yt-dlp path/duration
    // cap/title-matching implementation, regardless of how many sources
    // are enabled.
    m_youtubeFallback = std::make_unique<YoutubeFallbackResolver>(
        m_config.advanced.ytDlpPath, m_config.advanced.maxTrailerDurationSeconds);

    const qint64 candidateListTtlSeconds = qint64(m_config.advanced.cacheTtlDaysCandidateList) * 24 * 3600;

    m_registry.registerSource(std::make_unique<SteamTrailerSource>(
        *m_networkManager, *m_repo,
        m_config.sources.steamLanguage, m_config.sources.steamCountryCode,
        *m_youtubeFallback, candidateListTtlSeconds));

    // GOG's storefront API needs no credentials, so — like Steam — it's
    // always registered; whether it actually contributes anything is
    // controlled purely by sources.enabled (see TrailerResolver::preparePool,
    // which only iterates the enabled ids), same pattern as Steam.
    m_registry.registerSource(std::make_unique<GogTrailerSource>(
        *m_networkManager, *m_repo, *m_youtubeFallback, candidateListTtlSeconds));

    // IGDB requires a free Twitch developer Client ID/Secret the user
    // configures themselves in Settings — listing "igdb" in sources.enabled
    // with either left blank degrades to "registered, but contributes
    // nothing" rather than failing, same as every other missing-config path
    // in this codebase.
    if (!m_config.sources.igdbClientId.isEmpty() && !m_config.sources.igdbClientSecret.isEmpty()) {
        m_registry.registerSource(std::make_unique<IgdbTrailerSource>(
            *m_networkManager, *m_repo,
            m_config.sources.igdbClientId, m_config.sources.igdbClientSecret,
            *m_youtubeFallback, candidateListTtlSeconds));
    } else if (m_config.sources.enabled.contains(QStringLiteral("igdb"), Qt::CaseInsensitive)) {
        logInfo(QStringLiteral("igdb listed in sources.enabled but no client id/secret configured — skipping"));
    }

    m_resolver = std::make_unique<TrailerResolver>(m_registry, *m_repo, m_config.advanced);
    m_playlistEngine = std::make_unique<PlaylistEngine>(*m_repo);

    GenreFilter filter;
    filter.mode = m_config.filter.mode;
    filter.genres = m_config.filter.genres;
    filter.preferPopular = m_config.filter.preferPopular;

    const auto pool = m_resolver->preparePool(m_config.sources.enabled, filter);
    m_playlist = m_playlistEngine->buildPlaylist(pool, m_config.filter);
    m_playlistIndex = 0;

    logInfo(QStringLiteral("session started: pool=%1 playlist=%2").arg(pool.size()).arg(m_playlist.size()));

    return true;
}

void PlaybackSession::attach(MpvGLWidget* widget)
{
    widget->initializePlayer(m_config.advanced.ytDlpPath);
    connect(widget->player(), &MpvPlayer::playbackEnded, this, [this, widget]() { advance(widget); });
    advance(widget);
}

void PlaybackSession::loadIntoPlayer(MpvGLWidget* widget, const TrailerCandidate& candidate, const TrailerRendition& rendition)
{
    auto* player = widget->player();
    player->setMuted(m_config.playback.muted);
    player->setHardwareDecodeEnabled(m_config.playback.hardwareDecode);
    if (rendition.container == QStringLiteral("youtube")) {
        const int cap = m_config.playback.maxResolution == MaxResolution::P480 ? 480 : 4320;
        player->setYoutubeHeightCap(cap);
    }
    player->loadFile(rendition.url);
    logInfo(QStringLiteral("advance: loading \"%1\" -> %2").arg(candidate.title, rendition.url));

    m_repo->recordPlayback(candidate.sourceId, candidate.nativeId, QDateTime::currentSecsSinceEpoch());
}

void PlaybackSession::advance(MpvGLWidget* widget)
{
    if (m_playlist.isEmpty())
        return;

    if (m_playlistIndex >= m_playlist.size())
        m_playlistIndex = 0; // loop the playlist indefinitely

    // If schedulePrefetch() already did this candidate's resolution work
    // during the previous trailer's playback, use it directly instead of
    // repeating the (blocking) work now — this is what actually removes
    // the visible gap between trailers. A prefetch only ever matches the
    // exact index it was computed for; anything else (playlist wrapped,
    // earlier attempts skipped ahead) just falls through to resolving
    // normally below, same as if there were no prefetch at all.
    if (m_prefetch && m_prefetch->playlistIndex == m_playlistIndex) {
        Prefetch pf = std::move(*m_prefetch);
        m_prefetch.reset();
        m_playlistIndex = (pf.playlistIndex + 1) % static_cast<int>(m_playlist.size());

        logInfo(QStringLiteral("advance: using prefetched \"%1\"").arg(pf.candidate.title));
        loadIntoPlayer(widget, pf.candidate, pf.rendition);
        schedulePrefetch();
        return;
    }

    // Bounded retry: an entry might fail fallback resolution (yt-dlp error,
    // removed video); try a handful of playlist entries before giving up
    // for this cycle rather than looping forever on an all-broken playlist.
    const int maxAttempts = std::min(static_cast<int>(m_playlist.size()), 10);
    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
        if (m_playlistIndex >= m_playlist.size())
            m_playlistIndex = 0;

        TrailerCandidate candidate = m_playlist[m_playlistIndex++];
        logInfo(QStringLiteral("advance: trying \"%1\" (%2/%3)").arg(candidate.title, candidate.sourceId, candidate.nativeId));
        if (!m_resolver->ensurePlayable(candidate)) {
            logWarning(QStringLiteral("advance: could not resolve a playable rendition for \"%1\"").arg(candidate.title));
            continue;
        }

        const auto rendition = selectRendition(candidate.renditions, m_config.playback.maxResolution);
        if (rendition.url.isEmpty()) {
            logWarning(QStringLiteral("advance: no rendition matched resolution cap for \"%1\"").arg(candidate.title));
            continue;
        }

        loadIntoPlayer(widget, candidate, rendition);
        schedulePrefetch();
        return;
    }

    logWarning(QStringLiteral("no playable candidate found after %1 attempts").arg(maxAttempts));
}

void PlaybackSession::schedulePrefetch()
{
    if (m_prefetchPending || m_playlist.isEmpty())
        return;
    m_prefetchPending = true;
    QTimer::singleShot(kPrefetchDelayMs, this, &PlaybackSession::runPrefetch);
}

void PlaybackSession::runPrefetch()
{
    m_prefetchPending = false;
    if (m_playlist.isEmpty() || m_prefetch)
        return;

    const int idx = m_playlistIndex % static_cast<int>(m_playlist.size());
    TrailerCandidate candidate = m_playlist[idx];
    if (!m_resolver->ensurePlayable(candidate))
        return; // not fatal — advance() just resolves it normally when it gets there

    const auto rendition = selectRendition(candidate.renditions, m_config.playback.maxResolution);
    if (rendition.url.isEmpty())
        return;

    logInfo(QStringLiteral("prefetch: resolved \"%1\" for playlist index %2").arg(candidate.title).arg(idx));
    m_prefetch = Prefetch{idx, std::move(candidate), rendition};
}

} // namespace ssv
