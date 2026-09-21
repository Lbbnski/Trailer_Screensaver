#include "cache/CacheDatabase.h"
#include "cache/CacheRepository.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <memory>

using namespace ssv;

class TestCacheRepository : public QObject {
    Q_OBJECT
private slots:
    void init();

    void upsertThenGetRoundTrips();
    void comingSoonIsStoredMarkedAndOverwritten();
    void unfetchedCandidatesExcludesFreshRows();
    void freshDetailsRespectsStaleAfter();
    void candidatePageStatePersists();
    void fallbackVideoIdRoundTrips();
    void fallbackFailureBlocksUntilRetryAfter();
    void fallbackFailureNeverClobbersSuccess();
    void isPopularStaysStickyAcrossUpserts();
    void staleResolverVersionIsTreatedAsUnresolved();
    void staleResolverVersionFailureIsNotBlocking();
    void storeUrlRoundTrips();
    void playbackHistoryRecordsSnapshotAndOrdersNewestFirst();
    void blockGameRoundTripsAndCanBeUndone();
    void fallbackQueryUsedRoundTrips();
    void rejectedVideoIsIgnoredByFallbackCache();
    void migrationStripsFallbackVideosBakedIntoApps();

private:
    std::unique_ptr<CacheDatabase> m_db;
    std::unique_ptr<CacheRepository> m_repo;
};

void TestCacheRepository::init()
{
    m_db = std::make_unique<CacheDatabase>(CacheDatabase::open(QStringLiteral(":memory:")));
    QVERIFY(m_db->isOpen());
    m_repo = std::make_unique<CacheRepository>(*m_db);
}

void TestCacheRepository::upsertThenGetRoundTrips()
{
    TrailerCandidate candidate;
    candidate.sourceId = QStringLiteral("steam");
    candidate.nativeId = QStringLiteral("1091500");
    candidate.title = QStringLiteral("Test Game");
    candidate.canonicalGenres = {"Action", "RPG"};
    candidate.ageRating = 17;
    candidate.renditions.append(TrailerRendition{QStringLiteral("https://example.invalid/max.mp4"), 0, QStringLiteral("mp4")});

    m_repo->upsertAppDetails(candidate, /*hasTrailer=*/true, 1000, 2000);

    const auto fetched = m_repo->getAppDetails("steam", "1091500");
    QVERIFY(fetched.has_value());
    QCOMPARE(fetched->title, QStringLiteral("Test Game"));
    QCOMPARE(fetched->canonicalGenres, QStringList({"Action", "RPG"}));
    QCOMPARE(fetched->ageRating, 17);
    QCOMPARE(fetched->renditions.size(), 1);
    QCOMPARE(fetched->renditions.first().url, QStringLiteral("https://example.invalid/max.mp4"));
}

void TestCacheRepository::freshDetailsRespectsStaleAfter()
{
    TrailerCandidate candidate;
    candidate.sourceId = QStringLiteral("steam");
    candidate.nativeId = QStringLiteral("42");

    m_repo->upsertAppDetails(candidate, false, 1000, 2000);

    QVERIFY(m_repo->hasFreshDetails("steam", "42", 1500));  // now < stale_after
    QVERIFY(!m_repo->hasFreshDetails("steam", "42", 2500)); // now > stale_after
}

void TestCacheRepository::candidatePageStatePersists()
{
    CacheRepository::PageState state;
    state.lastSearchStart = 150;
    state.exhausted = true;
    state.lastRefreshedAt = 12345;

    m_repo->setCandidatePageState("steam", "Horror", state);

    const auto restored = m_repo->candidatePageState("steam", "Horror");
    QCOMPARE(restored.lastSearchStart, 150);
    QCOMPARE(restored.exhausted, true);
    QCOMPARE(restored.lastRefreshedAt, qint64(12345));
}

void TestCacheRepository::fallbackVideoIdRoundTrips()
{
    QVERIFY(!m_repo->fallbackVideoId("steam", "99").has_value());

    m_repo->cacheFallbackVideoId("steam", "99", "some game official trailer", "abc123", 5000);

    const auto videoId = m_repo->fallbackVideoId("steam", "99");
    QVERIFY(videoId.has_value());
    QCOMPARE(*videoId, QStringLiteral("abc123"));
}

void TestCacheRepository::fallbackFailureBlocksUntilRetryAfter()
{
    QVERIFY(!m_repo->fallbackRecentlyFailed("steam", "7", 5000));

    m_repo->recordFallbackFailure("steam", "7", 5000, 6000);

    QVERIFY(m_repo->fallbackRecentlyFailed("steam", "7", 5500));  // now < retry_after
    QVERIFY(!m_repo->fallbackRecentlyFailed("steam", "7", 6500)); // now > retry_after
}

void TestCacheRepository::fallbackFailureNeverClobbersSuccess()
{
    m_repo->cacheFallbackVideoId("steam", "8", "some game trailer", "xyz789", 5000);

    m_repo->recordFallbackFailure("steam", "8", 6000, 7000);

    const auto videoId = m_repo->fallbackVideoId("steam", "8");
    QVERIFY(videoId.has_value());
    QCOMPARE(*videoId, QStringLiteral("xyz789"));
    QVERIFY(!m_repo->fallbackRecentlyFailed("steam", "8", 6500));
}

