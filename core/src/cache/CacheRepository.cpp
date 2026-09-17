#include "cache/CacheRepository.h"
#include "util/Logging.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace ssv {

namespace {

QJsonArray toJsonArray(const QStringList& list)
{
    QJsonArray arr;
    for (const auto& s : list) arr.append(s);
    return arr;
}

QStringList fromJsonArray(const QByteArray& json)
{
    QStringList out;
    const auto doc = QJsonDocument::fromJson(json);
    for (const auto& v : doc.array()) out << v.toString();
    return out;
}

QByteArray renditionsToJson(const QList<TrailerRendition>& renditions)
{
    QJsonArray arr;
    for (const auto& r : renditions) {
        arr.append(QJsonObject{
            {"url", r.url},
            {"approxHeight", r.approxHeight},
            {"container", r.container},
        });
    }
    return QJsonDocument(arr).toJson(QJsonDocument::Compact);
}

QList<TrailerRendition> renditionsFromJson(const QByteArray& json)
{
    QList<TrailerRendition> out;
    const auto doc = QJsonDocument::fromJson(json);
    for (const auto& v : doc.array()) {
        const auto obj = v.toObject();
        out.append(TrailerRendition{
            obj.value("url").toString(),
            obj.value("approxHeight").toInt(),
            obj.value("container").toString(),
        });
    }
    return out;
}

TrailerCandidate rowToCandidate(const QSqlQuery& q)
{
    TrailerCandidate c;
    c.sourceId = q.value("source_id").toString();
    c.nativeId = q.value("native_id").toString();
    c.title = q.value("title").toString();
    c.developer = q.value("developer").toString();
    c.canonicalGenres = fromJsonArray(q.value("canonical_genres").toByteArray());
    c.ageRating = q.value("age_rating").toInt();
    c.contentDescriptors = fromJsonArray(q.value("content_descriptors").toByteArray());
    c.renditions = renditionsFromJson(q.value("renditions_json").toByteArray());
    c.needsFallbackResolution = c.renditions.isEmpty();
    c.discoveredAsPopular = q.value("is_popular").toInt() != 0;
    c.storeUrl = q.value("store_url").toString();
    return c;
}

} // namespace

CacheRepository::CacheRepository(CacheDatabase& db) : m_db(db) {}

void CacheRepository::upsertAppDetails(const TrailerCandidate& candidate, bool hasTrailer,
                                        qint64 fetchedAtEpoch, qint64 staleAfterEpoch)
{
    QSqlQuery q(m_db.handle());
    q.prepare(R"(
        INSERT INTO apps (source_id, native_id, title, developer, canonical_genres, age_rating,
                           content_descriptors, renditions_json, has_trailer,
                           details_fetched_at, details_stale_after, is_popular, store_url)
        VALUES (:source_id, :native_id, :title, :developer, :genres, :age,
                :descriptors, :renditions, :has_trailer, :fetched_at, :stale_after, :is_popular, :store_url)
        ON CONFLICT(source_id, native_id) DO UPDATE SET
            title=excluded.title, developer=excluded.developer, canonical_genres=excluded.canonical_genres,
            age_rating=excluded.age_rating, content_descriptors=excluded.content_descriptors,
            renditions_json=excluded.renditions_json, has_trailer=excluded.has_trailer,
            details_fetched_at=excluded.details_fetched_at, details_stale_after=excluded.details_stale_after,
            is_popular=MAX(is_popular, excluded.is_popular), store_url=excluded.store_url
    )");
    q.bindValue(":source_id", candidate.sourceId);
    q.bindValue(":native_id", candidate.nativeId);
    q.bindValue(":title", candidate.title);
    q.bindValue(":developer", candidate.developer);
    q.bindValue(":genres", QJsonDocument(toJsonArray(candidate.canonicalGenres)).toJson(QJsonDocument::Compact));
    q.bindValue(":age", candidate.ageRating);
    q.bindValue(":descriptors", QJsonDocument(toJsonArray(candidate.contentDescriptors)).toJson(QJsonDocument::Compact));
    q.bindValue(":renditions", renditionsToJson(candidate.renditions));
    q.bindValue(":has_trailer", hasTrailer ? 1 : 0);
    q.bindValue(":fetched_at", fetchedAtEpoch);
    q.bindValue(":stale_after", staleAfterEpoch);
    q.bindValue(":is_popular", candidate.discoveredAsPopular ? 1 : 0);
    q.bindValue(":store_url", candidate.storeUrl);

    if (!q.exec())
        logError(QStringLiteral("upsertAppDetails failed: %1").arg(q.lastError().text()));
}

