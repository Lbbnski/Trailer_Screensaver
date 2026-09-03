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
    };
    return t;
}

// The reverse direction — canonical name to the exact-cased GOG `category`
// facet value used to filter the listing endpoint.
const QHash<QString, QString>& categoryParamTable()
{
    static const QHash<QString, QString> t{
        {"Action", "Action"},
        {"Adventure", "Adventure"},
        {"Casual", "Casual"},
        {"Indie", "Indie"},
        {"RPG", "RPG"},
        {"Simulation", "Simulation"},
        {"Strategy", "Strategy"},
        {"Sports", "Sports"},
        {"Racing", "Racing"},
        {"Horror", "Horror"},
        {"Puzzle", "Puzzle"},
        {"Platformer", "Platformer"},
        {"Fighting", "Fighting"},
        {"Shooter", "Shooter"},
        {"Massively Multiplayer", "Massively Multiplayer"},
        // "Casual", "Free To Play", "Early Access" — Free To Play/Early
        // Access deliberately absent: GOG expresses "free" via the `price`
        // filter and "in development" via the `release` filter, neither of
        // which is a genre facet.
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
