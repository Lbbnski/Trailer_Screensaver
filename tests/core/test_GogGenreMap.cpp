#include "sources/gog/GogGenreMap.h"

#include <QtTest/QtTest>

using namespace ssv;

class TestGogGenreMap : public QObject {
    Q_OBJECT
private slots:
    void mapsKnownLabels();
    void passesThroughUnknownLabelsUnchanged();
    void categoryParamRoundTripsForEveryMappedGenre();
    void categoryParamIsNulloptForGenresGogCannotExpress();
};

void TestGogGenreMap::mapsKnownLabels()
{
    QCOMPARE(GogGenreMap::toCanonical("Simulation"), QStringLiteral("Simulation"));
    QCOMPARE(GogGenreMap::toCanonical("rpg"), QStringLiteral("RPG"));
    QCOMPARE(GogGenreMap::toCanonical("  Strategy  "), QStringLiteral("Strategy"));
    // Confirmed present in a real embed.gog.com response alongside actual
    // genres, in this exact casing.
    QCOMPARE(GogGenreMap::toCanonical("Sci-fi"), QStringLiteral("Sci-Fi"));
}

void TestGogGenreMap::passesThroughUnknownLabelsUnchanged()
{
    // GOG mixes thematic tags into the same "genres" list; those have no
    // canonical equivalent and must pass through unchanged, not crash.
    QCOMPARE(GogGenreMap::toCanonical("Historical"), QStringLiteral("Historical"));
    QCOMPARE(GogGenreMap::toCanonical("Comedy"), QStringLiteral("Comedy"));
}

void TestGogGenreMap::categoryParamRoundTripsForEveryMappedGenre()
{
    const auto param = GogGenreMap::categoryParamFor(QStringLiteral("Horror"));
    QVERIFY(param.has_value());
    QCOMPARE(GogGenreMap::toCanonical(*param), QStringLiteral("Horror"));
}

void TestGogGenreMap::categoryParamIsNulloptForGenresGogCannotExpress()
{
    QVERIFY(!GogGenreMap::categoryParamFor(QStringLiteral("Free To Play")).has_value());
    QVERIFY(!GogGenreMap::categoryParamFor(QStringLiteral("Early Access")).has_value());
}

QTEST_MAIN(TestGogGenreMap)
#include "test_GogGenreMap.moc"
