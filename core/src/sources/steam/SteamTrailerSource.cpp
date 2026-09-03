#include "sources/steam/SteamTrailerSource.h"
#include "sources/GenreTaxonomy.h"
#include "sources/steam/SteamAgeGate.h"

#include <QDateTime>
#include <QNetworkAccessManager>

#include <algorithm>

namespace ssv {

SteamTrailerSource::SteamTrailerSource(QNetworkAccessManager& networkManager, CacheRepository& repo,
                                        QString language, QString countryCode,
                                        YoutubeFallbackResolver& youtubeFallback,
                                        qint64 candidateListTtlSeconds)
    : m_repo(repo)
    , m_detailsClient(networkManager, std::move(language), std::move(countryCode))
    , m_candidateFinder(networkManager)
    , m_youtubeFallback(youtubeFallback)
    , m_candidateListTtlSeconds(candidateListTtlSeconds)
{
    SteamAgeGate::apply(networkManager);
}

QString SteamTrailerSource::id() const
{
    return QStringLiteral("steam");
}

QList<QString> SteamTrailerSource::discoverCandidates(const GenreFilter& filter, int requestBudget)
{
    // Steam's search only supports positive genre filtering, so a
    // block-list is realized as "every canonical genre except the blocked
    // ones" rather than a single "not X" query the API doesn't offer.
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

    // Popular titles go first: TrailerResolver::preparePool() detail-fetches
    // newIds in the order discoverCandidates() returns them, so putting
    // these first is what actually makes "prefer popular" bias which apps
    // get their details (and thus a shot at being played) fetched.
    if (filter.preferPopular && remaining > 0) {
        const int spend = std::min(2, remaining); // top100in2weeks + top100forever
        const auto popularIds = m_candidateFinder.topPlayed(spend, m_repo, m_candidateListTtlSeconds);
        discovered << popularIds;
        remaining -= spend;
    }

    if (remaining <= 0) {
        discovered.removeDuplicates();
        return discovered;
    }

    // Spread the per-run request budget across the genres being searched
    // this call, at least one request per genre touched.
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

std::optional<TrailerCandidate> SteamTrailerSource::fetchDetails(const QString& nativeId)
{
    return m_detailsClient.fetchDetails(nativeId);
}

std::optional<TrailerRendition> SteamTrailerSource::resolveFallback(const TrailerCandidate& candidate)
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
