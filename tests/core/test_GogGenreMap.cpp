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
    void categoryParamIsNulloptForGenresWithNoConfirmedFacet();
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
    const auto param = GogGenreMap::categoryParamFor(QStringLiteral("Strategy"));
    QVERIFY(param.has_value());
    QCOMPARE(GogGenreMap::toCanonical(*param), QStringLiteral("Strategy"));
}

void TestGogGenreMap::categoryParamIsNulloptForGenresWithNoConfirmedFacet()
{
    // These have real GOG genre *labels* (toCanonicalTable still maps them
    // when parsing a product's own genres list) but no confirmed, working
    // `category` query-facet slug — see categoryParamTable()'s comment.
    QVERIFY(!GogGenreMap::categoryParamFor(QStringLiteral("Horror")).has_value());
    QVERIFY(!GogGenreMap::categoryParamFor(QStringLiteral("Sci-Fi")).has_value());
}

void TestGogGenreMap::categoryParamIsNulloptForGenresGogCannotExpress()
{
    QVERIFY(!GogGenreMap::categoryParamFor(QStringLiteral("Free To Play")).has_value());
    QVERIFY(!GogGenreMap::categoryParamFor(QStringLiteral("Early Access")).has_value());
}

QTEST_MAIN(TestGogGenreMap)
#include "test_GogGenreMap.moc"
