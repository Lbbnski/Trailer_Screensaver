#pragma once

#include <QHash>
#include <QString>
#include <optional>

namespace ssv {

// Steam's storefront actually exposes two different, non-overlapping
// vocabularies that both get colloquially called "genre":
//
//  1. appdetails' `genres[]` field — a small, official, controlled list
//     (Action, Adventure, Casual, Indie, RPG, Simulation, Strategy, Sports,
//     Racing, Massively Multiplayer, Free to Play, Early Access, plus a few
//     non-gameplay categories like Utilities). Each has a stable numeric id
//     also usable as the `genre=<id>` filter on store search.
//  2. Community/store *tags* (Horror, Shooter, Platformer, Fighting,
//     Puzzle, ...) — much larger and more useful for filtering, but not
//     returned by appdetails at all, and not exposed as a simple query
//     parameter with stable documented ids. This project reads tags via
//     SteamSpy's `request=tag` endpoint instead (see
//     SteamGenreCandidateFinder) rather than scraping store-page tag chips.
//
// GenreTaxonomy's canonical list mixes both groups because that's what
// reads naturally to a user picking genres to filter by, so this mapper
// exposes both: a numeric id for the official-genre subset (used to filter
// Steam's own search), and pass-through name normalization usable against
// either vocabulary (used to interpret what a fetched app's genres/tags
// actually are).
class SteamGenreMap {
public:
    // Normalizes a raw Steam genre or tag label (e.g. from appdetails'
    // genres[].description or a SteamSpy tag key) to a canonical
    // GenreTaxonomy name, handling known synonyms (e.g. "MMO" ->
    // "Massively Multiplayer"). Returns the input unchanged, trimmed, if no
    // known synonym applies — most Steam tag names already match a
    // canonical name 1:1.
    static QString toCanonical(const QString& steamLabel);

    // The numeric id for the *official genre facet* (group 1 above) that
    // corresponds to a canonical name, if one exists. Genres that only
    // exist as a Steam tag (Horror, Shooter, Platformer, Fighting, Puzzle)
    // return std::nullopt — discovery for those instead goes through
    // SteamSpy's tag endpoint.
    static std::optional<int> officialGenreIdFor(const QString& canonicalGenre);
};

} // namespace ssv
