#include "cache/CacheDatabase.h"
#include "util/Logging.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace ssv {

namespace {

bool execOrLog(QSqlQuery& q, const QString& sql)
{
    if (!q.exec(sql)) {
        logError(QStringLiteral("cache schema migration failed: %1 (%2)")
                     .arg(sql, q.lastError().text()));
        return false;
    }
    return true;
}

} // namespace

bool CacheDatabase::migrate(QSqlDatabase& db)
{
    QSqlQuery q(db);

    // Each table's primary key includes source_id (or is scoped by it) so
    // rows from independent IMetadataSource implementations never collide.
    static const QStringList statements{
        R"(CREATE TABLE IF NOT EXISTS apps (
             source_id TEXT NOT NULL,
             native_id TEXT NOT NULL,
             title TEXT,
             canonical_genres TEXT,
             age_rating INTEGER,
             content_descriptors TEXT,
             renditions_json TEXT,
             has_trailer INTEGER,
             details_fetched_at INTEGER,
             details_stale_after INTEGER,
             PRIMARY KEY (source_id, native_id)
           ))",
        R"(CREATE TABLE IF NOT EXISTS genre_candidates (
             source_id TEXT NOT NULL,
             genre TEXT NOT NULL,
             native_id TEXT NOT NULL,
             discovered_at INTEGER,
             PRIMARY KEY (source_id, genre, native_id)
           ))",
        R"(CREATE TABLE IF NOT EXISTS candidate_pages (
             source_id TEXT NOT NULL,
             genre TEXT NOT NULL,
             last_search_start INTEGER NOT NULL DEFAULT 0,
             exhausted INTEGER NOT NULL DEFAULT 0,
             last_refreshed_at INTEGER,
             PRIMARY KEY (source_id, genre)
           ))",
        R"(CREATE TABLE IF NOT EXISTS fallback_trailers (
             source_id TEXT NOT NULL,
             native_id TEXT NOT NULL,
             query_used TEXT,
             video_id TEXT,
             resolved_at INTEGER,
             PRIMARY KEY (source_id, native_id)
           ))",
        R"(CREATE TABLE IF NOT EXISTS playback_history (
             source_id TEXT NOT NULL,
             native_id TEXT NOT NULL,
             played_at INTEGER NOT NULL
           ))",
        R"(CREATE INDEX IF NOT EXISTS idx_playback_history_played_at
             ON playback_history(played_at))",
    };

    for (const auto& sql : statements) {
        if (!execOrLog(q, sql))
            return false;
    }

    // Best-effort column addition for databases created before this field
    // existed. SQLite has no "ADD COLUMN IF NOT EXISTS"; on a database that
    // already has it, this fails with "duplicate column name" every time,
    // which is expected and not logged as an error.
    q.exec(QStringLiteral("ALTER TABLE apps ADD COLUMN developer TEXT"));

    // Marks a fallback_trailers row as "known unplayable until this time" —
    // written when YoutubeFallbackResolver::resolve() fails to find a
    // trustworthy match (video_id/query_used stay empty for that row).
    // Without this, a candidate with no rendition of its own that YouTube
    // search can't match gets re-attempted via a fresh yt-dlp subprocess
    // *every single time* it comes up in the playlist forever, and — since
    // it never records a fallback video id — PlaylistEngine had no way to
    // tell it apart from a candidate that simply hasn't been tried yet,
    // so it kept re-offering the same chronically-dead candidates instead
    // of ever excluding them (see TrailerResolver::kFallbackRetryAfterSeconds).
    q.exec(QStringLiteral("ALTER TABLE fallback_trailers ADD COLUMN retry_after INTEGER NOT NULL DEFAULT 0"));

    // True once a candidate has been seen in a source's popularity-sorted
    // discovery pass (SteamSpy's top100*, GOG's "popularity" sort, IGDB's
    // total_rating_count) — lets PlaylistEngine actually give
    // GenreFilter::preferPopular an effect on which candidates get played,
    // not just on which ones get discovered into the cache in the first
    // place (see PlaylistEngine::buildPlaylist's popularity weighting).
    q.exec(QStringLiteral("ALTER TABLE apps ADD COLUMN is_popular INTEGER NOT NULL DEFAULT 0"));

    return true;
}

CacheDatabase CacheDatabase::open(const QString& path)
{
    CacheDatabase self;

    QDir().mkpath(QFileInfo(path).absolutePath());

    // Each CacheDatabase instance gets its own uniquely-named connection —
    // QSqlDatabase's default-connection model isn't safe to share across
    // independently-constructed instances (e.g. in unit tests).
    const QString connectionName = QStringLiteral("ssv_cache_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    self.m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    self.m_db.setDatabaseName(path);

    if (!self.m_db.open()) {
        logError(QStringLiteral("failed to open cache database at %1: %2")
                     .arg(path, self.m_db.lastError().text()));
        self.m_open = false;
        return self;
    }

    {
        QSqlQuery pragma(self.m_db);
        pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
        pragma.exec(QStringLiteral("PRAGMA foreign_keys=ON"));
    }

    self.m_open = migrate(self.m_db);
    return self;
}

bool CacheDatabase::isOpen() const
{
    return m_open;
}

QSqlDatabase& CacheDatabase::handle()
{
    return m_db;
}

} // namespace ssv
