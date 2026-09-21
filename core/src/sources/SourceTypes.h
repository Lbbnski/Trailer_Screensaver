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

// How unreleased ("coming soon") games are treated — see
// TrailerCandidate::comingSoon and FilterConfig::upcomingMode.
enum class UpcomingMode {
    Include,       // mixed in with everything else (the default)
    OnlyUpcoming,  // only unreleased games
    Exclude,       // never unreleased games
};

// How many not-yet-fetched upcoming ids a source's discoverCandidates() hands
// back per run. TrailerResolver detail-fetches only a small budget per run,
// so in the default mix-in mode this stays small enough not to crowd out
// genre discovery, while "only upcoming" mode has nothing else to spend the
// budget on and asks for as many as the detail budget could use.
constexpr int kMixedInUpcomingIdsPerRun = 4;
constexpr int kOnlyUpcomingIdsPerRun = 60;

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
    QString storeUrl;   // empty if unknown; a page a user can open to look the game up themselves

    QStringList canonicalGenres;
    int ageRating = 0;                  // normalized minimum-age cutoff, e.g. Steam's required_age
    QStringList contentDescriptors;     // normalized flags, e.g. "nudity", "violence", "adult-only"

    QList<TrailerRendition> renditions; // empty if the source has no trailer for this candidate
    bool needsFallbackResolution = false; // true if renditions is empty and a fallback should be attempted

    // True if this candidate was ever seen in one of its source's
    // popularity-sorted discovery passes (see IMetadataSource::discoverCandidates'
    // preferPopular handling). Sticky once set — see CacheRepository::upsertAppDetails
    // — used by PlaylistEngine to actually give GenreFilter::preferPopular an
    // effect on playback, not just on discovery order.
    bool discoveredAsPopular = false;

    // True while the game hasn't been released yet, as of the last time its
    // details were fetched (Steam's appdetails release_date.coming_soon,
    // GOG's isComingSoon, IGDB's first_release_date in the future). Unlike
    // discoveredAsPopular this is *not* sticky — a refresh overwrites it, so
    // a game drops out of "upcoming" once its details are re-fetched after
    // release (up to the details cache TTL later; see docs/LIMITATIONS.md).
    bool comingSoon = false;
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

    // Steers discovery: OnlyUpcoming spends the whole discovery budget on
    // unreleased games instead of genre/popular/new-release lists; Exclude
    // skips upcoming discovery entirely; Include (default) adds a small
    // share of upcoming discovery alongside the normal kinds.
    UpcomingMode upcoming = UpcomingMode::Include;
};

} // namespace ssv
