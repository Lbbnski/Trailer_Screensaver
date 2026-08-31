#include "config/Config.h"

#include <QtTest/QtTest>

using namespace ssv;

class TestConfig : public QObject {
    Q_OBJECT
private slots:
    void roundTripsThroughJson();
    void malformedJsonFallsBackToDefaults();
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

QTEST_MAIN(TestConfig)
#include "test_Config.moc"
