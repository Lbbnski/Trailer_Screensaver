#include "sources/steam/SteamGenreMap.h"

namespace ssv {

namespace {

const QHash<QString, QString>& synonyms()
{
    // Keys are lower-cased Steam labels; values are canonical names.
    // NOTE: Steam's numeric genre ids below are widely observed but
    // undocumented and could shift — SteamGenreCandidateFinder treats a
    // failed/empty search result as "try SteamSpy instead" rather than
    // trusting these blindly.
    static const QHash<QString, QString> table{
        {"mmo", "Massively Multiplayer"},
        {"free to play", "Free To Play"},
        {"early access", "Early Access"},
    };
    return table;
}

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

} // namespace

QString SteamGenreMap::toCanonical(const QString& steamLabel)
{
    const QString trimmed = steamLabel.trimmed();
    const auto it = synonyms().find(trimmed.toLower());
    return it != synonyms().end() ? it.value() : trimmed;
}

std::optional<int> SteamGenreMap::officialGenreIdFor(const QString& canonicalGenre)
{
    const auto it = officialGenreIds().find(canonicalGenre);
    if (it == officialGenreIds().end())
        return std::nullopt;
    return it.value();
}

} // namespace ssv
