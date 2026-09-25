#include "sources/GenreTaxonomy.h"
#include "sources/gog/GogGenreMap.h"

#include <QtTest/QtTest>

using namespace ssv;

class TestGogGenreMap : public QObject {
    Q_OBJECT
private slots:
    void mapsGenresAndTags();
    void passesThroughUnknownLabelsUnchanged();
    void canonicalIfKnownRejectsUnknownLabels();
    void catalogFilterUsesGenreFacetsWhereGogHasThem();
    void catalogFilterUsesTagsForEverythingElse();
    void catalogFilterIsNulloptForGenresGogCannotExpress();
    void everyMappedGenreIsCanonical();
};

void TestGogGenreMap::mapsGenresAndTags()
{
    QCOMPARE(GogGenreMap::toCanonical("Simulation"), QStringLiteral("Simulation"));
    QCOMPARE(GogGenreMap::toCanonical("Role-playing"), QStringLiteral("RPG"));
    QCOMPARE(GogGenreMap::toCanonical("  Strategy  "), QStringLiteral("Strategy"));
    QCOMPARE(GogGenreMap::toCanonical("Sci-fi"), QStringLiteral("Sci-Fi"));
    // Tags: several fold into one canonical genre.
    QCOMPARE(GogGenreMap::toCanonical("Roguelite"), QStringLiteral("Roguelike"));
    QCOMPARE(GogGenreMap::toCanonical("Survival Horror"), QStringLiteral("Horror"));
    QCOMPARE(GogGenreMap::toCanonical("Point&Click"), QStringLiteral("Point & Click"));
}

void TestGogGenreMap::passesThroughUnknownLabelsUnchanged()
{
    QCOMPARE(GogGenreMap::toCanonical("Great Soundtrack"), QStringLiteral("Great Soundtrack"));
}

void TestGogGenreMap::canonicalIfKnownRejectsUnknownLabels()
{
    QVERIFY(!GogGenreMap::canonicalIfKnown("Great Soundtrack").has_value());
    QVERIFY(!GogGenreMap::canonicalIfKnown("Only On GOG").has_value());
    QCOMPARE(*GogGenreMap::canonicalIfKnown("Cyberpunk"), QStringLiteral("Cyberpunk"));
}

void TestGogGenreMap::catalogFilterUsesGenreFacetsWhereGogHasThem()
{
    const auto filter = GogGenreMap::catalogFilterFor(QStringLiteral("RPG"));
    QVERIFY(filter.has_value());
    QCOMPARE(filter->param, QStringLiteral("genres"));
    QCOMPARE(filter->slug, QStringLiteral("rpg"));
}

void TestGogGenreMap::catalogFilterUsesTagsForEverythingElse()
{
    const auto filter = GogGenreMap::catalogFilterFor(QStringLiteral("Roguelike"));
    QVERIFY(filter.has_value());
    QCOMPARE(filter->param, QStringLiteral("tags"));
    QCOMPARE(filter->slug, QStringLiteral("roguelike"));
}

void TestGogGenreMap::catalogFilterIsNulloptForGenresGogCannotExpress()
{
    QVERIFY(!GogGenreMap::catalogFilterFor(QStringLiteral("Massively Multiplayer")).has_value());
    QVERIFY(!GogGenreMap::catalogFilterFor(QStringLiteral("Early Access")).has_value());
}

void TestGogGenreMap::everyMappedGenreIsCanonical()
{
    // A typo in either table would otherwise create a "genre" nobody can
    // select in the settings dialog.
    for (const auto& genre : GenreTaxonomy::canonicalGenres()) {
        if (const auto filter = GogGenreMap::catalogFilterFor(genre))
            QVERIFY2(!filter->slug.isEmpty(), qPrintable(genre));
    }
    for (const char* label : {"Roguelike", "Cozy", "Cyberpunk", "Sexual Content", "Team sport"}) {
        const auto canonical = GogGenreMap::canonicalIfKnown(QString::fromLatin1(label));
        QVERIFY2(canonical.has_value(), label);
        QVERIFY2(GenreTaxonomy::isCanonical(*canonical), qPrintable(*canonical));
    }
}

QTEST_MAIN(TestGogGenreMap)
#include "test_GogGenreMap.moc"
