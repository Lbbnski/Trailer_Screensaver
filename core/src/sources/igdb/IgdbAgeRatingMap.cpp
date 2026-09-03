#include "sources/igdb/IgdbAgeRatingMap.h"
#include "util/Logging.h"

#include <QHash>
#include <QPair>
#include <QRegularExpression>

namespace ssv {

namespace {

// Word-form ratings that don't carry their own digits (PEGI's category
// labels are spelled-out numbers; ESRB/CERO/ACB use named tiers). Keys are
// "<lower-cased organization>|<lower-cased rating label>". Numeric labels
// (USK "18", GRAC "15", CLASS_IND "14", or any label containing a bare
// number) are handled separately in minimumAge() without needing an entry
// here at all.
const QHash<QString, int>& wordRatings()
{
    static const QHash<QString, int> table{
        // PEGI: spelled-out age thresholds.
        {"pegi|three", 3},
        {"pegi|seven", 7},
        {"pegi|twelve", 12},
        {"pegi|sixteen", 16},
        {"pegi|eighteen", 18},
        // ESRB: named tiers, not ages.
        {"esrb|rating pending", 0},
        {"esrb|early childhood", 0},
        {"esrb|everyone", 0},
        {"esrb|everyone 10+", 10},
        {"esrb|teen", 13},
        {"esrb|mature", 17},
        {"esrb|adults only", 18},
        // CERO (Japan): lettered tiers.
        {"cero|a", 0},
        {"cero|b", 12},
        {"cero|c", 15},
        {"cero|d", 17},
        {"cero|z", 18},
        // ACB (Australia): named tiers.
        {"acb|general", 0},
        {"acb|parental guidance", 0},
        {"acb|mature", 15},
        {"acb|mature accompanied", 15},
        {"acb|restricted 18+", 18},
        // GRAC (Korea): "All" isn't numeric.
        {"grac|all", 0},
    };
    return table;
}

} // namespace

int IgdbAgeRatingMap::minimumAge(const QString& organizationName, const QString& ratingLabel)
{
    const QString org = organizationName.trimmed().toLower();
    const QString label = ratingLabel.trimmed();

    // Numeric labels (USK "18"/"16"/..., GRAC/CLASS_IND numeric tiers, or
    // any other board that reports a bare age) — take the first number in
    // the label directly rather than needing a per-organization entry.
    static const QRegularExpression digits(QStringLiteral("\\d+"));
    const auto match = digits.match(label);
    if (match.hasMatch())
        return match.captured().toInt();

    const auto it = wordRatings().find(QStringLiteral("%1|%2").arg(org, label.toLower()));
    if (it != wordRatings().end())
        return it.value();

    logWarning(QStringLiteral("igdb: unrecognized age rating \"%1\" (%2) — treating as no known minimum")
                   .arg(label, organizationName));
    return 0;
}

} // namespace ssv
