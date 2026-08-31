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

    // --- playback_history: small ring buffer to avoid immediate repeats ---
    void recordPlayback(const QString& sourceId, const QString& nativeId, qint64 playedAtEpoch);
    bool playedSince(const QString& sourceId, const QString& nativeId, qint64 sinceEpoch) const;
    void pruneHistoryOlderThan(qint64 cutoffEpoch);

private:
    CacheDatabase& m_db;
};

} // namespace ssv
