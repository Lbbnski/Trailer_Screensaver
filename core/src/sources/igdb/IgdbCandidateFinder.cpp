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
const QString kRecentPseudoGenre = QStringLiteral("__recent__");
const QString kUpcomingPseudoGenre = QStringLiteral("__upcoming__");
}

IgdbCandidateFinder::IgdbCandidateFinder(IgdbClient& client) : m_client(client) {}

QStringList IgdbCandidateFinder::searchPageSorted(const QString& whereClause, const QString& sortField,
                                                   int offset, int limit, bool* hasMore)
{
    *hasMore = false;

    const QString body = QStringLiteral("fields id; where %1; sort %2 desc; offset %3; limit %4;")
                              .arg(whereClause, sortField).arg(offset).arg(limit);
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

QStringList IgdbCandidateFinder::searchPage(const QString& whereClause, int offset, int limit, bool* hasMore)
{
    return searchPageSorted(whereClause, QStringLiteral("total_rating_count"), offset, limit, hasMore);
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

    if (!ids.isEmpty()) {
        repo.addGenreCandidates(kSourceId, kPopularPseudoGenre, ids, now);
        for (const auto& id : ids)
            m_popularHints.insert(id);
    }

    state.lastRefreshedAt = now;
    repo.setCandidatePageState(kSourceId, kPopularPseudoGenre, state);
    return ids;
}

bool IgdbCandidateFinder::isPopularHint(const QString& nativeId) const
{
    return m_popularHints.contains(nativeId);
}

QStringList IgdbCandidateFinder::newAndTrending(int requestBudget, CacheRepository& repo, qint64 candidateListTtlSeconds)
{
    if (requestBudget <= 0)
        return {};

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    auto state = repo.candidatePageState(kSourceId, kRecentPseudoGenre);
    if ((now - state.lastRefreshedAt) <= candidateListTtlSeconds)
        return {};

    // Already-released games from roughly the last 90 days, ranked by rating
    // count as a "currently being talked about" proxy. (Unreleased games have
    // their own list — see upcoming().)
    bool hasMore = false;
    const qint64 ninetyDaysAgo = now - 90LL * 24 * 3600;
    QStringList discovered = searchPage(
        QStringLiteral("first_release_date <= %1 & first_release_date > %2").arg(now).arg(ninetyDaysAgo),
        0, kSearchPageSize, &hasMore);
    discovered.removeDuplicates();

    if (!discovered.isEmpty())
        repo.addGenreCandidates(kSourceId, kRecentPseudoGenre, discovered, now);

    state.lastRefreshedAt = now;
    repo.setCandidatePageState(kSourceId, kRecentPseudoGenre, state);
    return discovered;
}

QStringList IgdbCandidateFinder::upcoming(int requestBudget, int maxIds, CacheRepository& repo, qint64 candidateListTtlSeconds)
{
    if (requestBudget <= 0 || maxIds <= 0)
        return {};

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    auto state = repo.candidatePageState(kSourceId, kUpcomingPseudoGenre);

    // One request re-lists it, at most once per TTL window.
    if ((now - state.lastRefreshedAt) > candidateListTtlSeconds) {
        bool hasMore = false;
        // Ranked by IGDB's community-anticipation signal ("hypes" — the same
        // sort its own "Most Anticipated" page uses); total_rating_count is
        // useless for an unreleased game, which has no ratings yet.
        QStringList listed = searchPageSorted(QStringLiteral("first_release_date > %1 & hypes > 0").arg(now),
                                               QStringLiteral("hypes"), 0, kSearchPageSize, &hasMore);
        listed.removeDuplicates();

        if (!listed.isEmpty()) {
            repo.addGenreCandidates(kSourceId, kUpcomingPseudoGenre, listed, now);
            repo.markComingSoon(kSourceId, listed);
            state.lastRefreshedAt = now;
            repo.setCandidatePageState(kSourceId, kUpcomingPseudoGenre, state);
        }
    }

    return repo.unfetchedCandidates(kSourceId, kUpcomingPseudoGenre, now, maxIds);
}

} // namespace ssv
