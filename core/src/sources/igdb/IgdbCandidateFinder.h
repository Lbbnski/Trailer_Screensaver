#pragma once

#include "cache/CacheRepository.h"

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

private:
    QStringList searchPage(const QString& whereClause, int offset, int limit, bool* hasMore);

    IgdbClient& m_client;
};

} // namespace ssv
