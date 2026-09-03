#pragma once

#include "cache/CacheRepository.h"
#include "sources/SourceTypes.h"

#include <QHash>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;

namespace ssv {

// Discovers GOG product ids plausibly matching a canonical genre via
// embed.gog.com/games/ajax/filtered — GOG's public storefront listing
// endpoint, no API key required. Mirrors SteamGenreCandidateFinder's
// resumable-pagination approach (state persisted via the existing
// candidate_pages/genre_candidates tables under source_id "gog" — no
// schema change needed).
//
// Unlike Steam/IGDB, a listing page response already carries genres,
// developer, and ageLimit for every item — GOG's per-id product endpoint
// (api.gog.com/products/{id}) only adds trailer videos on top, it doesn't
// repeat this metadata at all. So this class also stashes each discovered
// item's non-video metadata in an in-memory map, keyed by native id, which
// GogTrailerSource::fetchDetails() consumes to avoid a second metadata
// fetch. This is safe because TrailerResolver::preparePool() always calls
// fetchDetails() for a newly-discovered id within the same call that
// produced it (same process, same GogTrailerSource instance) — see
// GogTrailerSource's header comment.
class GogCandidateFinder {
public:
    explicit GogCandidateFinder(QNetworkAccessManager& networkManager);

    QStringList discover(const QString& canonicalGenre, int requestBudget,
                          CacheRepository& repo, qint64 candidateListTtlSeconds);

    // Seeds from GOG's own "popularity" sort order, no genre filter — used
    // for GenreFilter::preferPopular, cached under the same "__popular__"
    // pseudo-genre key the other sources' topPlayed() use.
    QStringList topPlayed(int requestBudget, CacheRepository& repo, qint64 candidateListTtlSeconds);

    // Metadata (everything but videos) for a native id discovered by the
    // most recent discover()/topPlayed() call on this instance. Empty
    // (default-constructed) TrailerCandidate if the id wasn't just
    // discovered by this instance.
    TrailerCandidate pendingMetadata(const QString& nativeId) const;

private:
    QStringList fetchPage(const QString& categoryParam, const QString& sort, int page, bool* hasMore);

    QNetworkAccessManager& m_networkManager;
    QHash<QString, TrailerCandidate> m_pendingMetadata;
};

} // namespace ssv
