#pragma once

#include "cache/CacheRepository.h"

#include <QSet>
#include <QString>
#include <QStringList>

namespace ssv {

class IgdbClient;

// Discovers IGDB game ids plausibly matching a canonical genre, mirroring
// SteamGenreCandidateFinder's role and its resumable-pagination approach
// (state persisted via the same candidate_pages/genre_candidates tables,
// under source_id "igdb" — no schema change needed).
class IgdbCandidateFinder {
public:
    explicit IgdbCandidateFinder(IgdbClient& client);

    // Discovers up to `requestBudget` outbound-request's worth of new
    // candidates for `canonicalGenre`, sorted by IGDB's own popularity
    // proxy (total_rating_count). Returns an empty list if IGDB has no
    // vocabulary equivalent for this genre at all (see
    // IgdbGenreMap::apicalypseFilterFor) or the budget is 0.
    QStringList discover(const QString& canonicalGenre, int requestBudget,
                          CacheRepository& repo, qint64 candidateListTtlSeconds);

    // Seeds from IGDB's overall highest total_rating_count games, no genre
    // filter — used for GenreFilter::preferPopular, cached under the same
    // "__popular__" pseudo-genre key SteamGenreCandidateFinder::topPlayed()
    // uses (distinct source_id keeps the two sources' rows from colliding).
    QStringList topPlayed(int requestBudget, CacheRepository& repo, qint64 candidateListTtlSeconds);

    // True if `nativeId` was returned by topPlayed() during the most recent
    // call on this instance — IgdbTrailerSource::fetchDetails() merges this
    // into TrailerCandidate::discoveredAsPopular, mirroring
    // SteamGenreCandidateFinder::isPopularHint().
    bool isPopularHint(const QString& nativeId) const;

    // Already-released games from roughly the last 90 days, ranked by rating
    // count as a "currently being talked about" proxy — something
    // genre-based discovery structurally can't surface (a brand-new game has
    // few ratings yet). Cached under the pseudo-genre "__recent__", same
    // plumbing as topPlayed()'s "__popular__".
    QStringList newAndTrending(int requestBudget, CacheRepository& repo, qint64 candidateListTtlSeconds);

    // Unreleased games ranked by IGDB's own community-anticipation signal
    // ("hypes" — the same sort its "Most Anticipated" page uses;
    // total_rating_count, topPlayed()'s sort, is useless for an unreleased
    // game with no ratings yet). One request re-lists it at most once per
    // TTL window; recorded under "__upcoming__", flagged in the cache via
    // CacheRepository::markComingSoon, and drained a few ids per run (see
    // SteamGenreCandidateFinder::upcoming for why).
    QStringList upcoming(int requestBudget, int maxIds, CacheRepository& repo, qint64 candidateListTtlSeconds);

private:
    QStringList searchPage(const QString& whereClause, int offset, int limit, bool* hasMore);
    QStringList searchPageSorted(const QString& whereClause, const QString& sortField,
                                  int offset, int limit, bool* hasMore);

    IgdbClient& m_client;
    QSet<QString> m_popularHints;
};

} // namespace ssv
