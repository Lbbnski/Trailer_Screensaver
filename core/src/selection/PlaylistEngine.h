#pragma once

#include "cache/CacheRepository.h"
#include "config/Config.h"
#include "sources/SourceTypes.h"

#include <QList>

namespace ssv {

// Applies the user's genre/age/content-descriptor filter to a pool of
// TrailerCandidate objects (already normalized to the canonical taxonomy by
// whichever IMetadataSource produced them) and orders the result into a
// playlist, skipping anything played too recently. Knows nothing about
// Steam or any other specific source.
class PlaylistEngine {
public:
    explicit PlaylistEngine(CacheRepository& repo);

    // True if `candidate` satisfies `filter`'s genre mode/list, age cutoff,
    // and blocked content descriptors.
    bool passesFilter(const TrailerCandidate& candidate, const FilterConfig& filter) const;

    // Filters `pool` through passesFilter(), drops anything played within
    // the last `noRepeatWindowSeconds`, and returns the rest in shuffled
    // order.
    QList<TrailerCandidate> buildPlaylist(const QList<TrailerCandidate>& pool, const FilterConfig& filter,
                                           qint64 noRepeatWindowSeconds = 3600) const;

private:
    CacheRepository& m_repo;

    // When GenreFilter::preferPopular is set, a candidate discovered via one
    // of its source's popularity-sorted passes (TrailerCandidate::
    // discoveredAsPopular) is entered into the pre-shuffle list this many
    // times instead of once, giving it roughly this much better odds of
    // coming up in any given loop through the playlist. Deliberately a mild
    // multiplier, not an allow-list restricted to popular titles only:
    // "prefer" means biased, not exclusive, and every entry still only ever
    // gets shuffled uniformly among the (weighted) pool, so this can't
    // reintroduce the identical-order-every-loop problem
    // wrapPlaylistIndexIfNeeded() fixes — it only changes relative odds.
    static constexpr int kPopularBoostFactor = 3;
};

} // namespace ssv
