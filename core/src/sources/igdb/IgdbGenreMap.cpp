#include "sources/igdb/IgdbGenreMap.h"

#include <QHash>
#include <QPair>

namespace ssv {

namespace {

// IGDB splits what Steam lumps into one "genre" list across three separate
// vocabularies (Game.genres[], Game.themes[], Game.game_modes[]) — e.g.
// "Horror" and "Action" are IGDB *themes*, not genres, and "Massively
// Multiplayer" is a *game mode*. IgdbTrailerSource queries all three fields
// and runs every label through this same table, so this map doesn't care
// which vocabulary a label came from. Keys are lower-cased IGDB labels;
// values are canonical GenreTaxonomy names. "Casual", "Free To Play", and
// "Early Access" have no IGDB equivalent at all (IGDB doesn't model
// storefront business models), so IGDB candidates simply never carry those
// canonical genres — expected, not a bug.
const QHash<QString, QString>& table()
{
    static const QHash<QString, QString> t{
        // Game.genres[].name
        {"point-and-click", "Adventure"},
        {"fighting", "Fighting"},
        {"shooter", "Shooter"},
        {"platform", "Platformer"},
        {"puzzle", "Puzzle"},
        {"racing", "Racing"},
        {"real time strategy (rts)", "Strategy"},
        {"role-playing (rpg)", "RPG"},
        {"simulator", "Simulation"},
        {"sport", "Sports"},
        {"strategy", "Strategy"},
        {"turn-based strategy (tbs)", "Strategy"},
        {"tactical", "Strategy"},
        {"hack and slash/beat 'em up", "Action"},
        {"adventure", "Adventure"},
        {"indie", "Indie"},
        // Game.themes[].name
        {"action", "Action"},
        {"horror", "Horror"},
        // Game.game_modes[].name
        {"massively multiplayer online (mmo)", "Massively Multiplayer"},
    };
    return t;
}

} // namespace

QString IgdbGenreMap::toCanonical(const QString& igdbLabel)
{
    const QString trimmed = igdbLabel.trimmed();
    const auto it = table().find(trimmed.toLower());
    return it != table().end() ? it.value() : trimmed;
}

namespace {

// The reverse of table() above, grouped by which Apicalypse field each raw
// label lives under — kept as a second hand-maintained table rather than
// inverted from table() at runtime, since the grouping-by-field is exactly
// what apicalypseFilterFor() needs and a generic inversion would just have
// to reconstruct it anyway.
struct ReverseEntry {
    QString field;      // Apicalypse field, e.g. "genres.name"
    QStringList labels; // original-cased IGDB labels for this field
};

const QHash<QString, ReverseEntry>& reverseTable()
{
    static const QHash<QString, ReverseEntry> t{
        {"Action", {"genres.name", {"Hack and slash/Beat 'em up"}}}, // themes.name = "Action" also matches, but genres alone is enough to seed discovery
        {"Adventure", {"genres.name", {"Point-and-click", "Adventure"}}},
        {"Indie", {"genres.name", {"Indie"}}},
        {"RPG", {"genres.name", {"Role-playing (RPG)"}}},
        {"Simulation", {"genres.name", {"Simulator"}}},
        {"Strategy", {"genres.name", {"Real Time Strategy (RTS)", "Strategy", "Turn-based strategy (TBS)", "Tactical"}}},
        {"Sports", {"genres.name", {"Sport"}}},
        {"Racing", {"genres.name", {"Racing"}}},
        {"Horror", {"themes.name", {"Horror"}}},
        {"Puzzle", {"genres.name", {"Puzzle"}}},
        {"Platformer", {"genres.name", {"Platform"}}},
        {"Fighting", {"genres.name", {"Fighting"}}},
        {"Shooter", {"genres.name", {"Shooter"}}},
        {"Massively Multiplayer", {"game_modes.name", {"Massively Multiplayer Online (MMO)"}}},
        // "Casual", "Free To Play", "Early Access" deliberately absent —
        // no IGDB equivalent, see header comment.
    };
    return t;
}

} // namespace

std::optional<QString> IgdbGenreMap::apicalypseFilterFor(const QString& canonicalGenre)
{
    const auto it = reverseTable().find(canonicalGenre);
    if (it == reverseTable().end())
        return std::nullopt;

    QStringList quoted;
    for (const auto& label : it.value().labels)
        quoted << QStringLiteral("\"%1\"").arg(label);

    return QStringLiteral("%1 = (%2)").arg(it.value().field, quoted.join(QStringLiteral(",")));
}

} // namespace ssv
