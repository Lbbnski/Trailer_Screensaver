#pragma once

#include <QString>
#include <optional>

namespace ssv {

// Maps IGDB's genre vocabulary (Game.genres[].name) onto GenreTaxonomy's
// canonical list, mirroring SteamGenreMap's role for the Steam source.
class IgdbGenreMap {
public:
    // Returns the canonical genre name for `igdbLabel`, or the trimmed
    // label unchanged if IGDB has no equivalent in GenreTaxonomy (a handful
    // of IGDB genres — Visual Novel, Card & Board Game, MOBA, Quiz/Trivia,
    // Pinball — have no canonical match; they simply won't satisfy any
    // allow-listed genre filter, same "falls through, doesn't crash"
    // behavior as SteamGenreMap::toCanonical() for its own unmapped labels).
    static QString toCanonical(const QString& igdbLabel);

    // The reverse direction, used by IgdbCandidateFinder to build a
    // discovery query's `where` clause: an Apicalypse filter fragment (e.g.
    // `genres.name = ("Real Time Strategy (RTS)","Strategy")`) that selects
    // games IGDB would tag with `canonicalGenre` once mapped through
    // toCanonical(). Returns std::nullopt for a canonical genre with no
    // IGDB equivalent at all (Casual, Free To Play, Early Access — IGDB has
    // no storefront-business-model concept) — callers must treat that as
    // "IGDB can't discover this genre", not an error; other enabled sources
    // (e.g. Steam) still cover it.
    static std::optional<QString> apicalypseFilterFor(const QString& canonicalGenre);
};

} // namespace ssv
