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
};

} // namespace ssv
