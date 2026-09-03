#pragma once

#include "cache/CacheRepository.h"
#include "config/Config.h"
#include "sources/SourceRegistry.h"
#include "sources/SourceTypes.h"

#include <QList>
#include <QStringList>
#include <optional>

namespace ssv {

// Source-agnostic orchestrator sitting between SourceRegistry and
// PlaylistEngine. Fills the local cache from whichever enabled sources need
// it (spending at most a per-run request budget — see
// docs/ARCHITECTURE.md's "idle-footprint invariant"), and resolves a
// playable rendition for a chosen candidate, invoking a source's fallback
// path (e.g. Steam's YouTube search) only when actually needed for
// playback, not speculatively for the whole pool.
class TrailerResolver {
public:
    TrailerResolver(SourceRegistry& registry, CacheRepository& repo, AdvancedConfig advanced);

    // Tops up the cache for each enabled source (spending at most
    // advanced.maxCatalogRequestsPerRun requests total, split across
    // sources) only when its cached pool is thin, then returns the merged,
    // still-fresh cached pool across all enabled sources for PlaylistEngine
    // to filter and order.
    QList<TrailerCandidate> preparePool(const QStringList& enabledSourceIds, const GenreFilter& filter);

    // Ensures `candidate` has a playable rendition: returns its own first
    // rendition if it has one, otherwise asks its source to resolve a
    // fallback (caching the result) and updates the cached row so future
    // runs don't repeat the fallback lookup. Returns std::nullopt if
    // nothing playable could be found — callers must skip to the next
    // candidate rather than treat this as fatal.
    std::optional<TrailerRendition> ensurePlayable(TrailerCandidate& candidate);

private:
    SourceRegistry& m_registry;
    CacheRepository& m_repo;
    AdvancedConfig m_advanced;

    // Below this, a source gets a full third of its per-run budget for
    // discovery (the rest for detail fetches) — fast early growth. 150 is
    // comfortably larger than any single shuffled playlist would visibly
    // cycle through in one screensaver session.
    static constexpr int kMinPoolSizeBeforeDiscovery = 150;

    // Once a source's pool is past kMinPoolSizeBeforeDiscovery, it still
    // gets this much of its budget for discovery every run rather than
    // zero — without this, discovery hard-stops entirely for the full
    // cacheTtlDaysAppDetails staleness window (weeks) the moment the
    // threshold is first reached, freezing the catalog at whatever was
    // discovered early (worse still when GenreFilter::preferPopular biases
    // that early discovery toward the same handful of top-played titles).
    // This trickle keeps the catalog slowly growing forever instead.
    static constexpr int kTrickleDiscoveryBudget = 3;
};

} // namespace ssv
