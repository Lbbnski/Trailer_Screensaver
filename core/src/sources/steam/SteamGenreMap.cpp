#include "sources/steam/SteamGenreMap.h"

namespace ssv {

namespace {

// Steam's numeric genre ids below are widely observed but undocumented and
// could shift — SteamGenreCandidateFinder treats a failed/empty search
// result as "nothing found" rather than trusting these blindly.
const QHash<QString, int>& officialGenreIds()
{
    static const QHash<QString, int> table{
        {"Action", 1},
        {"Strategy", 2},
        {"RPG", 3},
        {"Casual", 4},
        {"Racing", 9},
        {"Sports", 18},
        {"Indie", 23},
        {"Adventure", 25},
        {"Simulation", 28},
        {"Massively Multiplayer", 29},
        {"Free To Play", 37},
        {"Early Access", 70},
    };
    return table;
}

// Steam user-tag id -> canonical genre. Ids come from
// store.steampowered.com/tagdata/populartags/english; the tag marked
// "primary" is the one a canonical genre is searched by (search's `tags=`
// filter), the others only matter when reading an app's own tag list. Many
// Steam tags fold into one canonical genre (e.g. Roguelite -> Roguelike).
struct TagEntry {
    int id;
    const char* canonical;
};

const QList<TagEntry>& tagTable()
{
    static const QList<TagEntry> table{
        {19, "Action"},  // primary
        {21, "Adventure"},  // primary
        {597, "Casual"},  // primary
        {492, "Indie"},  // primary
        {122, "RPG"},  // primary
        {599, "Simulation"},  // primary
        {9, "Strategy"},  // primary
        {701, "Sports"},  // primary
        {7038, "Sports"},
        {5914, "Sports"},
        {22955, "Sports"},
        {1753, "Sports"},
        {28444, "Sports"},
        {7309, "Sports"},
        {7328, "Sports"},
        {4852, "Sports"},
        {12190, "Sports"},
        {252854, "Sports"},
        {19568, "Sports"},
        {13382, "Sports"},
        {699, "Racing"},  // primary
        {1644, "Racing"},
        {1100687, "Racing"},
        {4102, "Racing"},
        {15868, "Racing"},
        {7622, "Racing"},
        {1667, "Horror"},  // primary
        {1721, "Horror"},
        {3978, "Horror"},
        {42089, "Horror"},
        {1664, "Puzzle"},  // primary
        {6129, "Puzzle"},
        {1730, "Puzzle"},
        {4400, "Puzzle"},
        {24003, "Puzzle"},
        {10437, "Puzzle"},
        {1625, "Platformer"},  // primary
        {5379, "Platformer"},
        {5395, "Platformer"},
        {5537, "Platformer"},
        {3877, "Platformer"},
        {5652, "Platformer"},
        {1743, "Fighting"},  // primary
        {4736, "Fighting"},
        {6506, "Fighting"},
        {47827, "Fighting"},
        {1774, "Shooter"},  // primary
        {5547, "Shooter"},
        {4758, "Shooter"},
        {4637, "Shooter"},
        {56690, "Shooter"},
        {620519, "Shooter"},
        {1199779, "Shooter"},
        {353880, "Shooter"},
        {128, "Massively Multiplayer"},  // primary
        {1754, "Massively Multiplayer"},
        {113, "Free To Play"},  // primary
        {493, "Early Access"},  // primary
        {3942, "Sci-Fi"},  // primary
        {4295, "Sci-Fi"},
        {1673, "Sci-Fi"},
        {1684, "Fantasy"},  // primary
        {4604, "Fantasy"},
        {4057, "Fantasy"},
        {4046, "Fantasy"},
        {6054, "Fantasy"},
        {4535, "Fantasy"},
        {1662, "Survival"},  // primary
        {1100689, "Survival"},
        {1687, "Stealth"},  // primary
        {1776, "Stealth"},
        {97070, "Stealth"},
        {3810, "Sandbox"},  // primary
        {3799, "Visual Novel"},  // primary
        {31579, "Visual Novel"},
        {1716, "Roguelike"},  // primary
        {3959, "Roguelike"},
        {42804, "Roguelike"},
        {454187, "Roguelike"},
        {1091588, "Roguelike"},
        {198631, "Roguelike"},
        {1628, "Metroidvania"},  // primary
        {29482, "Souls-like"},  // primary
        {1646, "Hack and Slash"},  // primary
        {3955, "Hack and Slash"},
        {4777, "Hack and Slash"},
        {4158, "Beat 'em Up"},  // primary
        {4255, "Shoot 'em Up"},  // primary
        {4885, "Shoot 'em Up"},
        {723991, "Shoot 'em Up"},
        {1663, "First-Person Shooter"},  // primary
        {1023537, "First-Person Shooter"},
        {3814, "Third-Person Shooter"},  // primary
        {176981, "Battle Royale"},  // primary
        {1645, "Tower Defense"},  // primary
        {4328, "City Builder"},  // primary
        {220585, "City Builder"},
        {12472, "Management"},  // primary
        {8945, "Management"},
        {16689, "Management"},
        {4695, "Management"},
        {4845, "Management"},
        {91114, "Management"},
        {35079, "Management"},
        {1702, "Crafting"},  // primary
        {5981, "Crafting"},
        {7332, "Base Building"},  // primary
        {1643, "Base Building"},
        {255534, "Base Building"},
        {1677, "Turn-Based"},  // primary
        {1741, "Turn-Based"},
        {4325, "Turn-Based"},
        {14139, "Turn-Based"},
        {1676, "Real-Time Strategy"},  // primary
        {4161, "Real-Time Strategy"},
        {3813, "Real-Time Strategy"},
        {1723, "Real-Time Strategy"},
        {7107, "Real-Time Strategy"},
        {4364, "Grand Strategy"},  // primary
        {4684, "Grand Strategy"},
        {6310, "Grand Strategy"},
        {26921, "Grand Strategy"},
        {4598, "Grand Strategy"},
        {1670, "4X"},  // primary
        {1708, "Tactical"},  // primary
        {4434, "JRPG"},  // primary
        {4474, "CRPG"},  // primary
        {10695, "CRPG"},
        {4231, "Action RPG"},  // primary
        {21725, "Tactical RPG"},  // primary
        {17305, "Tactical RPG"},
        {1720, "Dungeon Crawler"},  // primary
        {1666, "Card Game"},  // primary
        {32322, "Card Game"},
        {791774, "Card Game"},
        {9271, "Card Game"},
        {1770, "Board Game"},  // primary
        {17389, "Board Game"},
        {4184, "Board Game"},
        {7556, "Board Game"},
        {33572, "Board Game"},
        {13070, "Board Game"},
        {1698, "Point & Click"},  // primary
        {1738, "Hidden Object"},  // primary
        {5900, "Walking Simulator"},  // primary
        {1752, "Rhythm"},  // primary
        {1621, "Rhythm"},
        {8253, "Rhythm"},
        {7178, "Party Game"},  // primary
        {7108, "Party Game"},
        {8093, "Party Game"},
        {87918, "Farming"},  // primary
        {4520, "Farming"},
        {22602, "Farming"},
        {10235, "Life Sim"},  // primary
        {1220528, "Life Sim"},
        {3920, "Life Sim"},
        {9551, "Dating Sim"},  // primary
        {15564, "Fishing"},  // primary
        {3968, "Physics"},  // primary
        {5363, "Physics"},
        {1665, "Match 3"},  // primary
        {31275, "Text-Based"},  // primary
        {560542, "Incremental"},  // primary
        {615955, "Incremental"},
        {916648, "Creature Collector"},  // primary
        {1773, "Arcade"},  // primary
        {5154, "Arcade"},
        {8666, "Arcade"},
        {1718, "MOBA"},  // primary
        {5300, "God Game"},  // primary
        {9204, "Immersive Sim"},  // primary
        {18594, "FMV"},  // primary
        {1746, "Team Sports"},  // primary
        {1254546, "Team Sports"},
        {5727, "Team Sports"},
        {324176, "Team Sports"},
        {1254552, "Team Sports"},
        {49213, "Team Sports"},
        {158638, "Team Sports"},
        {847164, "Team Sports"},
        {15045, "Flight"},  // primary
        {1616, "Trains"},  // primary
        {10383, "Trains"},
        {6910, "Naval"},  // primary
        {4994, "Naval"},
        {13577, "Naval"},
        {13276, "Tanks"},  // primary
        {1685, "Co-op"},  // primary
        {3843, "Co-op"},
        {3841, "Co-op"},
        {4508, "Co-op"},
        {4840, "Co-op"},
        {3859, "Multiplayer"},  // primary
        {1775, "Multiplayer"},
        {17770, "Multiplayer"},
        {10816, "Multiplayer"},
        {7368, "Multiplayer"},
        {5711, "Multiplayer"},
        {3878, "Multiplayer"},
        {21978, "VR"},  // primary
        {856791, "VR"},
        {1695, "Open World"},  // primary
        {3834, "Exploration"},  // primary
        {1742, "Story Rich"},  // primary
        {7702, "Story Rich"},
        {6426, "Story Rich"},
        {11014, "Story Rich"},
        {4486, "Story Rich"},
        {42152, "Story Rich"},
        {6971, "Story Rich"},
        {5716, "Mystery"},  // primary
        {5613, "Mystery"},
        {8369, "Mystery"},
        {6052, "Mystery"},
        {5372, "Mystery"},
        {6378, "Crime"},  // primary
        {1680, "Crime"},
        {4115, "Cyberpunk"},  // primary
        {4137, "Cyberpunk"},
        {3835, "Post-Apocalyptic"},  // primary
        {5030, "Post-Apocalyptic"},
        {1755, "Space"},  // primary
        {16598, "Space"},
        {4291, "Space"},
        {6702, "Space"},
        {4172, "Medieval"},  // primary
        {3987, "Historical"},  // primary
        {6948, "Historical"},
        {5179, "Historical"},
        {1678, "War"},  // primary
        {4168, "War"},
        {4150, "War"},
        {5382, "War"},
        {1647, "Western"},  // primary
        {1681, "Pirates"},  // primary
        {1659, "Zombies"},  // primary
        {1100686, "Zombies"},
        {10808, "Supernatural"},  // primary
        {12686, "Supernatural"},
        {17015, "Supernatural"},
        {9541, "Supernatural"},
        {52406, "Supernatural"},
        {7432, "Lovecraftian"},  // primary
        {1777, "Steampunk"},  // primary
        {1671, "Superhero"},  // primary
        {4085, "Anime"},  // primary
        {16094, "Mythology"},  // primary
        {11634, "Mythology"},
        {5752, "Robots"},  // primary
        {4821, "Robots"},
        {5160, "Dinosaurs"},  // primary
        {9626, "Animals"},  // primary
        {17894, "Animals"},
        {1637, "Animals"},
        {6041, "Animals"},
        {6214, "Animals"},
        {507423, "Animals"},
        {20486, "Animals"},
        {1352486, "Animals"},
        {30358, "Nature"},  // primary
        {9803, "Nature"},
        {9157, "Underwater"},  // primary
        {19780, "Underwater"},
        {6915, "Martial Arts"},  // primary
        {1688, "Martial Arts"},
        {10617, "Martial Arts"},
        {25959, "Martial Arts"},
        {760247, "Martial Arts"},
        {4608, "Martial Arts"},
        {10679, "Time Travel"},  // primary
        {6625, "Time Travel"},
        {4947, "Romance"},  // primary
        {1036, "Educational"},  // primary
        {21635, "Educational"},
        {5432, "Educational"},
        {1674, "Educational"},
        {71389, "Educational"},
        {1719, "Comedy"},  // primary
        {4136, "Comedy"},
        {5923, "Comedy"},
        {1651, "Comedy"},
        {4878, "Comedy"},
        {19995, "Comedy"},
        {10397, "Comedy"},
        {97376, "Cozy"},  // primary
        {552282, "Cozy"},
        {1654, "Cozy"},
        {4342, "Dark"},  // primary
        {3952, "Dark"},
        {4166, "Atmospheric"},  // primary
        {3934, "Atmospheric"},
        {5411, "Atmospheric"},
        {4026, "Difficult"},  // primary
        {7208, "Female Protagonist"},  // primary
        {44868, "LGBTQ+"},  // primary
        {5350, "Family Friendly"},  // primary
        {4726, "Family Friendly"},
        {3871, "2D"},  // primary
        {4191, "3D"},  // primary
        {3839, "First-Person"},  // primary
        {1697, "Third-Person"},  // primary
        {4791, "Top-Down"},  // primary
        {5851, "Isometric"},  // primary
        {3798, "Side Scroller"},  // primary
        {3964, "Pixel Graphics"},  // primary
        {4004, "Retro"},  // primary
        {3916, "Retro"},
        {7743, "Retro"},
        {6691, "Retro"},
        {14720, "Retro"},
        {1693, "Retro"},
        {4195, "Cartoony"},  // primary
        {4562, "Cartoony"},
        {1751, "Cartoony"},
        {4175, "Realistic"},  // primary
        {4345, "Gore"},  // primary
        {4667, "Violent"},  // primary
        {6650, "Nudity"},  // primary
        {12095, "Sexual Content"},  // primary
        {9130, "Sexual Content"},
    };
    return table;
}

// Lower-cased Steam label (appdetails genre description or tag name) ->
// canonical genre.
const QHash<QString, QString>& labelTable()
{
    static const QHash<QString, QString> table{
        {"mmo", "Massively Multiplayer"},
        {"1980s", "Retro"},
        {"1990's", "Retro"},
        {"2d", "2D"},
        {"2d fighter", "Fighting"},
        {"2d platformer", "Platformer"},
        {"3d", "3D"},
        {"3d fighter", "Fighting"},
        {"3d platformer", "Platformer"},
        {"4 player local", "Co-op"},
        {"4x", "4X"},
        {"abstract", "Puzzle"},
        {"action", "Action"},
        {"action roguelike", "Roguelike"},
        {"action rpg", "Action RPG"},
        {"action rts", "Real-Time Strategy"},
        {"adventure", "Adventure"},
        {"agriculture", "Farming"},
        {"aliens", "Sci-Fi"},
        {"alternate history", "Grand Strategy"},
        {"animals", "Animals"},
        {"anime", "Anime"},
        {"arcade", "Arcade"},
        {"archery", "Sports"},
        {"arena shooter", "Shooter"},
        {"assassins", "Stealth"},
        {"asymmetric vr", "VR"},
        {"asynchronous multiplayer", "Multiplayer"},
        {"atmospheric", "Atmospheric"},
        {"automation", "Base Building"},
        {"automobile sim", "Racing"},
        {"base building", "Base Building"},
        {"baseball", "Team Sports"},
        {"basketball", "Team Sports"},
        {"battle royale", "Battle Royale"},
        {"beat 'em up", "Beat 'em Up"},
        {"beautiful", "Atmospheric"},
        {"billiards", "Sports"},
        {"birds", "Animals"},
        {"bmx", "Sports"},
        {"board game", "Board Game"},
        {"boomer shooter", "First-Person Shooter"},
        {"bowling", "Sports"},
        {"boxing", "Sports"},
        {"building", "Base Building"},
        {"bullet heaven", "Shoot 'em Up"},
        {"bullet hell", "Shoot 'em Up"},
        {"capitalism", "Management"},
        {"capybaras", "Animals"},
        {"card battler", "Card Game"},
        {"card game", "Card Game"},
        {"cartoon", "Cartoony"},
        {"cartoony", "Cartoony"},
        {"casual", "Casual"},
        {"cats", "Animals"},
        {"character action game", "Hack and Slash"},
        {"chess", "Board Game"},
        {"choices matter", "Story Rich"},
        {"choose your own adventure", "Story Rich"},
        {"city builder", "City Builder"},
        {"classic", "Retro"},
        {"co-op", "Co-op"},
        {"co-op campaign", "Co-op"},
        {"cold war", "Historical"},
        {"collectathon", "Platformer"},
        {"colony sim", "City Builder"},
        {"combat racing", "Racing"},
        {"comedy", "Comedy"},
        {"comic book", "Cartoony"},
        {"competitive", "Multiplayer"},
        {"conspiracy", "Mystery"},
        {"cooking", "Life Sim"},
        {"cozy", "Cozy"},
        {"crafting", "Crafting"},
        {"creature collector", "Creature Collector"},
        {"cricket", "Team Sports"},
        {"crime", "Crime"},
        {"crpg", "CRPG"},
        {"cult", "Supernatural"},
        {"cute", "Family Friendly"},
        {"cyberpunk", "Cyberpunk"},
        {"cycling", "Sports"},
        {"dark", "Dark"},
        {"dark comedy", "Comedy"},
        {"dark fantasy", "Fantasy"},
        {"dark humor", "Comedy"},
        {"dating sim", "Dating Sim"},
        {"deckbuilding", "Card Game"},
        {"demons", "Supernatural"},
        {"destruction", "Physics"},
        {"detective", "Mystery"},
        {"dialogue heavy", "Story Rich"},
        {"dice", "Board Game"},
        {"difficult", "Difficult"},
        {"dinosaurs", "Dinosaurs"},
        {"diplomacy", "Grand Strategy"},
        {"dogs", "Animals"},
        {"dragons", "Fantasy"},
        {"driving", "Racing"},
        {"dungeon crawler", "Dungeon Crawler"},
        {"dwarves", "Fantasy"},
        {"dystopian", "Post-Apocalyptic"},
        {"early access", "Early Access"},
        {"economy", "Management"},
        {"education", "Educational"},
        {"elves", "Fantasy"},
        {"espionage", "Stealth"},
        {"exploration", "Exploration"},
        {"extraction shooter", "Shooter"},
        {"family friendly", "Family Friendly"},
        {"fantasy", "Fantasy"},
        {"farming", "Farming"},
        {"farming sim", "Farming"},
        {"female protagonist", "Female Protagonist"},
        {"fighting", "Fighting"},
        {"first-person", "First-Person"},
        {"fishing", "Fishing"},
        {"flight", "Flight"},
        {"fmv", "FMV"},
        {"football (american)", "Team Sports"},
        {"football (soccer)", "Team Sports"},
        {"foxes", "Animals"},
        {"fps", "First-Person Shooter"},
        {"free to play", "Free To Play"},
        {"funny", "Comedy"},
        {"futuristic", "Sci-Fi"},
        {"god game", "God Game"},
        {"golf", "Sports"},
        {"gore", "Gore"},
        {"gothic", "Dark"},
        {"grand strategy", "Grand Strategy"},
        {"hack and slash", "Hack and Slash"},
        {"heist", "Crime"},
        {"hentai", "Sexual Content"},
        {"hero shooter", "Shooter"},
        {"hidden object", "Hidden Object"},
        {"historical", "Historical"},
        {"hobby sim", "Life Sim"},
        {"hockey", "Team Sports"},
        {"horror", "Horror"},
        {"horses", "Animals"},
        {"idler", "Incremental"},
        {"immersive", "Atmospheric"},
        {"immersive sim", "Immersive Sim"},
        {"incremental", "Incremental"},
        {"indie", "Indie"},
        {"interactive fiction", "Story Rich"},
        {"investigation", "Mystery"},
        {"isometric", "Isometric"},
        {"job simulator", "Management"},
        {"jrpg", "JRPG"},
        {"jump scare", "Horror"},
        {"language learning", "Educational"},
        {"lgbtq+", "LGBTQ+"},
        {"life sim", "Life Sim"},
        {"local co-op", "Co-op"},
        {"local multiplayer", "Multiplayer"},
        {"logic", "Puzzle"},
        {"looter shooter", "Shooter"},
        {"lovecraftian", "Lovecraftian"},
        {"magic", "Fantasy"},
        {"mahjong", "Board Game"},
        {"management", "Management"},
        {"mars", "Space"},
        {"martial arts", "Martial Arts"},
        {"massively multiplayer", "Massively Multiplayer"},
        {"match 3", "Match 3"},
        {"mechs", "Robots"},
        {"medieval", "Medieval"},
        {"memes", "Comedy"},
        {"metroidvania", "Metroidvania"},
        {"military", "War"},
        {"mini golf", "Sports"},
        {"minigames", "Party Game"},
        {"mining", "Crafting"},
        {"mmorpg", "Massively Multiplayer"},
        {"moba", "MOBA"},
        {"motocross", "Racing"},
        {"multiplayer", "Multiplayer"},
        {"multiple endings", "Story Rich"},
        {"music", "Rhythm"},
        {"music-based procedural generation", "Rhythm"},
        {"mystery", "Mystery"},
        {"mystery dungeon", "Roguelike"},
        {"mythology", "Mythology"},
        {"narrative", "Story Rich"},
        {"nature", "Nature"},
        {"naval", "Naval"},
        {"naval combat", "Naval"},
        {"ninja", "Martial Arts"},
        {"noir", "Mystery"},
        {"nostalgia", "Retro"},
        {"nudity", "Nudity"},
        {"offroad", "Racing"},
        {"old school", "Retro"},
        {"on-rails shooter", "Shooter"},
        {"online co-op", "Co-op"},
        {"open world", "Open World"},
        {"open world survival craft", "Survival"},
        {"otome", "Visual Novel"},
        {"outbreak sim", "Zombies"},
        {"parody", "Comedy"},
        {"party", "Party Game"},
        {"party game", "Party Game"},
        {"party-based rpg", "CRPG"},
        {"physics", "Physics"},
        {"pirates", "Pirates"},
        {"pixel graphics", "Pixel Graphics"},
        {"platformer", "Platformer"},
        {"point & click", "Point & Click"},
        {"political sim", "Grand Strategy"},
        {"post-apocalyptic", "Post-Apocalyptic"},
        {"precision platformer", "Platformer"},
        {"programming", "Educational"},
        {"psychological horror", "Horror"},
        {"puzzle", "Puzzle"},
        {"puzzle platformer", "Platformer"},
        {"pvp", "Multiplayer"},
        {"racing", "Racing"},
        {"real time tactics", "Real-Time Strategy"},
        {"real-time", "Real-Time Strategy"},
        {"real-time with pause", "Real-Time Strategy"},
        {"realistic", "Realistic"},
        {"relaxing", "Cozy"},
        {"resource management", "Management"},
        {"retro", "Retro"},
        {"rhythm", "Rhythm"},
        {"robots", "Robots"},
        {"roguelike", "Roguelike"},
        {"roguelike deckbuilder", "Roguelike"},
        {"roguelite", "Roguelike"},
        {"romance", "Romance"},
        {"rome", "Historical"},
        {"rpg", "RPG"},
        {"rts", "Real-Time Strategy"},
        {"rugby", "Team Sports"},
        {"runner", "Arcade"},
        {"sailing", "Naval"},
        {"samurai", "Martial Arts"},
        {"sandbox", "Sandbox"},
        {"satire", "Comedy"},
        {"sci-fi", "Sci-Fi"},
        {"score attack", "Arcade"},
        {"sexual content", "Sexual Content"},
        {"shoot 'em up", "Shoot 'em Up"},
        {"shooter", "Shooter"},
        {"shop keeper", "Management"},
        {"side scroller", "Side Scroller"},
        {"simulation", "Simulation"},
        {"skateboarding", "Sports"},
        {"skiing", "Sports"},
        {"snow", "Nature"},
        {"snowboarding", "Sports"},
        {"sokoban", "Puzzle"},
        {"solitaire", "Board Game"},
        {"souls-like", "Souls-like"},
        {"space", "Space"},
        {"space sim", "Space"},
        {"spaceships", "Space"},
        {"spectacle fighter", "Hack and Slash"},
        {"spelling", "Educational"},
        {"split screen", "Multiplayer"},
        {"sports", "Sports"},
        {"stealth", "Stealth"},
        {"steampunk", "Steampunk"},
        {"story rich", "Story Rich"},
        {"strategy", "Strategy"},
        {"strategy rpg", "Tactical RPG"},
        {"submarine", "Underwater"},
        {"superhero", "Superhero"},
        {"supernatural", "Supernatural"},
        {"survival", "Survival"},
        {"survival horror", "Horror"},
        {"swordplay", "Martial Arts"},
        {"tabletop", "Board Game"},
        {"tactical", "Tactical"},
        {"tactical rpg", "Tactical RPG"},
        {"tanks", "Tanks"},
        {"team-based", "Multiplayer"},
        {"tennis", "Sports"},
        {"text-based", "Text-Based"},
        {"third person", "Third-Person"},
        {"third-person shooter", "Third-Person Shooter"},
        {"time management", "Management"},
        {"time manipulation", "Time Travel"},
        {"time travel", "Time Travel"},
        {"top-down", "Top-Down"},
        {"top-down shooter", "Shooter"},
        {"tower defense", "Tower Defense"},
        {"trading card game", "Card Game"},
        {"traditional roguelike", "Roguelike"},
        {"trains", "Trains"},
        {"transhumanism", "Cyberpunk"},
        {"transportation", "Trains"},
        {"trivia", "Puzzle"},
        {"turn-based", "Turn-Based"},
        {"turn-based combat", "Turn-Based"},
        {"turn-based strategy", "Turn-Based"},
        {"turn-based tactics", "Turn-Based"},
        {"twin stick shooter", "Shooter"},
        {"typing", "Educational"},
        {"underwater", "Underwater"},
        {"vampires", "Supernatural"},
        {"vikings", "Mythology"},
        {"violent", "Violent"},
        {"visual novel", "Visual Novel"},
        {"volleyball", "Team Sports"},
        {"vr", "VR"},
        {"walking simulator", "Walking Simulator"},
        {"war", "War"},
        {"wargame", "Grand Strategy"},
        {"werewolves", "Supernatural"},
        {"western", "Western"},
        {"wholesome", "Cozy"},
        {"wolves", "Animals"},
        {"word game", "Puzzle"},
        {"world war i", "War"},
        {"world war ii", "War"},
        {"wrestling", "Fighting"},
        {"wuxia", "Martial Arts"},
        {"xianxia", "Martial Arts"},
        {"zombies", "Zombies"},
    };
    return table;
}

} // namespace

QString SteamGenreMap::toCanonical(const QString& steamLabel)
{
    const QString trimmed = steamLabel.trimmed();
    const auto it = labelTable().find(trimmed.toLower());
    return it != labelTable().end() ? it.value() : trimmed;
}

std::optional<QString> SteamGenreMap::canonicalIfKnown(const QString& steamLabel)
{
    const auto it = labelTable().find(steamLabel.trimmed().toLower());
    if (it == labelTable().end())
        return std::nullopt;
    return it.value();
}

std::optional<int> SteamGenreMap::officialGenreIdFor(const QString& canonicalGenre)
{
    const auto it = officialGenreIds().find(canonicalGenre);
    if (it == officialGenreIds().end())
        return std::nullopt;
    return it.value();
}

std::optional<int> SteamGenreMap::tagIdFor(const QString& canonicalGenre)
{
    // The table lists each canonical genre's primary tag first.
    for (const auto& entry : tagTable()) {
        if (canonicalGenre == QLatin1String(entry.canonical))
            return entry.id;
    }
    return std::nullopt;
}

std::optional<QString> SteamGenreMap::canonicalForTagId(int tagId)
{
    for (const auto& entry : tagTable()) {
        if (entry.id == tagId)
            return QString::fromLatin1(entry.canonical);
    }
    return std::nullopt;
}

} // namespace ssv