std::optional<TrailerCandidate> CacheRepository::getAppDetails(const QString& sourceId, const QString& nativeId) const
{
    QSqlQuery q(m_db.handle());
    q.prepare("SELECT * FROM apps WHERE source_id = :s AND native_id = :n");
    q.bindValue(":s", sourceId);
    q.bindValue(":n", nativeId);
    if (!q.exec() || !q.next())
        return std::nullopt;
    return rowToCandidate(q);
}

bool CacheRepository::hasFreshDetails(const QString& sourceId, const QString& nativeId, qint64 nowEpoch) const
{
    QSqlQuery q(m_db.handle());
    q.prepare(R"(SELECT 1 FROM apps WHERE source_id = :s AND native_id = :n AND details_stale_after > :now)");
    q.bindValue(":s", sourceId);
    q.bindValue(":n", nativeId);
    q.bindValue(":now", nowEpoch);
    return q.exec() && q.next();
}

QList<TrailerCandidate> CacheRepository::freshAppDetails(const QString& sourceId, qint64 nowEpoch) const
{
    QList<TrailerCandidate> out;
    QSqlQuery q(m_db.handle());
    q.prepare("SELECT * FROM apps WHERE source_id = :s AND details_stale_after > :now");
    q.bindValue(":s", sourceId);
    q.bindValue(":now", nowEpoch);
    if (!q.exec()) {
        logError(QStringLiteral("freshAppDetails query failed: %1").arg(q.lastError().text()));
        return out;
    }
    while (q.next())
        out.append(rowToCandidate(q));
    return out;
}

void CacheRepository::addGenreCandidates(const QString& sourceId, const QString& genre,
                                          const QStringList& nativeIds, qint64 discoveredAtEpoch)
{
    if (nativeIds.isEmpty())
        return;

    // SteamSpy's bulk genre/tag endpoints can return tens of thousands of
    // appids in a single response. Without an explicit transaction, SQLite
    // auto-commits each INSERT individually — observed in practice to run
    // at roughly 900 rows/sec, meaning a single genre's worth of candidates
    // could take minutes to write and make the screensaver look hung
    // before it ever gets to fetch a single trailer. Wrapping the whole
    // batch in one transaction is the standard fix and is easily two
    // orders of magnitude faster.
    QSqlDatabase db = m_db.handle();
    const bool ownTransaction = db.transaction();

    QSqlQuery q(db);
    q.prepare(R"(INSERT OR IGNORE INTO genre_candidates (source_id, genre, native_id, discovered_at)
                 VALUES (:s, :g, :n, :t))");
    for (const auto& nativeId : nativeIds) {
        q.bindValue(":s", sourceId);
        q.bindValue(":g", genre);
        q.bindValue(":n", nativeId);
        q.bindValue(":t", discoveredAtEpoch);
        if (!q.exec())
            logError(QStringLiteral("addGenreCandidates failed: %1").arg(q.lastError().text()));
    }

    if (ownTransaction)
        db.commit();
}

QStringList CacheRepository::genreCandidates(const QString& sourceId, const QString& genre) const
{
    QStringList out;
    QSqlQuery q(m_db.handle());
    q.prepare("SELECT native_id FROM genre_candidates WHERE source_id = :s AND genre = :g");
    q.bindValue(":s", sourceId);
    q.bindValue(":g", genre);
    if (!q.exec())
        return out;
    while (q.next())
        out << q.value(0).toString();
    return out;
}

CacheRepository::PageState CacheRepository::candidatePageState(const QString& sourceId, const QString& genre) const
{
    PageState state;
    QSqlQuery q(m_db.handle());
    q.prepare("SELECT last_search_start, exhausted, last_refreshed_at FROM candidate_pages WHERE source_id = :s AND genre = :g");
    q.bindValue(":s", sourceId);
    q.bindValue(":g", genre);
    if (q.exec() && q.next()) {
        state.lastSearchStart = q.value(0).toInt();
        state.exhausted = q.value(1).toInt() != 0;
        state.lastRefreshedAt = q.value(2).toLongLong();
    }
    return state;
}

void CacheRepository::setCandidatePageState(const QString& sourceId, const QString& genre, const PageState& state)
{
    QSqlQuery q(m_db.handle());
    q.prepare(R"(
        INSERT INTO candidate_pages (source_id, genre, last_search_start, exhausted, last_refreshed_at)
        VALUES (:s, :g, :start, :ex, :t)
        ON CONFLICT(source_id, genre) DO UPDATE SET
            last_search_start=excluded.last_search_start,
            exhausted=excluded.exhausted,
            last_refreshed_at=excluded.last_refreshed_at
    )");
    q.bindValue(":s", sourceId);
    q.bindValue(":g", genre);
    q.bindValue(":start", state.lastSearchStart);
    q.bindValue(":ex", state.exhausted ? 1 : 0);
    q.bindValue(":t", state.lastRefreshedAt);
    if (!q.exec())
        logError(QStringLiteral("setCandidatePageState failed: %1").arg(q.lastError().text()));
}

