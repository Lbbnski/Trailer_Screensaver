#pragma once

#include <QString>
#include <QStringList>
#include <optional>

namespace ssv {

// One playable video rendition of a trailer. approxHeight is a best-effort
// vertical resolution used to enforce the user's configured resolution cap;
// sources that don't know an exact height (e.g. an unresolved YouTube id
// handed to mpv's ytdl_hook) may report 0 and let mpv-side format selection
// do the capping instead (see YoutubeFallbackResolver / MpvPlayer).
struct TrailerRendition {
    QString url;
    int approxHeight = 0;
    QString container; // "webm", "mp4", "youtube"
};

// A game/trailer candidate normalized to a common shape so PlaylistEngine,
// the cache, and playback never need to know which IMetadataSource produced
// it. Genre and age fields are expressed in the canonical taxonomy (see
// GenreTaxonomy), not in any one source's native vocabulary, so filtering
// stays uniform as more sources are added later.
struct TrailerCandidate {
    QString sourceId;   // e.g. "steam" — matches IMetadataSource::id()
    QString nativeId;   // the source's own id, e.g. a Steam appid as a string
    QString title;
    QString developer;  // empty if unknown; used to disambiguate YouTube-fallback searches

    QStringList canonicalGenres;
    int ageRating = 0;                  // normalized minimum-age cutoff, e.g. Steam's required_age
    QStringList contentDescriptors;     // normalized flags, e.g. "nudity", "violence", "adult-only"

    QList<TrailerRendition> renditions; // empty if the source has no trailer for this candidate
    bool needsFallbackResolution = false; // true if renditions is empty and a fallback should be attempted
};

// What the user's genre filter setting selects.
struct GenreFilter {
    enum class Mode { AllowList, BlockList };
    Mode mode = Mode::AllowList;
    QStringList genres; // canonical genre names

    // Biases candidate discovery toward popular/most-played titles when a
    // source supports it (e.g. SteamTrailerSource seeds from SteamSpy's
    // top-played lists first). Purely a discovery-order preference, not an
    // exclusionary filter — a source that doesn't support it just ignores
    // this flag.
    bool preferPopular = false;
};

} // namespace ssv
