#pragma once

#include <QHash>
#include <QList>
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
//     returned by appdetails at all. Each has a numeric tag id, listed at
//     store.steampowered.com/tagdata/populartags/english, usable as the
//     `tags=<id>` filter on store search, and an app's own tag ids come
//     from IStoreBrowseService/GetItems (see SteamAppDetailsClient).
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
    // genres[].description or a tag name) to a canonical
    // GenreTaxonomy name, handling known synonyms (e.g. "MMO" ->
    // "Massively Multiplayer"). Returns the input unchanged, trimmed, if no
    // known synonym applies — most Steam tag names already match a
    // canonical name 1:1.
    static QString toCanonical(const QString& steamLabel);

    // Like toCanonical(), but std::nullopt for a label that has no canonical
    // equivalent — used when reading an app's whole tag list, where passing
    // every unmapped tag ("Great Soundtrack", "Controller", ...) through as a
    // "genre" would only bloat the cache.
    static std::optional<QString> canonicalIfKnown(const QString& steamLabel);

    // The numeric id for the *official genre facet* (group 1 above) that
    // corresponds to a canonical name, if one exists.
    static std::optional<int> officialGenreIdFor(const QString& canonicalGenre);

    // The Steam user-tag id a canonical genre is searched by (store search's
    // `tags=` filter) — the first of the Steam tags that fold into it.
    // std::nullopt for a genre Steam has no tag for.
    static std::optional<int> tagIdFor(const QString& canonicalGenre);

    // The canonical genre a Steam user-tag id belongs to, or std::nullopt
    // for a tag with no canonical equivalent. Used to turn an app's tag ids
    // (IStoreBrowseService) into canonical genres.
    static std::optional<QString> canonicalForTagId(int tagId);
};

} // namespace ssv
