#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

namespace ssv {

// The one genre vocabulary every source's raw genres get mapped onto, and
// the vocabulary the filter UI (SettingsDialog) and PlaylistEngine operate
// on. Keeping this list source-agnostic is what lets a future second source
// with its own genre vocabulary (e.g. GOG's tags) filter uniformly
// alongside Steam without touching the UI or PlaylistEngine.
//
// Content descriptors follow the same idea but aren't an enum here — each
// source maps its own rating-system flags onto a small set of canonical
// strings such as "nudity", "violence", "adult-only"; TrailerCandidate
// carries those directly as QStringList since the set is source-driven and
// doesn't need central enumeration the way genres do (genres are shown as a
// fixed picklist in settings; content descriptors are mostly used to gate
// "adult-only" regardless of the age slider).
class GenreTaxonomy {
public:
    // Fixed list shown in SettingsDialog's genre picker, in display order.
    static const QStringList& canonicalGenres();

    // True if `genre` is a recognized canonical genre name.
    static bool isCanonical(const QString& genre);
};

} // namespace ssv
