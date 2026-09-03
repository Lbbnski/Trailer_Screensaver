#include "sources/GenreTaxonomy.h"

namespace ssv {

const QStringList& GenreTaxonomy::canonicalGenres()
{
    // Deliberately modeled on Steam's own store genre list since Steam is
    // the first source, but treated as the shared canonical vocabulary for
    // all sources going forward — a future source maps its own genres onto
    // this list rather than this list growing per-source.
    static const QStringList genres{
        QStringLiteral("Action"),
        QStringLiteral("Adventure"),
        QStringLiteral("Casual"),
        QStringLiteral("Indie"),
        QStringLiteral("RPG"),
        QStringLiteral("Simulation"),
        QStringLiteral("Strategy"),
        QStringLiteral("Sports"),
        QStringLiteral("Racing"),
        QStringLiteral("Horror"),
        QStringLiteral("Puzzle"),
        QStringLiteral("Platformer"),
        QStringLiteral("Fighting"),
        QStringLiteral("Shooter"),
        QStringLiteral("Massively Multiplayer"),
        QStringLiteral("Free To Play"),
        QStringLiteral("Early Access"),
        QStringLiteral("Sci-Fi"),
        QStringLiteral("Fantasy"),
        QStringLiteral("Survival"),
        QStringLiteral("Stealth"),
        QStringLiteral("Sandbox"),
        QStringLiteral("Visual Novel"),
    };
    return genres;
}

bool GenreTaxonomy::isCanonical(const QString& genre)
{
    return canonicalGenres().contains(genre, Qt::CaseInsensitive);
}

} // namespace ssv
