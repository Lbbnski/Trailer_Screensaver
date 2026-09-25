#include "sources/GenreTaxonomy.h"
#include "sources/steam/SteamGenreMap.h"

#include <QtTest/QtTest>

using namespace ssv;

class TestSteamGenreMap : public QObject {
    Q_OBJECT
private slots:
    void mapsOfficialGenresAndTags();
    void canonicalIfKnownRejectsUnknownLabels();
    void tagIdsRoundTrip();
    void everyCanonicalGenreCanBeDiscovered();
    void everyMappedTagLandsOnACanonicalGenre();
};

void TestSteamGenreMap::mapsOfficialGenresAndTags()
{
    QCOMPARE(SteamGenreMap::toCanonical("MMO"), QStringLiteral("Massively Multiplayer"));
    QCOMPARE(SteamGenreMap::toCanonical("Free to Play"), QStringLiteral("Free To Play"));
    QCOMPARE(SteamGenreMap::toCanonical("Sci-fi"), QStringLiteral("Sci-Fi"));
    QCOMPARE(SteamGenreMap::toCanonical("Roguelite"), QStringLiteral("Roguelike"));
    QCOMPARE(SteamGenreMap::toCanonical("Souls-like"), QStringLiteral("Souls-like"));
    // Unknown labels pass through unchanged.
    QCOMPARE(SteamGenreMap::toCanonical("Utilities"), QStringLiteral("Utilities"));
}

void TestSteamGenreMap::canonicalIfKnownRejectsUnknownLabels()
{
    QVERIFY(!SteamGenreMap::canonicalIfKnown("Great Soundtrack").has_value());
    QVERIFY(!SteamGenreMap::canonicalIfKnown("Controller").has_value());
    QCOMPARE(*SteamGenreMap::canonicalIfKnown("Cyberpunk"), QStringLiteral("Cyberpunk"));
}

void TestSteamGenreMap::tagIdsRoundTrip()
{
    // Roguelike is searched by its primary tag (1716); the other Steam tags
    // folded into it resolve back to the same canonical genre.
    QCOMPARE(*SteamGenreMap::tagIdFor("Roguelike"), 1716);
    QCOMPARE(*SteamGenreMap::canonicalForTagId(1716), QStringLiteral("Roguelike"));
    QCOMPARE(*SteamGenreMap::canonicalForTagId(3959), QStringLiteral("Roguelike")); // Roguelite
    QVERIFY(!SteamGenreMap::canonicalForTagId(1756).has_value());                    // Great Soundtrack
}

void TestSteamGenreMap::everyCanonicalGenreCanBeDiscovered()
{
    // Each genre must be reachable by an official genre id or a tag id,
    // otherwise selecting it in the settings dialog could never find games.
    for (const auto& genre : GenreTaxonomy::canonicalGenres()) {
        QVERIFY2(SteamGenreMap::officialGenreIdFor(genre).has_value() || SteamGenreMap::tagIdFor(genre).has_value(),
                 qPrintable(genre));
    }
}

void TestSteamGenreMap::everyMappedTagLandsOnACanonicalGenre()
{
    for (const auto& genre : GenreTaxonomy::canonicalGenres()) {
        const auto id = SteamGenreMap::tagIdFor(genre);
        if (!id)
            continue;
        const auto back = SteamGenreMap::canonicalForTagId(*id);
        QVERIFY2(back.has_value() && GenreTaxonomy::isCanonical(*back), qPrintable(genre));
    }
}

QTEST_MAIN(TestSteamGenreMap)
#include "test_SteamGenreMap.moc"
