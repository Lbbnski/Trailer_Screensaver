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
    //
    // Version history: 1 = movie/TV category filter; 2 = TrailerHeuristics
    // (trailer-word requirement, non-trailer/movie word lists, strict
    // matching for short titles, film/TV description signals) — added after
    // a user's bad-trailer reports showed a Halo Infinite commentary video
    // and same-named movie trailers getting through.
    static constexpr int kCurrentFallbackResolverVersion = 2;

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

    // --- playback_history: also doubles as the user-facing "what have I
    // seen" list (see qtui/src/PlaybackHistoryDialog.h) ---
    void recordPlayback(const QString& sourceId, const QString& nativeId,
                         const QString& title, const QString& developer, const QString& storeUrl,
                         const QString& videoUrl, qint64 playedAtEpoch);
    bool playedSince(const QString& sourceId, const QString& nativeId, qint64 sinceEpoch) const;

    // Not called anywhere today — playback_history is left to grow
    // unbounded, which is what makes it usable as a full "everything I've
    // ever seen" history rather than just a short no-repeat window. Kept
    // available for a future cap if the table's size ever actually becomes
    // a problem for a real user's cache.
    void pruneHistoryOlderThan(qint64 cutoffEpoch);

    // One played trailer, as shown in the playback-history view. Snapshotted
    // at play time (see recordPlayback) rather than joined against `apps`
    // live, so it stays meaningful even after that row goes stale/changes.
    struct PlaybackHistoryEntry {
        QString sourceId;
        QString nativeId;
        QString title;
        QString developer;
        QString storeUrl;
        QString videoUrl;
        qint64 playedAt = 0;
        bool blocked = false;
    };

    // Most recent plays first, capped at `limit` — this is a full audit
    // trail (playback_history is never pruned automatically; see
    // pruneHistoryOlderThan's own doc), so a generous limit keeps
    // effectively everything reachable from the UI rather than needing a
    // separate "manage blocked games" screen.
    QList<PlaybackHistoryEntry> recentPlaybackHistory(int limit) const;

    // --- blocked_games: games the user never wants offered again ---
    void blockGame(const QString& sourceId, const QString& nativeId, qint64 blockedAtEpoch);
    void unblockGame(const QString& sourceId, const QString& nativeId);
    bool isGameBlocked(const QString& sourceId, const QString& nativeId) const;

    // --- rejected_videos: YouTube videos the user reported as not a game
    // trailer. Unlike blockGame() (which drops a whole game), this only bans
    // one video — the game itself stays eligible and simply re-resolves to a
    // different one. fallbackVideoId() ignores a cached match that's been
    // rejected, and TrailerResolver::ensurePlayable() drops a rejected
    // rendition from any source, not just fallback-resolved ones.
    void rejectVideo(const QString& videoId, qint64 rejectedAtEpoch);
    bool isVideoRejected(const QString& videoId) const;

    // The YouTube search query that produced a candidate's cached fallback
    // video, if it was resolved that way — std::nullopt for a candidate
    // with its own curated trailer, or one never fallback-resolved at all.
    // Included in a reported-trailer record (see PlaybackHistoryDialog) as
    // context for why a search might have landed on the wrong video.
    std::optional<QString> fallbackQueryUsed(const QString& sourceId, const QString& nativeId) const;

private:
    CacheDatabase& m_db;
};

} // namespace ssv
