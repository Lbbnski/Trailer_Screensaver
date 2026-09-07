#pragma once

#include "cache/CacheDatabase.h"
#include "sources/SourceTypes.h"

#include <QString>
#include <QStringList>
#include <optional>

namespace ssv {

// Typed CRUD over CacheDatabase. Every method takes/scopes by source_id so
// this stays correct once a second IMetadataSource is registered — nothing
// here assumes Steam.
class CacheRepository {
public:
    explicit CacheRepository(CacheDatabase& db);

    // Bump this whenever YoutubeFallbackResolver::resolve()'s matching or
    // filtering logic changes in a way that could change which video gets
    // picked for the same query (e.g. adding the movie/TV-category filter
    // did). fallbackVideoId() and fallbackRecentlyFailed() only trust a
    // cached row whose resolver_version is at least this value — every
    // pre-existing row defaults to resolver_version 0 via the schema
    // migration (see CacheDatabase::migrate), so bumping this constant
    // automatically makes every previously-cached match (right or wrong)
    // and every previously-recorded failure eligible for re-evaluation
    // under the new logic on its next use, instead of a filtering fix
    // silently having zero effect on an already-populated cache forever —
    // exactly what happened before this existed: every fallback video id in
    // a real cache had been resolved before the category filter was added,
    // so it never got a chance to reject any of them.
    static constexpr int kCurrentFallbackResolverVersion = 1;

    // --- apps: cached TrailerCandidate details ---
    void upsertAppDetails(const TrailerCandidate& candidate, bool hasTrailer,
                           qint64 fetchedAtEpoch, qint64 staleAfterEpoch);
    std::optional<TrailerCandidate> getAppDetails(const QString& sourceId, const QString& nativeId) const;
    bool hasFreshDetails(const QString& sourceId, const QString& nativeId, qint64 nowEpoch) const;

    // All cached candidates for a source that are not stale as of nowEpoch.
    // PlaylistEngine applies genre/age filtering on top of this — the cache
    // layer doesn't know about the user's filter settings.
    QList<TrailerCandidate> freshAppDetails(const QString& sourceId, qint64 nowEpoch) const;

    // --- genre_candidates: which native ids discovery found for a genre ---
    void addGenreCandidates(const QString& sourceId, const QString& genre,
                             const QStringList& nativeIds, qint64 discoveredAtEpoch);
    QStringList genreCandidates(const QString& sourceId, const QString& genre) const;

    // --- candidate_pages: resumable discovery pagination across runs ---
    struct PageState {
        int lastSearchStart = 0;
        bool exhausted = false;
        qint64 lastRefreshedAt = 0;
    };
    PageState candidatePageState(const QString& sourceId, const QString& genre) const;
    void setCandidatePageState(const QString& sourceId, const QString& genre, const PageState& state);

    // --- fallback_trailers: resolved YouTube (or other) fallback video ids ---
    void cacheFallbackVideoId(const QString& sourceId, const QString& nativeId,
                               const QString& queryUsed, const QString& videoId, qint64 resolvedAtEpoch);
    std::optional<QString> fallbackVideoId(const QString& sourceId, const QString& nativeId) const;

    // Records that a fallback resolution attempt (e.g. YoutubeFallbackResolver::resolve())
    // found nothing trustworthy, and shouldn't be retried until retryAfterEpoch.
    // Never overwrites an already-successful cached video id for the same
    // (sourceId, nativeId) — see the ON CONFLICT guard in the .cpp.
    void recordFallbackFailure(const QString& sourceId, const QString& nativeId,
                                qint64 failedAtEpoch, qint64 retryAfterEpoch);

    // True if the most recent fallback attempt for this candidate failed and
    // its retry-after window hasn't elapsed yet — callers should treat this
    // the same as "no way to play this right now" without re-attempting
    // resolution (see TrailerResolver::ensurePlayable / PlaylistEngine::buildPlaylist).
    bool fallbackRecentlyFailed(const QString& sourceId, const QString& nativeId, qint64 nowEpoch) const;

    // --- playback_history: small ring buffer to avoid immediate repeats ---
    void recordPlayback(const QString& sourceId, const QString& nativeId, qint64 playedAtEpoch);
    bool playedSince(const QString& sourceId, const QString& nativeId, qint64 sinceEpoch) const;
    void pruneHistoryOlderThan(qint64 cutoffEpoch);

private:
    CacheDatabase& m_db;
};

} // namespace ssv
