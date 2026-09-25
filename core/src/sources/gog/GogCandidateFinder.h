#pragma once

#include "cache/CacheRepository.h"
#include "sources/SourceTypes.h"

#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;

namespace ssv {

// Discovers GOG product ids plausibly matching a canonical genre via
// catalog.gog.com/v1/catalog — GOG's public storefront catalog endpoint,
// no API key required (it replaced the older embed.gog.com listing, which
// can only filter on nine genre facets; the catalog also filters by GOG's
// ~200 tags). Mirrors SteamGenreCandidateFinder's
// resumable-pagination approach (state persisted via the existing
// candidate_pages/genre_candidates tables under source_id "gog" — no
// schema change needed).
//
// Unlike Steam/IGDB, a listing page response already carries genres, tags,
// developer, and age ratings for every item — GOG's per-id product endpoint
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

    // Seeds from GOG's own "trending" sort order, no genre filter — used
    // for GenreFilter::preferPopular, cached under the same "__popular__"
    // pseudo-genre key the other sources' topPlayed() use.
    QStringList topPlayed(int requestBudget, CacheRepository& repo, qint64 candidateListTtlSeconds);

    // Seeds from GOG's own release-date-ordered listing (no genre filter) —
    // verified live to return upcoming/just-listed products first; genre-based
    // discovery on its own structurally favors older, already-established
    // titles, so without this a new/upcoming game essentially never
    // surfaces. Cached under the pseudo-genre "__recent__", same plumbing as
    // topPlayed()'s "__popular__".
    QStringList newAndTrending(int requestBudget, CacheRepository& repo, qint64 candidateListTtlSeconds);

    // Unreleased games: walks the catalog's upcoming-filtered listing (most
    // trending first) up to `requestBudget` pages, records them under
    // "__upcoming__" and flags them in the cache, and returns up to `maxIds`
    // that still lack details.
    // Not TTL-gated — see the .cpp for why each call re-fetches its pages.
    QStringList upcoming(int requestBudget, int maxIds, CacheRepository& repo);

    // Metadata (everything but videos) for a native id discovered by the
    // most recent discover()/topPlayed() call on this instance. Empty
    // (default-constructed) TrailerCandidate if the id wasn't just
    // discovered by this instance.
    TrailerCandidate pendingMetadata(const QString& nativeId) const;

    // Looks a product's listing metadata up by its title (catalog text
    // search), for ids that were discovered in an earlier run — the listing
    // is the only place GOG returns genres/tags/age/developer, and
    // pendingMetadata() only knows what this run's discovery listed.
    // Default-constructed TrailerCandidate if the product isn't found.
    TrailerCandidate lookupMetadata(const QString& nativeId, const QString& title);

private:
    static TrailerCandidate parseProduct(const QJsonObject& item);
    // `filter` is one "param=in:slug" catalog filter, or empty; `order` is a
    // catalog order such as "desc:trending" or "desc:releaseDate".
    QStringList fetchPage(const QString& filter, const QString& order, int page, bool* hasMore);

    QNetworkAccessManager& m_networkManager;
    QHash<QString, TrailerCandidate> m_pendingMetadata;
};

} // namespace ssv
