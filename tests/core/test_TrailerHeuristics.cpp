#include "sources/steam/TrailerHeuristics.h"

#include <QtTest/QtTest>

using namespace ssv::TrailerHeuristics;

// Every case here is a real video title / game pair from a user's bad-trailer
// reports or diagnostics log (see docs/LIMITATIONS.md), not an invented
// example.
class TestTrailerHeuristics : public QObject {
    Q_OBJECT
private slots:
    void shortTitleMustMatchInFull();
    void punctuationIsIgnoredWhenMatching();
    void longerTitlesOnlyNeedHalfTheirWords();
    void trailerWordRequired();
    void nonTrailerWordsRejected();
    void nonTrailerWordInGameTitleIsIgnored();
    void movieWordsInTitle();
    void filmCategories();
    void movieSignalsInDescription();
    void gameContextOnlyRequiredForKnownNonGamingCategory();
};

void TestTrailerHeuristics::shortTitleMustMatchInFull()
{
    // Reported: "Gun Beat" played the trailer for the movie Top Gun: Maverick
    // — one shared word out of two used to be enough.
    QVERIFY(!titleMatchesGame("Gun Beat", "Top Gun: Maverick | Official Trailer (2022 Movie)"));
    QVERIFY(titleMatchesGame("Gun Beat", "Gun Beat - Official Trailer"));
    QVERIFY(!titleMatchesGame("Arsonist", "Some Other Film Trailer"));
    QVERIFY(titleMatchesGame("Arsonist", "ARSONIST - Announcement Trailer"));
}

void TestTrailerHeuristics::punctuationIsIgnoredWhenMatching()
{
    QVERIFY(titleMatchesGame("Freefall 3050AD", "Freefall 3050 A.D. Opening Cinematic"));
    QVERIFY(titleMatchesGame("Spider-Man", "Marvel's Spider Man - Launch Trailer"));
}

void TestTrailerHeuristics::longerTitlesOnlyNeedHalfTheirWords()
{
    // A store entry's edition suffix ("Demo", "Soundtrack") isn't in the
    // real trailer's title.
    QVERIFY(titleMatchesGame("3 Minutes to Midnight Demo", "3 Minutes to Midnight - Official Reveal Trailer"));
    QVERIFY(!titleMatchesGame("The Long Dark Survival Game", "Totally Unrelated Cooking Show Trailer"));
}

void TestTrailerHeuristics::trailerWordRequired()
{
    QVERIFY(hasTrailerWord("Agartha Official Trailer"));
    QVERIFY(hasTrailerWord("Project Zomboid Steam Announcement Trailer HD"));
    QVERIFY(hasTrailerWord("Redviil (Русский трейлер)"));
    QVERIFY(hasTrailerWord("東方紅魔郷 予告"));
    QVERIFY(hasTrailerWord("Red Dead Redemption 2: Official Gameplay Video"));
    // Streamer/fan/OST uploads that merely mention the game:
    QVERIFY(!hasTrailerWord("[Gameplay] The ScreaMaze - UE4"));
    QVERIFY(!hasTrailerWord("Zibbs - Alien Survival Gameplay First Look"));
    QVERIFY(!hasTrailerWord("Mirror OST - Yoko"));
}

void TestTrailerHeuristics::nonTrailerWordsRejected()
{
    // Reported as "Game dev talk": a trailer *about* a trailer.
    QCOMPARE(nonTrailerWord("Halo Infinite", "343 Unpacks E3 2019 Halo Infinite Trailer"), QStringLiteral("Unpacks"));
    QVERIFY(!nonTrailerWord("Where the Bees Make Honey", "Where the Bees Make Honey Review").isEmpty());
    QVERIFY(!nonTrailerWord("Some Game", "Some Game Trailer REACTION").isEmpty());
    QVERIFY(nonTrailerWord("Halo Infinite", "Halo Infinite - Official Launch Trailer").isEmpty());
}

void TestTrailerHeuristics::nonTrailerWordInGameTitleIsIgnored()
{
    QVERIFY(nonTrailerWord("Review Board: The Game", "Review Board: The Game - Official Trailer").isEmpty());
    QVERIFY(!nonTrailerWord("Other Game", "Other Game - Official Trailer Review").isEmpty());
}

void TestTrailerHeuristics::movieWordsInTitle()
{
    QVERIFY(!movieWordInTitle("Arsonist", "Arsonist (2019) Official Movie Trailer").isEmpty());
    QVERIFY(!movieWordInTitle("Some Game", "Some Game - TV Series Trailer Season 2").isEmpty());
    QVERIFY(movieWordInTitle("Film Noir Detective", "Film Noir Detective - Official Trailer").isEmpty());
    QVERIFY(movieWordInTitle("Some Game", "Some Game - Launch Trailer").isEmpty());
}

void TestTrailerHeuristics::filmCategories()
{
    QVERIFY(isFilmCategory({"Film & Animation"}));
    QVERIFY(isFilmCategory({"Movies"}));
    QVERIFY(isFilmCategory({"Shows"}));
    QVERIFY(!isFilmCategory({"Gaming"}));
    QVERIFY(!isFilmCategory({"Entertainment"}));
    QVERIFY(!isFilmCategory({}));
}

void TestTrailerHeuristics::movieSignalsInDescription()
{
    QCOMPARE(movieSignalCount("In theaters everywhere. Watch on Netflix."), 2);
    QCOMPARE(movieSignalCount("Official movie trailer from Warner Bros. Pictures"), 2);
    // A real game trailer whose description credits a voice cast must not
    // trip this: "starring" was deliberately left out of the signals after
    // it fired on Freedom Finger's genuine announcement trailer.
    QCOMPARE(movieSignalCount("Freedom Finger — starring a cast of aliens. Wishlist on Steam!"), 0);
}

void TestTrailerHeuristics::gameContextOnlyRequiredForKnownNonGamingCategory()
{
    QVERIFY(!lacksGameContext({"Gaming"}, "nothing about games here"));
    QVERIFY(!lacksGameContext({}, "unknown category is never evidence"));
    QVERIFY(lacksGameContext({"People & Blogs"}, "my vlog about cooking pasta"));
    QVERIFY(!lacksGameContext({"People & Blogs"}, "Out now on Steam and PlayStation 5"));
}

QTEST_MAIN(TestTrailerHeuristics)
#include "test_TrailerHeuristics.moc"