void TestCacheRepository::isPopularStaysStickyAcrossUpserts()
{
    TrailerCandidate candidate;
    candidate.sourceId = QStringLiteral("steam");
    candidate.nativeId = QStringLiteral("55");
    candidate.discoveredAsPopular = true;

    m_repo->upsertAppDetails(candidate, false, 1000, 2000);
    QVERIFY(m_repo->getAppDetails("steam", "55")->discoveredAsPopular);

    // A later refresh that doesn't happen to come from a popularity-sorted
    // discovery pass this time (the common case: most refreshes come from
    // genre discovery) must not un-set a popularity flag learned earlier.
    candidate.discoveredAsPopular = false;
    m_repo->upsertAppDetails(candidate, false, 3000, 4000);
    QVERIFY(m_repo->getAppDetails("steam", "55")->discoveredAsPopular);
}

void TestCacheRepository::staleResolverVersionIsTreatedAsUnresolved()
{
    // Simulates a row cached before resolver versioning existed (real ones
    // default to resolver_version 0 via the schema migration) — must not be
    // trusted, so a matching-logic change actually gets to re-evaluate it.
    QSqlQuery insert(m_db->handle());
    insert.prepare(R"(INSERT INTO fallback_trailers (source_id, native_id, query_used, video_id, resolved_at, resolver_version)
                       VALUES ('steam', '11', 'old query', 'oldvid', 1000, 0))");
    QVERIFY(insert.exec());

    QVERIFY(!m_repo->fallbackVideoId("steam", "11").has_value());
}

void TestCacheRepository::staleResolverVersionFailureIsNotBlocking()
{
    // Same idea for a failure recorded under an old resolver version: it
    // shouldn't keep blocking retries under the current logic.
    QSqlQuery insert(m_db->handle());
    insert.prepare(R"(INSERT INTO fallback_trailers (source_id, native_id, query_used, video_id, resolved_at, retry_after, resolver_version)
                       VALUES ('steam', '12', '', '', 1000, 999999999999, 0))");
    QVERIFY(insert.exec());

    QVERIFY(!m_repo->fallbackRecentlyFailed("steam", "12", 1500));
}

void TestCacheRepository::storeUrlRoundTrips()
{
    TrailerCandidate candidate;
    candidate.sourceId = QStringLiteral("gog");
    candidate.nativeId = QStringLiteral("2001");
    candidate.storeUrl = QStringLiteral("https://www.gog.com/en/game/example");

    m_repo->upsertAppDetails(candidate, false, 1000, 2000);

    QCOMPARE(m_repo->getAppDetails("gog", "2001")->storeUrl, QStringLiteral("https://www.gog.com/en/game/example"));
}

void TestCacheRepository::playbackHistoryRecordsSnapshotAndOrdersNewestFirst()
{
    m_repo->recordPlayback("steam", "1", "First Game", "Dev A", "https://store.invalid/1",
                            "https://youtube.invalid/watch?v=aaa", 1000);
    m_repo->recordPlayback("steam", "2", "Second Game", "Dev B", "https://store.invalid/2",
                            "https://youtube.invalid/watch?v=bbb", 2000);

    const auto history = m_repo->recentPlaybackHistory(10);
    QCOMPARE(history.size(), 2);
    // Newest first.
    QCOMPARE(history[0].nativeId, QStringLiteral("2"));
    QCOMPARE(history[0].title, QStringLiteral("Second Game"));
    QCOMPARE(history[0].developer, QStringLiteral("Dev B"));
    QCOMPARE(history[0].storeUrl, QStringLiteral("https://store.invalid/2"));
    QCOMPARE(history[0].videoUrl, QStringLiteral("https://youtube.invalid/watch?v=bbb"));
    QCOMPARE(history[0].playedAt, qint64(2000));
    QVERIFY(!history[0].blocked);
    QCOMPARE(history[1].nativeId, QStringLiteral("1"));
}

void TestCacheRepository::blockGameRoundTripsAndCanBeUndone()
{
    QVERIFY(!m_repo->isGameBlocked("steam", "3"));

    m_repo->blockGame("steam", "3", 5000);
    QVERIFY(m_repo->isGameBlocked("steam", "3"));

    // recentPlaybackHistory's blocked flag reflects the same state.
    m_repo->recordPlayback("steam", "3", "Blocked Game", "", "", "", 5000);
    QVERIFY(m_repo->recentPlaybackHistory(10).first().blocked);

    m_repo->unblockGame("steam", "3");
    QVERIFY(!m_repo->isGameBlocked("steam", "3"));
}

void TestCacheRepository::fallbackQueryUsedRoundTrips()
{
    QVERIFY(!m_repo->fallbackQueryUsed("steam", "4").has_value());

    m_repo->cacheFallbackVideoId("steam", "4", "some game official trailer", "vid123", 5000);

    const auto query = m_repo->fallbackQueryUsed("steam", "4");
    QVERIFY(query.has_value());
    QCOMPARE(*query, QStringLiteral("some game official trailer"));
}

