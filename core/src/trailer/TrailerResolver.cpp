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

        // Spend network budget on this source's discovery every run, not
        // just while its cache is thin — a thin pool gets roughly a third
        // of the budget (fast early growth); a healthy one still gets a
        // small trickle rather than zero, so the catalog keeps growing
        // instead of freezing at whatever was discovered early (see
        // kTrickleDiscoveryBudget's comment). This is still bounded and
        // still respects the "no idle daemon" model — it only ever spends
        // budget inside a call made during an already-running screensaver
        // session, same as before.
        const bool poolIsThin = fresh.size() < kMinPoolSizeBeforeDiscovery;
        const int discoveryBudget = poolIsThin
            ? std::max(1, budgetPerSource / 3)
            : std::min(budgetPerSource, kTrickleDiscoveryBudget);

        if (discoveryBudget > 0) {
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

    const qint64 now = QDateTime::currentSecsSinceEpoch();

    // Already know this one's dead as of a recent attempt — skip straight
    // to "unplayable" instead of re-running a (usually multi-second, yt-dlp
    // subprocess-backed) resolution that's very likely to fail the same way
    // again. PlaylistEngine::buildPlaylist should normally have already
    // excluded this candidate for the same reason; this check stays as a
    // second line of defense for a candidate resolved outside that path.
    if (m_repo.fallbackRecentlyFailed(candidate.sourceId, candidate.nativeId, now))
        return std::nullopt;

    const auto fallback = source->resolveFallback(candidate);
    if (!fallback) {
        m_repo.recordFallbackFailure(candidate.sourceId, candidate.nativeId, now, now + kFallbackRetryAfterSeconds);
        return std::nullopt;
    }

    candidate.renditions.append(*fallback);
    candidate.needsFallbackResolution = false;

    const qint64 staleAfterSeconds = 21LL * 24 * 3600; // matches default cacheTtlDaysAppDetails; caller-provided TTL not needed for this incremental update
    m_repo.upsertAppDetails(candidate, /*hasTrailer=*/true, now, now + staleAfterSeconds);

    return fallback;
}

} // namespace ssv