void CacheRepository::cacheFallbackVideoId(const QString& sourceId, const QString& nativeId,
                                            const QString& queryUsed, const QString& videoId, qint64 resolvedAtEpoch)
{
    QSqlQuery q(m_db.handle());
    q.prepare(R"(
        INSERT INTO fallback_trailers (source_id, native_id, query_used, video_id, resolved_at, resolver_version)
        VALUES (:s, :n, :q, :v, :t, :ver)
        ON CONFLICT(source_id, native_id) DO UPDATE SET
            query_used=excluded.query_used, video_id=excluded.video_id, resolved_at=excluded.resolved_at,
            resolver_version=excluded.resolver_version
    )");
    q.bindValue(":s", sourceId);
    q.bindValue(":n", nativeId);
    q.bindValue(":q", queryUsed);
    q.bindValue(":v", videoId);
    q.bindValue(":t", resolvedAtEpoch);
    q.bindValue(":ver", kCurrentFallbackResolverVersion);
    if (!q.exec())
        logError(QStringLiteral("cacheFallbackVideoId failed: %1").arg(q.lastError().text()));
}

std::optional<QString> CacheRepository::fallbackVideoId(const QString& sourceId, const QString& nativeId) const
{
    QSqlQuery q(m_db.handle());
    q.prepare("SELECT video_id FROM fallback_trailers WHERE source_id = :s AND native_id = :n AND resolver_version >= :ver");
    q.bindValue(":s", sourceId);
    q.bindValue(":n", nativeId);
    q.bindValue(":ver", kCurrentFallbackResolverVersion);
    if (!q.exec() || !q.next())
        return std::nullopt;
    const auto videoId = q.value(0).toString();
    return videoId.isEmpty() ? std::nullopt : std::make_optional(videoId);
}