void TestCacheRepository::rejectedVideoIsIgnoredByFallbackCache()
{
    m_repo->cacheFallbackVideoId("steam", "5", "some game official trailer", "badvid", 5000);
    QVERIFY(m_repo->fallbackVideoId("steam", "5").has_value());
    QVERIFY(!m_repo->isVideoRejected("badvid"));

    m_repo->rejectVideo("badvid", 6000);

    QVERIFY(m_repo->isVideoRejected("badvid"));
    // The cached match is now treated as unresolved, so the game re-resolves
    // to a different video instead of replaying the reported one.
    QVERIFY(!m_repo->fallbackVideoId("steam", "5").has_value());
    // Rejecting one video says nothing about other videos or other games.
    QVERIFY(!m_repo->isVideoRejected("othervid"));
}

void TestCacheRepository::migrationStripsFallbackVideosBakedIntoApps()
{
    // Reproduces a real cache: a fallback video copied into apps.renditions_json
    // (which ensurePlayable() used to do) made a candidate arrive with a
    // rendition attached and skip the versioned fallback_trailers cache
    // entirely, so a stale/reported match kept playing.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("cache.sqlite3");

    {
        auto db = CacheDatabase::open(path);
        QVERIFY(db.isOpen());
        CacheRepository repo(db);

        TrailerCandidate baked;
        baked.sourceId = "steam";
        baked.nativeId = "1";
        baked.renditions.append(TrailerRendition{"https://www.youtube.com/watch?v=fallbackvid", 0, "youtube"});
        repo.upsertAppDetails(baked, true, 1000, 999999999);
        repo.cacheFallbackVideoId("steam", "1", "q", "fallbackvid", 1000);

        // An IGDB-style candidate whose youtube video is its own curated
        // one (no fallback row) must be left alone.
        TrailerCandidate curated;
        curated.sourceId = "igdb";
        curated.nativeId = "2";
        curated.renditions.append(TrailerRendition{"https://www.youtube.com/watch?v=curatedvid", 0, "youtube"});
        repo.upsertAppDetails(curated, true, 1000, 999999999);

        // A Steam CDN candidate is untouched too.
        TrailerCandidate cdn;
        cdn.sourceId = "steam";
        cdn.nativeId = "3";
        cdn.renditions.append(TrailerRendition{"https://cdn.invalid/3.mp4", 0, "mp4"});
        repo.upsertAppDetails(cdn, true, 1000, 999999999);
    }

    // Reopening runs the migration again over the populated file.
    auto reopened = CacheDatabase::open(path);
    QVERIFY(reopened.isOpen());
    CacheRepository repo(reopened);

    QVERIFY(repo.getAppDetails("steam", "1")->renditions.isEmpty());
    QCOMPARE(repo.getAppDetails("igdb", "2")->renditions.size(), 1);
    QCOMPARE(repo.getAppDetails("steam", "3")->renditions.size(), 1);
    // ...and the fallback cache itself is untouched, so it can still serve
    // the video (subject to its own version/rejection checks).
    QVERIFY(repo.fallbackVideoId("steam", "1").has_value());
}

void TestCacheRepository::comingSoonIsStoredMarkedAndOverwritten()
{
    TrailerCandidate c;
    c.sourceId = QStringLiteral("steam");
    c.nativeId = QStringLiteral("10");
    c.title = QStringLiteral("Soon");
    m_repo->upsertAppDetails(c, false, 1000, 2000);
    QVERIFY(!m_repo->getAppDetails(c.sourceId, c.nativeId)->comingSoon);

    m_repo->markComingSoon(c.sourceId, {c.nativeId, QStringLiteral("not-cached")});
    QVERIFY(m_repo->getAppDetails(c.sourceId, c.nativeId)->comingSoon);

    // A re-fetch after release clears the flag again.
    m_repo->upsertAppDetails(c, false, 1500, 2500);
    QVERIFY(!m_repo->getAppDetails(c.sourceId, c.nativeId)->comingSoon);
}

void TestCacheRepository::unfetchedCandidatesExcludesFreshRows()
{
    const QString src = QStringLiteral("steam");
    m_repo->addGenreCandidates(src, QStringLiteral("__upcoming__"), {"1", "2", "3"}, 1000);

    TrailerCandidate c;
    c.sourceId = src;
    c.nativeId = QStringLiteral("2");
    m_repo->upsertAppDetails(c, false, 1000, 5000);

    const auto ids = m_repo->unfetchedCandidates(src, QStringLiteral("__upcoming__"), 2000, 10);
    QCOMPARE(QSet<QString>(ids.begin(), ids.end()), QSet<QString>({"1", "3"}));
    QCOMPARE(m_repo->unfetchedCandidates(src, QStringLiteral("__upcoming__"), 2000, 1).size(), 1);
}

QTEST_MAIN(TestCacheRepository)
#include "test_CacheRepository.moc"
