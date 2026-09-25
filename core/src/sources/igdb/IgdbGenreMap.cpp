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
        // Game.themes[].name
        {"science fiction", "Sci-Fi"},
        {"fantasy", "Fantasy"},
        {"survival", "Survival"},
        {"stealth", "Stealth"},
        {"sandbox", "Sandbox"},
        // Game.genres[].name
        {"visual novel", "Visual Novel"},
        {"card & board game", "Card Game"},
        {"moba", "MOBA"},
        {"music", "Rhythm"},
        {"arcade", "Arcade"},
        {"pinball", "Arcade"},
        {"quiz/trivia", "Puzzle"},
        // Game.themes[].name
        {"historical", "Historical"},
        {"warfare", "War"},
        {"mystery", "Mystery"},
        {"comedy", "Comedy"},
        {"romance", "Romance"},
        {"open world", "Open World"},
        {"4x (explore, expand, exploit, and exterminate)", "4X"},
        {"party", "Party Game"},
        {"educational", "Educational"},
        {"business", "Management"},
        {"erotic", "Sexual Content"},
        // Game.game_modes[].name
        {"multiplayer", "Multiplayer"},
        {"co-operative", "Co-op"},
        {"split screen", "Multiplayer"},
        {"battle royale", "Battle Royale"},
    };
    return t;
}

// IGDB's broad labels that also imply a more specific canonical genre, so a
// game IGDB calls "Real Time Strategy (RTS)" carries both Strategy and
// Real-Time Strategy. Kept separate from table() so toCanonical() keeps its
// one-label-one-genre contract.
const QHash<QString, QStringList>& extraTable()
{
    static const QHash<QString, QStringList> t{
        {"point-and-click", {"Point & Click"}},
        {"real time strategy (rts)", {"Real-Time Strategy"}},
        {"turn-based strategy (tbs)", {"Turn-Based"}},
        {"tactical", {"Tactical"}},
        {"hack and slash/beat 'em up", {"Hack and Slash", "Beat 'em Up"}},
        {"card & board game", {"Board Game"}},
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

QStringList IgdbGenreMap::toCanonicalAll(const QString& igdbLabel)
{
    QStringList out{toCanonical(igdbLabel)};
    out << extraTable().value(igdbLabel.trimmed().toLower());
    return out;
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
        {"Sci-Fi", {"themes.name", {"Science Fiction"}}},
        {"Fantasy", {"themes.name", {"Fantasy"}}},
        {"Survival", {"themes.name", {"Survival"}}},
        {"Stealth", {"themes.name", {"Stealth"}}},
        {"Sandbox", {"themes.name", {"Sandbox"}}},
        {"Visual Novel", {"genres.name", {"Visual Novel"}}},
        {"Point & Click", {"genres.name", {"Point-and-click"}}},
        {"Real-Time Strategy", {"genres.name", {"Real Time Strategy (RTS)"}}},
        {"Turn-Based", {"genres.name", {"Turn-based strategy (TBS)"}}},
        {"Tactical", {"genres.name", {"Tactical"}}},
        {"Hack and Slash", {"genres.name", {"Hack and slash/Beat 'em up"}}},
        {"Beat 'em Up", {"genres.name", {"Hack and slash/Beat 'em up"}}},
        {"Card Game", {"genres.name", {"Card & Board Game"}}},
        {"Board Game", {"genres.name", {"Card & Board Game"}}},
        {"MOBA", {"genres.name", {"MOBA"}}},
        {"Rhythm", {"genres.name", {"Music"}}},
        {"Arcade", {"genres.name", {"Arcade", "Pinball"}}},
        {"Historical", {"themes.name", {"Historical"}}},
        {"War", {"themes.name", {"Warfare"}}},
        {"Mystery", {"themes.name", {"Mystery"}}},
        {"Comedy", {"themes.name", {"Comedy"}}},
        {"Romance", {"themes.name", {"Romance"}}},
        {"Open World", {"themes.name", {"Open world"}}},
        {"4X", {"themes.name", {"4X (explore, expand, exploit, and exterminate)"}}},
        {"Party Game", {"themes.name", {"Party"}}},
        {"Educational", {"themes.name", {"Educational"}}},
        {"Management", {"themes.name", {"Business"}}},
        {"Sexual Content", {"themes.name", {"Erotic"}}},
        {"Multiplayer", {"game_modes.name", {"Multiplayer", "Split screen"}}},
        {"Co-op", {"game_modes.name", {"Co-operative"}}},
        {"Battle Royale", {"game_modes.name", {"Battle Royale"}}},
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
