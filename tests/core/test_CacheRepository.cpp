#include "cache/CacheDatabase.h"
#include "cache/CacheRepository.h"

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

QTEST_MAIN(TestCacheRepository)
#include "test_CacheRepository.moc"
