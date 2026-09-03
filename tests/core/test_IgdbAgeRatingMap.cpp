#include "sources/igdb/IgdbAgeRatingMap.h"

#include <QtTest/QtTest>

using namespace ssv;

class TestIgdbAgeRatingMap : public QObject {
    Q_OBJECT
private slots:
    void parsesNumericLabelsDirectly();
    void mapsKnownWordFormLabels();
    void unknownLabelYieldsZeroNotAFailure();
};

void TestIgdbAgeRatingMap::parsesNumericLabelsDirectly()
{
    // USK, GRAC, CLASS_IND report the age itself as the label text.
    QCOMPARE(IgdbAgeRatingMap::minimumAge("USK", "18"), 18);
    QCOMPARE(IgdbAgeRatingMap::minimumAge("USK", "6"), 6);
    QCOMPARE(IgdbAgeRatingMap::minimumAge("GRAC", "15"), 15);
}

void TestIgdbAgeRatingMap::mapsKnownWordFormLabels()
{
    QCOMPARE(IgdbAgeRatingMap::minimumAge("PEGI", "Eighteen"), 18);
    QCOMPARE(IgdbAgeRatingMap::minimumAge("PEGI", "Three"), 3);
    QCOMPARE(IgdbAgeRatingMap::minimumAge("ESRB", "Mature"), 17);
    QCOMPARE(IgdbAgeRatingMap::minimumAge("ESRB", "Adults Only"), 18);
    QCOMPARE(IgdbAgeRatingMap::minimumAge("ESRB", "Everyone"), 0);
    // Case-insensitive on the organization name.
    QCOMPARE(IgdbAgeRatingMap::minimumAge("esrb", "Teen"), 13);
}

void TestIgdbAgeRatingMap::unknownLabelYieldsZeroNotAFailure()
{
    // A future IGDB schema/wording change must degrade to "unknown", never
    // crash or silently over-filter.
    QCOMPARE(IgdbAgeRatingMap::minimumAge("ESRB", "Some Future Label"), 0);
    QCOMPARE(IgdbAgeRatingMap::minimumAge("SomeNewBoard", "Whatever"), 0);
}

QTEST_MAIN(TestIgdbAgeRatingMap)
#include "test_IgdbAgeRatingMap.moc"
