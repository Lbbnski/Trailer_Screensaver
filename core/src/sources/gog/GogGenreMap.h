#pragma once

#include <QString>
#include <optional>

namespace ssv {

// Maps GOG's vocabularies onto GenreTaxonomy's canonical list, mirroring
// SteamGenreMap/IgdbGenreMap's role for their sources. GOG's catalog API
// (catalog.gog.com/v1/catalog) returns both a product's `genres[]` (a small
// fixed set: Action, Adventure, Racing, Role-playing, Shooter, Simulation,
// Sports, Strategy) and its `tags[]` (~200: Roguelike, Cyberpunk, Cozy, ...),
// and lists its full genre/tag vocabulary in the response's `filters`; both
// map through the same table here. A label with no canonical equivalent is
// simply not carried over (see canonicalIfKnown), so it can never satisfy a
// filter — same "don't crash, don't over-claim" rule as the other sources.
class GogGenreMap {
public:
    static QString toCanonical(const QString& gogLabel);

    // Like toCanonical(), but std::nullopt for a genre/tag with no
    // canonical equivalent, so a product's several dozen tags don't all
    // end up stored as "genres".
    static std::optional<QString> canonicalIfKnown(const QString& gogLabel);

    // How catalog.gog.com lists a canonical genre: `<param>=in:<slug>`,
    // where param is "genres" (GOG's eight top-level genres) or "tags".
    struct CatalogFilter {
        QString param;
        QString slug;
    };

    // The filter that discovers `canonicalGenre`, or std::nullopt if GOG has
    // no genre or tag for it (Massively Multiplayer, Early Access, ...).
    static std::optional<CatalogFilter> catalogFilterFor(const QString& canonicalGenre);
};

} // namespace ssv
