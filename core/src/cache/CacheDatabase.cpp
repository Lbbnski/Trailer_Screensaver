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
        R"(CREATE TABLE IF NOT EXISTS blocked_games (
             source_id TEXT NOT NULL,
             native_id TEXT NOT NULL,
             blocked_at INTEGER NOT NULL,
             PRIMARY KEY (source_id, native_id)
           ))",
        // YouTube video ids the user reported as "not a game trailer" — never
        // played again, for any game (see CacheRepository::rejectVideo).
        R"(CREATE TABLE IF NOT EXISTS rejected_videos (
             video_id TEXT NOT NULL PRIMARY KEY,
             rejected_at INTEGER NOT NULL
           ))",
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

    // Defaults to 0 for every pre-existing row, which is always below
    // CacheRepository::kCurrentFallbackResolverVersion — see that constant's
    // comment for why that's exactly the point: it's what makes a future
    // change to YoutubeFallbackResolver's matching logic actually apply to
    // an already-populated cache instead of being silently inert for every
    // row resolved before the change.
    q.exec(QStringLiteral("ALTER TABLE fallback_trailers ADD COLUMN resolver_version INTEGER NOT NULL DEFAULT 0"));

    // True once a candidate has been seen in a source's popularity-sorted
    // discovery pass (SteamSpy's top100*, GOG's "popularity" sort, IGDB's
    // total_rating_count) — lets PlaylistEngine actually give
    // GenreFilter::preferPopular an effect on which candidates get played,
    // not just on which ones get discovered into the cache in the first
    // place (see PlaylistEngine::buildPlaylist's popularity weighting).
    q.exec(QStringLiteral("ALTER TABLE apps ADD COLUMN is_popular INTEGER NOT NULL DEFAULT 0"));

    // A page the user can open to look a game up themselves (Steam/GOG
    // store page, IGDB's own game page) — surfaced in the playback-history
    // view (see qtui/src/PlaybackHistoryDialog.h).
    q.exec(QStringLiteral("ALTER TABLE apps ADD COLUMN store_url TEXT"));

    // Whether the game was unreleased when its details were last fetched
    // (see TrailerCandidate::comingSoon). Every pre-existing row defaults to
    // 0 = released; upcoming discovery flips the ones it finds via
    // CacheRepository::markComingSoon rather than waiting out the details TTL.
    q.exec(QStringLiteral("ALTER TABLE apps ADD COLUMN coming_soon INTEGER NOT NULL DEFAULT 0"));

    // playback_history started as just enough to drive the no-repeat-window
    // check (source_id/native_id/played_at). These columns denormalize a
    // snapshot of what was actually played — title/developer/store_url from
    // the candidate, video_url from the rendition actually loaded — so the
    // playback-history view stays meaningful (readable titles, a working
    // store link, enough context to report a bad match) even if the
    // matching `apps` row later expires or gets overwritten with different
    // data, rather than depending on a join that could go stale or miss.
    q.exec(QStringLiteral("ALTER TABLE playback_history ADD COLUMN title TEXT"));
    q.exec(QStringLiteral("ALTER TABLE playback_history ADD COLUMN developer TEXT"));
    q.exec(QStringLiteral("ALTER TABLE playback_history ADD COLUMN store_url TEXT"));
    q.exec(QStringLiteral("ALTER TABLE playback_history ADD COLUMN video_url TEXT"));

    // TrailerResolver::ensurePlayable() used to copy a resolved fallback
    // video into apps.renditions_json as well as fallback_trailers. A
    // candidate loaded from `apps` then arrived with a rendition already
    // attached and never consulted fallback_trailers — where the
    // resolver_version check lives — at all, so a stale or since-rejected
    // match kept playing straight out of `apps` for the full cache TTL
    // (confirmed on a real cache: two user-reported bad trailers were
    // still resolver_version 0 in fallback_trailers, yet playing). The
    // fallback video's only home is fallback_trailers now; strip any copy
    // still sitting in `apps` so those rows go back through it. Cast to
    // TEXT because the column holds a BLOB when written from a QByteArray.
    // A no-op once nothing bakes fallback videos into `apps` anymore, and
    // harmless to repeat: a stripped row just re-resolves from the cache.
    q.exec(QStringLiteral(R"(
        UPDATE apps SET renditions_json = '[]'
        WHERE EXISTS (
            SELECT 1 FROM fallback_trailers f
            WHERE f.source_id = apps.source_id AND f.native_id = apps.native_id
              AND f.video_id IS NOT NULL AND f.video_id <> ''
              AND instr(CAST(apps.renditions_json AS TEXT), f.video_id) > 0
        )
    )"));

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
