#include "config/Config.h"
#include "sources/GenreTaxonomy.h"

#include <QtTest/QtTest>

using namespace ssv;

class TestConfig : public QObject {
    Q_OBJECT
private slots:
    void roundTripsThroughJson();
    void malformedJsonFallsBackToDefaults();
    void originalFullGenreListIsExtendedToNewGenres();
    void customGenreSelectionIsLeftAlone();
};

void TestConfig::roundTripsThroughJson()
{
    Config original;
    original.playback.maxResolution = MaxResolution::P480;
    original.playback.monitorMode = MonitorMode::PrimaryOnly;
    original.playback.muted = false;
    original.filter.mode = GenreFilter::Mode::BlockList;
    original.filter.genres = {"Horror", "Shooter"};
    original.filter.maxAge = 12;
    original.filter.upcomingMode = UpcomingMode::OnlyUpcoming;
    original.filter.blockedContentDescriptors = {"adult-only-sexual-content"};
    original.sources.enabled = {"steam"};
    original.advanced.maxCatalogRequestsPerRun = 25;

    bool ok = false;
    const Config restored = Config::fromJson(original.toJson(), &ok);

    QVERIFY(ok);
    QCOMPARE(restored.playback.maxResolution, MaxResolution::P480);
    QCOMPARE(restored.playback.monitorMode, MonitorMode::PrimaryOnly);
    QCOMPARE(restored.playback.muted, false);
    QCOMPARE(restored.filter.mode, GenreFilter::Mode::BlockList);
    QCOMPARE(restored.filter.genres, QStringList({"Horror", "Shooter"}));
    QCOMPARE(restored.filter.maxAge, 12);
    QCOMPARE(restored.filter.upcomingMode, UpcomingMode::OnlyUpcoming);
    QCOMPARE(restored.filter.blockedContentDescriptors, QStringList({"adult-only-sexual-content"}));
    QCOMPARE(restored.sources.enabled, QStringList({"steam"}));
    QCOMPARE(restored.advanced.maxCatalogRequestsPerRun, 25);
}

void TestConfig::malformedJsonFallsBackToDefaults()
{
    bool ok = true;
    const Config cfg = Config::fromJson("not valid json", &ok);
    QVERIFY(!ok);
    QCOMPARE(cfg.version, Config::kCurrentVersion);
    QCOMPARE(cfg.sources.enabled, QStringList({"steam"}));
}

void TestConfig::originalFullGenreListIsExtendedToNewGenres()
{
    Config legacy;
    legacy.filter.genres = GenreTaxonomy::canonicalGenres().mid(0, GenreTaxonomy::kLegacyGenreCount);

    const Config restored = Config::fromJson(legacy.toJson(), nullptr);
    QCOMPARE(restored.filter.genres, GenreTaxonomy::canonicalGenres());
}

void TestConfig::customGenreSelectionIsLeftAlone()
{
    Config narrowed;
    narrowed.filter.genres = GenreTaxonomy::canonicalGenres().mid(0, GenreTaxonomy::kLegacyGenreCount);
    narrowed.filter.genres.removeAll(QStringLiteral("Horror"));

    QCOMPARE(Config::fromJson(narrowed.toJson(), nullptr).filter.genres, narrowed.filter.genres);

    Config blockList;
    blockList.filter.mode = GenreFilter::Mode::BlockList;
    blockList.filter.genres = GenreTaxonomy::canonicalGenres().mid(0, GenreTaxonomy::kLegacyGenreCount);
    QCOMPARE(Config::fromJson(blockList.toJson(), nullptr).filter.genres, blockList.filter.genres);
}

QTEST_MAIN(TestConfig)
#include "test_Config.moc"
