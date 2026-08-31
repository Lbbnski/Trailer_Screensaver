#include "cache/CacheDatabase.h"
#include "cache/CacheRepository.h"
#include "selection/PlaylistEngine.h"

#include <QDateTime>
#include <QtTest/QtTest>

#include <memory>

using namespace ssv;

namespace {

TrailerCandidate makeCandidate(const QString& id, QStringList genres, int age, QStringList descriptors = {})
{
    TrailerCandidate c;
    c.sourceId = QStringLiteral("steam");
    c.nativeId = id;
    c.title = QStringLiteral("Game %1").arg(id);
    c.canonicalGenres = std::move(genres);
    c.ageRating = age;
    c.contentDescriptors = std::move(descriptors);
    c.renditions.append(TrailerRendition{QStringLiteral("https://example.invalid/%1.mp4").arg(id), 480, QStringLiteral("mp4")});
    return c;
}

} // namespace

class TestPlaylistEngine : public QObject {
    Q_OBJECT
private slots:
    void init();

    void allowListKeepsMatchingGenre();
    void allowListDropsNonMatchingGenre();
    void emptyAllowListKeepsEverything();
    void blockListDropsMatchingGenre();
    void ageCutoffExcludesOverAgeCandidates();
    void blockedContentDescriptorExcludesRegardlessOfAge();
    void recentlyPlayedIsExcluded();

private:
    std::unique_ptr<CacheDatabase> m_db;
    std::unique_ptr<CacheRepository> m_repo;
    std::unique_ptr<PlaylistEngine> m_engine;
};

void TestPlaylistEngine::init()
{
    m_db = std::make_unique<CacheDatabase>(CacheDatabase::open(QStringLiteral(":memory:")));
    QVERIFY(m_db->isOpen());
    m_repo = std::make_unique<CacheRepository>(*m_db);
    m_engine = std::make_unique<PlaylistEngine>(*m_repo);
}

void TestPlaylistEngine::allowListKeepsMatchingGenre()
{
    FilterConfig filter;
    filter.mode = GenreFilter::Mode::AllowList;
    filter.genres = {"Horror"};

    const auto candidate = makeCandidate("1", {"Horror", "Indie"}, 18);
    QVERIFY(m_engine->passesFilter(candidate, filter));
}

void TestPlaylistEngine::allowListDropsNonMatchingGenre()
{
    FilterConfig filter;
    filter.mode = GenreFilter::Mode::AllowList;
    filter.genres = {"Horror"};

    const auto candidate = makeCandidate("1", {"Racing"}, 18);
    QVERIFY(!m_engine->passesFilter(candidate, filter));
}

void TestPlaylistEngine::emptyAllowListKeepsEverything()
{
    FilterConfig filter;
    filter.mode = GenreFilter::Mode::AllowList;
    filter.genres = {}; // no restriction configured

    const auto candidate = makeCandidate("1", {"Racing"}, 18);
    QVERIFY(m_engine->passesFilter(candidate, filter));
}

void TestPlaylistEngine::blockListDropsMatchingGenre()
{
    FilterConfig filter;
    filter.mode = GenreFilter::Mode::BlockList;
    filter.genres = {"Horror"};

    QVERIFY(!m_engine->passesFilter(makeCandidate("1", {"Horror"}, 18), filter));
    QVERIFY(m_engine->passesFilter(makeCandidate("2", {"Racing"}, 18), filter));
}

void TestPlaylistEngine::ageCutoffExcludesOverAgeCandidates()
{
    FilterConfig filter;
    filter.maxAge = 12;

    QVERIFY(m_engine->passesFilter(makeCandidate("1", {}, 12), filter));
    QVERIFY(!m_engine->passesFilter(makeCandidate("2", {}, 18), filter));
}

void TestPlaylistEngine::blockedContentDescriptorExcludesRegardlessOfAge()
{
    FilterConfig filter;
    filter.maxAge = 18;
    filter.blockedContentDescriptors = {"adult-only-sexual-content"};

    const auto candidate = makeCandidate("1", {}, 18, {"adult-only-sexual-content"});
    QVERIFY(!m_engine->passesFilter(candidate, filter));
}

void TestPlaylistEngine::recentlyPlayedIsExcluded()
{
    FilterConfig filter;
    const auto candidate = makeCandidate("1", {}, 18);

    m_repo->recordPlayback(candidate.sourceId, candidate.nativeId, QDateTime::currentSecsSinceEpoch());

    const auto playlist = m_engine->buildPlaylist({candidate}, filter, /*noRepeatWindowSeconds=*/3600);
    QVERIFY(playlist.isEmpty());
}

QTEST_MAIN(TestPlaylistEngine)
#include "test_PlaylistEngine.moc"
