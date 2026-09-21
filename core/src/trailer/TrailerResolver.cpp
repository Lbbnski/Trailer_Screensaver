#include "trailer/TrailerResolver.h"
#include "sources/steam/YoutubeFallbackResolver.h"
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
            }
        }

        // Re-read after discovery instead of appending to what was loaded
        // before it: discovery can change already-cached rows (upcoming
        // discovery flags them via CacheRepository::markComingSoon), and the
        // pre-discovery copy would hand PlaylistEngine stale flags for this
        // whole run — e.g. an "only upcoming games" playlist that comes up
        // empty on the very run that just found the upcoming games.
        pool << m_repo.freshAppDetails(sourceId, now);
    }

    return pool;
}

std::optional<TrailerRendition> TrailerResolver::ensurePlayable(TrailerCandidate& candidate)
{
    // A video the user reported as not a game trailer never plays again, no
    // matter which source it came from (a curated IGDB/GOG video id, a
    // cached fallback match, ...) — drop it, and if that leaves nothing,
    // fall through to a fresh fallback resolution below (which itself skips
    // rejected videos).
    const auto beforeCount = candidate.renditions.size();
    candidate.renditions.removeIf([this](const TrailerRendition& r) {
        return m_repo.isVideoRejected(YoutubeFallbackResolver::videoIdFromUrl(r.url));
    });
    if (candidate.renditions.size() != beforeCount)
        candidate.needsFallbackResolution = candidate.renditions.isEmpty();

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

    // Only the in-memory candidate gets the resolved video — deliberately
    // NOT written back into `apps`. It used to be, and a candidate loaded
    // from `apps` then arrived with a rendition already attached and never
    // consulted fallback_trailers again, which is where the
    // resolver_version check (and the user's rejected-video list) lives:
    // a stale or reported match kept playing straight out of `apps` for the
    // whole cache TTL. fallback_trailers is the single source of truth for
    // a fallback video; resolving it again is one cheap DB lookup.
    candidate.renditions.append(*fallback);
    candidate.needsFallbackResolution = false;

    return fallback;
}

} // namespace ssv
