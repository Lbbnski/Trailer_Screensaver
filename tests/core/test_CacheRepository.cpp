#include "cache/CacheDatabase.h"
#include "cache/CacheRepository.h"

#include <QSqlQuery>
#include <QtTest/QtTest>

#include <memory>

using namespace ssv;

class TestCacheRepository : public QObject {
    Q_OBJECT
private slots:
    void init();

    void upsertThenGetRoundTrips();
    void freshDetailsRespectsStaleAfter();
    void candidatePageStatePersists();
    void fallbackVideoIdRoundTrips();
    void fallbackFailureBlocksUntilRetryAfter();
    void fallbackFailureNeverClobbersSuccess();
    void isPopularStaysStickyAcrossUpserts();
    void staleResolverVersionIsTreatedAsUnresolved();
    void staleResolverVersionFailureIsNotBlocking();

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

QTEST_MAIN(TestCacheRepository)
#include "test_CacheRepository.moc"
