#include "sources/igdb/IgdbGenreMap.h"

#include <QtTest/QtTest>

using namespace ssv;

class TestIgdbGenreMap : public QObject {
    Q_OBJECT
private slots:
    void mapsKnownGenreThemeAndGameModeLabels();
    void passesThroughUnknownLabelsUnchanged();
    void specificLabelsImplyBroadAndNarrowGenres();
    void apicalypseFilterCoversEveryMappedCanonicalGenre();
    void apicalypseFilterIsNulloptForGenresIgdbCannotExpress();
};

void TestIgdbGenreMap::mapsKnownGenreThemeAndGameModeLabels()
{
    // Genre.
    QCOMPARE(IgdbGenreMap::toCanonical("Role-playing (RPG)"), QStringLiteral("RPG"));
    QCOMPARE(IgdbGenreMap::toCanonical("Turn-based strategy (TBS)"), QStringLiteral("Strategy"));
    // Theme.
    QCOMPARE(IgdbGenreMap::toCanonical("Horror"), QStringLiteral("Horror"));
    QCOMPARE(IgdbGenreMap::toCanonical("Action"), QStringLiteral("Action"));
    // Game mode.
    QCOMPARE(IgdbGenreMap::toCanonical("Massively Multiplayer Online (MMO)"), QStringLiteral("Massively Multiplayer"));
    // Theme (a second one, distinct from "Action"/"Horror" tested above).
    QCOMPARE(IgdbGenreMap::toCanonical("Science Fiction"), QStringLiteral("Sci-Fi"));
    // Genre that was previously deliberately left unmapped.
    QCOMPARE(IgdbGenreMap::toCanonical("Visual Novel"), QStringLiteral("Visual Novel"));
    // Case-insensitive lookup, trimmed input.
    QCOMPARE(IgdbGenreMap::toCanonical("  shooter  "), QStringLiteral("Shooter"));
}

void TestIgdbGenreMap::passesThroughUnknownLabelsUnchanged()
{
    // IGDB genres with no canonical equivalent — see header comment.
    QCOMPARE(IgdbGenreMap::toCanonical("Non-fiction"), QStringLiteral("Non-fiction"));
    QCOMPARE(IgdbGenreMap::toCanonical("Kids"), QStringLiteral("Kids"));
}

void TestIgdbGenreMap::specificLabelsImplyBroadAndNarrowGenres()
{
    QCOMPARE(IgdbGenreMap::toCanonicalAll("Real Time Strategy (RTS)"),
             QStringList({"Strategy", "Real-Time Strategy"}));
    QCOMPARE(IgdbGenreMap::toCanonicalAll("Card & Board Game"), QStringList({"Card Game", "Board Game"}));
    // No narrower genre: just the one-to-one mapping.
    QCOMPARE(IgdbGenreMap::toCanonicalAll("Horror"), QStringList({"Horror"}));
    QCOMPARE(IgdbGenreMap::toCanonicalAll("Warfare"), QStringList({"War"}));
}

void TestIgdbGenreMap::apicalypseFilterCoversEveryMappedCanonicalGenre()
{
    const auto filter = IgdbGenreMap::apicalypseFilterFor(QStringLiteral("Strategy"));
    QVERIFY(filter.has_value());
    QVERIFY(filter->startsWith(QStringLiteral("genres.name = (")));
    QVERIFY(filter->contains(QStringLiteral("\"Strategy\"")));

    const auto horrorFilter = IgdbGenreMap::apicalypseFilterFor(QStringLiteral("Horror"));
    QVERIFY(horrorFilter.has_value());
    QCOMPARE(*horrorFilter, QStringLiteral("themes.name = (\"Horror\")"));
}

void TestIgdbGenreMap::apicalypseFilterIsNulloptForGenresIgdbCannotExpress()
{
    QVERIFY(!IgdbGenreMap::apicalypseFilterFor(QStringLiteral("Casual")).has_value());
    QVERIFY(!IgdbGenreMap::apicalypseFilterFor(QStringLiteral("Free To Play")).has_value());
    QVERIFY(!IgdbGenreMap::apicalypseFilterFor(QStringLiteral("Early Access")).has_value());
}

QTEST_MAIN(TestIgdbGenreMap)
#include "test_IgdbGenreMap.moc"