void CacheRepository::recordFallbackFailure(const QString& sourceId, const QString& nativeId,
                                             qint64 failedAtEpoch, qint64 retryAfterEpoch)
{
    QSqlQuery q(m_db.handle());
    // The WHERE clause on the DO UPDATE normally leaves a row that already
    // holds a successfully-resolved video_id untouched — this only ever
    // records/refreshes a "known dead" marker, never clobbers a working
    // match. The "OR resolver_version < %1" arm is what lets it overwrite a
    // *stale-version* success instead: if a fresh re-resolution attempt
    // under the current logic just failed for a candidate whose cached
    // video_id was trusted under an old (possibly since-tightened) version,
    // that old id needs to actually be cleared, not left in place forever
    // just because it happens to be non-empty.
    q.prepare(QStringLiteral(R"(
        INSERT INTO fallback_trailers (source_id, native_id, query_used, video_id, resolved_at, retry_after, resolver_version)
        VALUES (:s, :n, '', '', :t, :retry, :ver)
        ON CONFLICT(source_id, native_id) DO UPDATE SET
            video_id=excluded.video_id, resolved_at=excluded.resolved_at,
            retry_after=excluded.retry_after, resolver_version=excluded.resolver_version
        WHERE video_id IS NULL OR video_id = '' OR resolver_version < %1
    )").arg(kCurrentFallbackResolverVersion));
    q.bindValue(":s", sourceId);
    q.bindValue(":n", nativeId);
    q.bindValue(":t", failedAtEpoch);
    q.bindValue(":retry", retryAfterEpoch);
    q.bindValue(":ver", kCurrentFallbackResolverVersion);
    if (!q.exec())
        logError(QStringLiteral("recordFallbackFailure failed: %1").arg(q.lastError().text()));
}

bool CacheRepository::fallbackRecentlyFailed(const QString& sourceId, const QString& nativeId, qint64 nowEpoch) const
{
    QSqlQuery q(m_db.handle());
    q.prepare(R"(SELECT 1 FROM fallback_trailers
                 WHERE source_id = :s AND native_id = :n
                   AND (video_id IS NULL OR video_id = '')
                   AND retry_after > :now
                   AND resolver_version >= :ver)");
    q.bindValue(":s", sourceId);
    q.bindValue(":n", nativeId);
    q.bindValue(":now", nowEpoch);
    q.bindValue(":ver", kCurrentFallbackResolverVersion);
    return q.exec() && q.next();
}

void CacheRepository::recordPlayback(const QString& sourceId, const QString& nativeId,
                                      const QString& title, const QString& developer, const QString& storeUrl,
                                      const QString& videoUrl, qint64 playedAtEpoch)
{
    QSqlQuery q(m_db.handle());
    q.prepare(R"(INSERT INTO playback_history (source_id, native_id, title, developer, store_url, video_url, played_at)
                 VALUES (:s, :n, :title, :dev, :store, :video, :t))");
    q.bindValue(":s", sourceId);
    q.bindValue(":n", nativeId);
    q.bindValue(":title", title);
    q.bindValue(":dev", developer);
    q.bindValue(":store", storeUrl);
    q.bindValue(":video", videoUrl);
    q.bindValue(":t", playedAtEpoch);
    if (!q.exec())
        logError(QStringLiteral("recordPlayback failed: %1").arg(q.lastError().text()));
}

bool CacheRepository::playedSince(const QString& sourceId, const QString& nativeId, qint64 sinceEpoch) const
{
    QSqlQuery q(m_db.handle());
    q.prepare("SELECT 1 FROM playback_history WHERE source_id = :s AND native_id = :n AND played_at >= :since LIMIT 1");
    q.bindValue(":s", sourceId);
    q.bindValue(":n", nativeId);
    q.bindValue(":since", sinceEpoch);
    return q.exec() && q.next();
}

void CacheRepository::pruneHistoryOlderThan(qint64 cutoffEpoch)
{
    QSqlQuery q(m_db.handle());
    q.prepare("DELETE FROM playback_history WHERE played_at < :cutoff");
    q.bindValue(":cutoff", cutoffEpoch);
    if (!q.exec())
        logError(QStringLiteral("pruneHistoryOlderThan failed: %1").arg(q.lastError().text()));
}

QList<CacheRepository::PlaybackHistoryEntry> CacheRepository::recentPlaybackHistory(int limit) const
{
    QList<PlaybackHistoryEntry> out;
    QSqlQuery q(m_db.handle());
    q.prepare(R"(
        SELECT h.source_id, h.native_id, h.title, h.developer, h.store_url, h.video_url, h.played_at,
               CASE WHEN b.source_id IS NULL THEN 0 ELSE 1 END AS blocked
        FROM playback_history h
        LEFT JOIN blocked_games b ON b.source_id = h.source_id AND b.native_id = h.native_id
        ORDER BY h.played_at DESC
        LIMIT :limit
    )");
    q.bindValue(":limit", limit);
    if (!q.exec()) {
        logError(QStringLiteral("recentPlaybackHistory query failed: %1").arg(q.lastError().text()));
        return out;
    }
    while (q.next()) {
        PlaybackHistoryEntry e;
        e.sourceId = q.value(0).toString();
        e.nativeId = q.value(1).toString();
        e.title = q.value(2).toString();
        e.developer = q.value(3).toString();
        e.storeUrl = q.value(4).toString();
        e.videoUrl = q.value(5).toString();
        e.playedAt = q.value(6).toLongLong();
        e.blocked = q.value(7).toInt() != 0;
        out.append(e);
    }
    return out;
}

void CacheRepository::blockGame(const QString& sourceId, const QString& nativeId, qint64 blockedAtEpoch)
{
    QSqlQuery q(m_db.handle());
    q.prepare(R"(
        INSERT INTO blocked_games (source_id, native_id, blocked_at) VALUES (:s, :n, :t)
        ON CONFLICT(source_id, native_id) DO UPDATE SET blocked_at=excluded.blocked_at
    )");
    q.bindValue(":s", sourceId);
    q.bindValue(":n", nativeId);
    q.bindValue(":t", blockedAtEpoch);
    if (!q.exec())
        logError(QStringLiteral("blockGame failed: %1").arg(q.lastError().text()));
}

void CacheRepository::unblockGame(const QString& sourceId, const QString& nativeId)
{
    QSqlQuery q(m_db.handle());
    q.prepare("DELETE FROM blocked_games WHERE source_id = :s AND native_id = :n");
    q.bindValue(":s", sourceId);
    q.bindValue(":n", nativeId);
    if (!q.exec())
        logError(QStringLiteral("unblockGame failed: %1").arg(q.lastError().text()));
}

bool CacheRepository::isGameBlocked(const QString& sourceId, const QString& nativeId) const
{
    QSqlQuery q(m_db.handle());
    q.prepare("SELECT 1 FROM blocked_games WHERE source_id = :s AND native_id = :n");
    q.bindValue(":s", sourceId);
    q.bindValue(":n", nativeId);
    return q.exec() && q.next();
}

std::optional<QString> CacheRepository::fallbackQueryUsed(const QString& sourceId, const QString& nativeId) const
{
    QSqlQuery q(m_db.handle());
    q.prepare("SELECT query_used FROM fallback_trailers WHERE source_id = :s AND native_id = :n");
    q.bindValue(":s", sourceId);
    q.bindValue(":n", nativeId);
    if (!q.exec() || !q.next())
        return std::nullopt;
    const auto query = q.value(0).toString();
    return query.isEmpty() ? std::nullopt : std::make_optional(query);
}

} // namespace ssv
