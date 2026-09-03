#include "sources/gog/GogTrailerSource.h"
#include "sources/GenreTaxonomy.h"
#include "sources/steam/YoutubeFallbackResolver.h"
#include "util/Logging.h"
#include "util/NetworkAwait.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>
#include <memory>

namespace ssv {

namespace {

QList<TrailerRendition> parseVideos(const QJsonArray& videos)
{
    QList<TrailerRendition> renditions;

    static const QRegularExpression youtubeIdPattern(QStringLiteral(R"re(embed/([A-Za-z0-9_-]+))re"));

    for (const auto& v : videos) {
        const auto entry = v.toObject();
        if (entry.value("provider").toString() != QStringLiteral("youtube"))
            continue; // Wistia (or any other provider) — see header comment

        const auto match = youtubeIdPattern.match(entry.value("video_url").toString());
        if (match.hasMatch())
            renditions << YoutubeFallbackResolver::renditionForVideoId(match.captured(1));
    }

    return renditions;
}

} // namespace

GogTrailerSource::GogTrailerSource(QNetworkAccessManager& networkManager, CacheRepository& repo,
                                    YoutubeFallbackResolver& youtubeFallback, qint64 candidateListTtlSeconds)
    : m_networkManager(networkManager)
    , m_repo(repo)
    , m_candidateFinder(networkManager)
    , m_youtubeFallback(youtubeFallback)
    , m_candidateListTtlSeconds(candidateListTtlSeconds)
{
}

QString GogTrailerSource::id() const
{
    return QStringLiteral("gog");
}

QList<QString> GogTrailerSource::discoverCandidates(const GenreFilter& filter, int requestBudget)
{
    QStringList genresToSearch;
    if (filter.mode == GenreFilter::Mode::AllowList) {
        genresToSearch = filter.genres.isEmpty() ? GenreTaxonomy::canonicalGenres() : filter.genres;
    } else {
        for (const auto& g : GenreTaxonomy::canonicalGenres()) {
            if (!filter.genres.contains(g, Qt::CaseInsensitive))
                genresToSearch << g;
        }
    }
    if (genresToSearch.isEmpty() || requestBudget <= 0)
        return {};

    QStringList discovered;
    int remaining = requestBudget;

    if (filter.preferPopular && remaining > 0) {
        const int spend = std::min(1, remaining);
        discovered << m_candidateFinder.topPlayed(spend, m_repo, m_candidateListTtlSeconds);
        remaining -= spend;
    }

    if (remaining <= 0) {
        discovered.removeDuplicates();
        return discovered;
    }

    // Shuffled so a request budget smaller than genresToSearch.size() (the
    // common case once TrailerResolver's trickle-discovery budget kicks
    // in) doesn't always exhaust itself on the same first few genres in a
    // fixed order every run.
    std::shuffle(genresToSearch.begin(), genresToSearch.end(), *QRandomGenerator::global());

    const int perGenreBudget = std::max(1, remaining / static_cast<int>(genresToSearch.size()));
    for (const auto& genre : genresToSearch) {
        if (remaining <= 0)
            break;
        const int spend = std::min(perGenreBudget, remaining);
        const auto ids = m_candidateFinder.discover(genre, spend, m_repo, m_candidateListTtlSeconds);
        discovered << ids;
        remaining -= spend;
    }
    discovered.removeDuplicates();
    return discovered;
}

std::optional<TrailerCandidate> GogTrailerSource::fetchDetails(const QString& nativeId)
{
    TrailerCandidate candidate = m_candidateFinder.pendingMetadata(nativeId);
    if (candidate.nativeId.isEmpty()) {
        // Not discovered by this instance this run (e.g. called out of the
        // normal discoverCandidates()-then-fetchDetails() sequence) — still
        // produce a minimal, valid candidate rather than failing outright;
        // it just won't carry genre/developer/age info.
        candidate.sourceId = id();
        candidate.nativeId = nativeId;
        logWarning(QStringLiteral("gog: fetchDetails(%1) called with no pending metadata from discovery — "
                                   "genre/age/developer will be empty for this candidate").arg(nativeId));
    }

    QUrl url(QStringLiteral("https://api.gog.com/products/%1").arg(nativeId));
    QUrlQuery query;
    query.addQueryItem("expand", "videos");
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("SteamTrailerScreensaver/1.0"));

    std::unique_ptr<QNetworkReply> reply(m_networkManager.get(request));
    if (!awaitReply(reply.get()) || reply->error() != QNetworkReply::NoError) {
        logWarning(QStringLiteral("gog: product detail request for %1 failed").arg(nativeId));
        candidate.needsFallbackResolution = true;
        return candidate;
    }

    const auto root = QJsonDocument::fromJson(reply->readAll()).object();
    candidate.renditions = parseVideos(root.value("videos").toArray());
    candidate.needsFallbackResolution = candidate.renditions.isEmpty();

    return candidate;
}

std::optional<TrailerRendition> GogTrailerSource::resolveFallback(const TrailerCandidate& candidate)
{
    if (const auto cachedId = m_repo.fallbackVideoId(id(), candidate.nativeId))
        return YoutubeFallbackResolver::renditionForVideoId(*cachedId);

    QString videoId, queryUsed;
    const auto rendition = m_youtubeFallback.resolve(candidate.title, candidate.developer, &videoId, &queryUsed);
    if (rendition)
        m_repo.cacheFallbackVideoId(id(), candidate.nativeId, queryUsed, videoId, QDateTime::currentSecsSinceEpoch());
    return rendition;
}

} // namespace ssv
