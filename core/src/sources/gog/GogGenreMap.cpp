#include "sources/gog/GogGenreMap.h"

#include <QHash>

namespace ssv {

namespace {

// Keys are lower-cased GOG genre labels; values are canonical names.
const QHash<QString, QString>& toCanonicalTable()
{
    static const QHash<QString, QString> t{
        {"action", "Action"},
        {"adventure", "Adventure"},
        {"casual", "Casual"},
        {"indie", "Indie"},
        {"rpg", "RPG"},
        {"simulation", "Simulation"},
        {"strategy", "Strategy"},
        {"sports", "Sports"},
        {"racing", "Racing"},
        {"horror", "Horror"},
        {"puzzle", "Puzzle"},
        {"platformer", "Platformer"},
        {"fighting", "Fighting"},
        {"shooter", "Shooter"},
        {"massively multiplayer", "Massively Multiplayer"},
        {"sci-fi", "Sci-Fi"},
        {"fantasy", "Fantasy"},
        {"survival", "Survival"},
        {"stealth", "Stealth"},
        {"sandbox", "Sandbox"},
        {"visual novel", "Visual Novel"},
    };
    return t;
}

// The reverse direction — canonical name to the GOG `category` facet value
// used to filter the listing endpoint.
//
// Every value below was verified live against embed.gog.com/games/ajax/filtered
// on 2026-09-07: the endpoint returns HTTP 500 (not an empty/graceful
// result) for ANY `category` value it doesn't recognize, so a wrong guess
// here doesn't just fail to filter — GogCandidateFinder::fetchPage()
// silently gets nothing at all for it. The original table (title-cased,
// e.g. "RPG", "Massively Multiplayer") was wrong on both counts: GOG's
// category facet values are lowercase, and several genres this taxonomy
// has simply have no equivalent facet on GOG at all (confirmed 500 under
// several plausible slug guesses each, not just a casing mismatch) —
// mapping those would only ever waste a discovery request. Left unmapped
// (falls through to categoryParamFor() returning nullopt, i.e. "nothing to
// discover" — the existing graceful path) rather than guessed: Casual,
// Horror, Puzzle, Platformer, Fighting, Massively Multiplayer, Sci-Fi,
// Fantasy, Survival, Stealth, Sandbox, Visual Novel. GOG's category
// taxonomy could of course change or turn out to have a facet for these
// under some other slug — if you find one, verify it returns 200 with real
// products before adding it back.
const QHash<QString, QString>& categoryParamTable()
{
    static const QHash<QString, QString> t{
        {"Action", "action"},
        {"Adventure", "adventure"},
        {"Indie", "indie"},
        {"RPG", "role-playing"},
        {"Simulation", "simulation"},
        {"Strategy", "strategy"},
        {"Sports", "sports"},
        {"Racing", "racing"},
        {"Shooter", "shooter"},
        // "Free To Play", "Early Access" deliberately absent: GOG expresses
        // "free" via the `price` filter and "in development" via the
        // `release` filter, neither of which is a genre facet.
    };
    return t;
}

} // namespace

QString GogGenreMap::toCanonical(const QString& gogLabel)
{
    const QString trimmed = gogLabel.trimmed();
    const auto it = toCanonicalTable().find(trimmed.toLower());
    return it != toCanonicalTable().end() ? it.value() : trimmed;
}

std::optional<QString> GogGenreMap::categoryParamFor(const QString& canonicalGenre)
{
    const auto it = categoryParamTable().find(canonicalGenre);
    if (it == categoryParamTable().end())
        return std::nullopt;
    return it.value();
}

} // namespace ssv
