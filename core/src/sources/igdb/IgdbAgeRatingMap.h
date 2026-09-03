#pragma once

#include <QString>

namespace ssv {

// Normalizes one IGDB AgeRating entry to a minimum-age int, mirroring
// TrailerCandidate::ageRating's meaning for Steam's required_age. IGDB
// exposes several regional rating boards at once (ESRB, PEGI, USK, CERO,
// ACB, GRAC, CLASS_IND) rather than Steam's single US-centric field;
// IgdbTrailerSource takes the max across all of a game's age_ratings[] so
// the strictest applicable rating always wins.
class IgdbAgeRatingMap {
public:
    // `organizationName` is AgeRating.organization.name (e.g. "ESRB",
    // "PEGI", "USK"); `ratingLabel` is AgeRating.rating_category's own
    // AgeRatingCategory.rating string (e.g. "Mature", "Eighteen", "18") —
    // deliberately keyed off this human-readable adjective rather than any
    // numeric category id, since IGDB has already migrated the rating
    // representation from an inline numeric enum to this reference-table
    // string once before and could again; a label is far more stable than
    // an id across such a migration.
    //
    // Returns 0 ("no known minimum") for any organization/label this table
    // doesn't recognize — including if a future IGDB schema change alters
    // the label text — so an unrecognized rating never wrongly excludes a
    // game, it just fails to contribute to the age filter. This is the same
    // "unknown passes through, never crashes or over-filters" behavior as
    // SteamAppDetailsClient's contentDescriptorName() fallback.
    static int minimumAge(const QString& organizationName, const QString& ratingLabel);
};

} // namespace ssv
