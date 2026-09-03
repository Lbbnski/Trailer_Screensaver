#include "sources/igdb/IgdbCandidateFinder.h"
#include "sources/igdb/IgdbClient.h"
#include "sources/igdb/IgdbGenreMap.h"
#include "util/Logging.h"

#include <QDateTime>
#include <QJsonObject>

namespace ssv {

namespace {
constexpr int kSearchPageSize = 200; // IGDB allows up to 500 per request; keep modest given this runs per genre, per short-lived process activation
const QString kSourceId = QStringLiteral("igdb");
const QString kPopularPseudoGenre = QStringLiteral("__popular__"); // matches SteamGenreCandidateFinder's convention
}

IgdbCandidateFinder::IgdbCandidateFinder(IgdbClient& client) : m_client(client) {}

QStringList IgdbCandidateFinder::searchPage(const QString& whereClause, int offset, int limit, bool* hasMore)
{
    *hasMore = false;

    const QString body = QStringLiteral("fields id; where %1; sort total_rating_count desc; offset %2; limit %3;")
                              .arg(whereClause).arg(offset).arg(limit);
    const auto results = m_client.query(QStringLiteral("games"), body);

    QStringList ids;
    for (const auto& v : results) {
        const auto id = v.toObject().value("id").toVariant().toLongLong();
        if (id > 0)
            ids << QString::number(id);
    }

    *hasMore = ids.size() >= limit;
    return ids;
}

QStringList IgdbCandidateFinder::discover(const QString& canonicalGenre, int requestBudget,
                                           CacheRepository& repo, qint64 candidateListTtlSeconds)
{
    if (requestBudget <= 0)
        return {};

    const auto filter = IgdbGenreMap::apicalypseFilterFor(canonicalGenre);
    if (!filter) {
        logInfo(QStringLiteral("igdb: no genre/theme/mode equivalent for \"%1\" — nothing to discover").arg(canonicalGenre));
        return {};
    }

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    auto state = repo.candidatePageState(kSourceId, canonicalGenre);
    if (state.exhausted && (now - state.lastRefreshedAt) <= candidateListTtlSeconds)
        return {}; // fully paged through recently — caller falls back to cache

    QStringList discovered;
    while (requestBudget > 0) {
        bool hasMore = false;
        const auto pageIds = searchPage(*filter, state.lastSearchStart, kSearchPageSize, &hasMore);
        --requestBudget;
        if (pageIds.isEmpty())
            break;

        repo.addGenreCandidates(kSourceId, canonicalGenre, pageIds, now);
        discovered << pageIds;
        state.lastSearchStart += kSearchPageSize;
        state.exhausted = !hasMore;
        if (!hasMore)
            break;
    }

    state.lastRefreshedAt = now;
    repo.setCandidatePageState(kSourceId, canonicalGenre, state);
    discovered.removeDuplicates();
    return discovered;
}

QStringList IgdbCandidateFinder::topPlayed(int requestBudget, CacheRepository& repo, qint64 candidateListTtlSeconds)
{
    if (requestBudget <= 0)
        return {};

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    auto state = repo.candidatePageState(kSourceId, kPopularPseudoGenre);
    if ((now - state.lastRefreshedAt) <= candidateListTtlSeconds)
        return {};

    bool hasMore = false;
    const auto ids = searchPage(QStringLiteral("total_rating_count > 0"), 0, kSearchPageSize, &hasMore);

    if (!ids.isEmpty())
        repo.addGenreCandidates(kSourceId, kPopularPseudoGenre, ids, now);

    state.lastRefreshedAt = now;
    repo.setCandidatePageState(kSourceId, kPopularPseudoGenre, state);
    return ids;
}

} // namespace ssv
