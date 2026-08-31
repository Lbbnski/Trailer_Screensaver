#include "trailer/TrailerResolver.h"
#include "util/Logging.h"

#include <QDateTime>

#include <algorithm>

namespace ssv {

TrailerResolver::TrailerResolver(SourceRegistry& registry, CacheRepository& repo, AdvancedConfig advanced)
    : m_registry(registry)
    , m_repo(repo)
    , m_advanced(std::move(advanced))
{
}

QList<TrailerCandidate> TrailerResolver::preparePool(const QStringList& enabledSourceIds, const GenreFilter& filter)
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const qint64 staleAfterSeconds = qint64(m_advanced.cacheTtlDaysAppDetails) * 24 * 3600;

    QList<TrailerCandidate> pool;
    if (enabledSourceIds.isEmpty())
        return pool;

    const int budgetPerSource = std::max(1, m_advanced.maxCatalogRequestsPerRun / static_cast<int>(enabledSourceIds.size()));

    for (const auto& sourceId : enabledSourceIds) {
        auto* source = m_registry.find(sourceId);
        if (!source) {
            logWarning(QStringLiteral("config lists unknown source \"%1\" — skipping").arg(sourceId));
            continue;
        }

        auto fresh = m_repo.freshAppDetails(sourceId, now);

        // Only spend network budget on this source when the cache can't
        // already fill a playlist — cache-only runs start playback
        // immediately, breadth grows opportunistically on the runs where
        // it's needed. This is what makes the "no idle daemon" model work:
        // state persists in SQLite between short-lived process runs.
        if (fresh.size() < kMinPoolSizeBeforeDiscovery) {
            // Reserve roughly a third of this source's budget for
            // discovery queries, the rest for per-id detail fetches.
            const int discoveryBudget = std::max(1, budgetPerSource / 3);
            const auto newIds = source->discoverCandidates(filter, discoveryBudget);
            logInfo(QStringLiteral("preparePool[%1]: fresh=%2 discovered=%3 detailBudget=%4")
                        .arg(sourceId).arg(fresh.size()).arg(newIds.size()).arg(budgetPerSource - discoveryBudget));

            int detailBudget = budgetPerSource - discoveryBudget;
            for (const auto& nativeId : newIds) {
                if (detailBudget <= 0)
                    break;
                if (m_repo.hasFreshDetails(sourceId, nativeId, now))
                    continue; // already have it, don't spend a request re-fetching

                const auto details = source->fetchDetails(nativeId);
                --detailBudget;
                if (!details)
                    continue; // removed app / request failure — skip, not fatal

                const bool hasTrailer = !details->renditions.isEmpty();
                m_repo.upsertAppDetails(*details, hasTrailer, now, now + staleAfterSeconds);
                fresh.append(*details);
            }
        }

        pool << fresh;
    }

    return pool;
}

std::optional<TrailerRendition> TrailerResolver::ensurePlayable(TrailerCandidate& candidate)
{
    if (!candidate.renditions.isEmpty())
        return candidate.renditions.first();

    auto* source = m_registry.find(candidate.sourceId);
    if (!source)
        return std::nullopt;

    const auto fallback = source->resolveFallback(candidate);
    if (!fallback)
        return std::nullopt;

    candidate.renditions.append(*fallback);
    candidate.needsFallbackResolution = false;

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const qint64 staleAfterSeconds = 21LL * 24 * 3600; // matches default cacheTtlDaysAppDetails; caller-provided TTL not needed for this incremental update
    m_repo.upsertAppDetails(candidate, /*hasTrailer=*/true, now, now + staleAfterSeconds);

    return fallback;
}

} // namespace ssv
