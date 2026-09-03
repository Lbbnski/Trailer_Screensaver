#pragma once

#include <QString>
#include <optional>

namespace ssv {

// Maps GOG's genre vocabulary (the "genres" field returned by
// embed.gog.com/games/ajax/filtered, and the "category" query param that
// filters it) onto GenreTaxonomy's canonical list, mirroring
// SteamGenreMap/IgdbGenreMap's role for their sources.
//
// GOG's "genres" array observed in practice mixes actual genres
// ("Simulation", "Strategy") with theme-like tags ("Sci-fi") in one flat
// list — there's no separate genres/themes split like IGDB, and no
// facet-enumeration endpoint was found to confirm GOG's full genre
// vocabulary exhaustively. This table covers the well-established GOG
// storefront genre facets; anything else (thematic tags like "Sci-fi",
// "Fantasy", "Historical") simply passes through unmapped — it won't
// satisfy an allow-listed canonical genre, which is the same "don't crash,
// don't over-claim" behavior every other source's genre map uses for
// labels it doesn't recognize.
class GogGenreMap {
public:
    static QString toCanonical(const QString& gogLabel);

    // The GOG `category` query-param value that discovers `canonicalGenre`,
    // or std::nullopt if GOG has no facet for it at all (Free To Play/Early
    // Access are release-state/price filters on GOG, not genres — see
    // GogCandidateFinder). Unlike IgdbGenreMap::apicalypseFilterFor(), this
    // returns at most one label rather than an OR-list: GOG's `category`
    // param wasn't confirmed to accept multiple values, so each canonical
    // genre maps to its single closest GOG facet.
    static std::optional<QString> categoryParamFor(const QString& canonicalGenre);
};

} // namespace ssv
