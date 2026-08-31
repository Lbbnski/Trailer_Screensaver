#pragma once

#include "sources/SourceTypes.h"

#include <QString>
#include <optional>

namespace ssv {

// Resolves a fallback trailer via YouTube search when a Steam app has none
// of its own, using yt-dlp as a subprocess rather than any bundled
// extraction logic (YouTube's site changes too often to maintain that
// in-tree — see docs/LIMITATIONS.md).
//
// Only ever caches the resolved *video id*, never a resolved stream URL:
// googlevideo direct URLs expire in hours, so persisting one would just
// reintroduce a staleness bug. At playback time MpvPlayer hands mpv the
// durable youtube.com/watch?v=<id> URL and lets libmpv's built-in
// ytdl_hook re-resolve a fresh stream URL every time.
class YoutubeFallbackResolver {
public:
    // maxDurationSeconds <= 0 disables the length filter entirely.
    explicit YoutubeFallbackResolver(QString ytDlpPath = QStringLiteral("yt-dlp"), int maxDurationSeconds = 600);

    // Searches for `gameTitle`'s official trailer, disambiguated with
    // `developer` when known (plenty of game titles collide with a movie,
    // book, or unrelated video of the same name — plain "<title> official
    // trailer" alone routinely picks up exactly that). Looks at several
    // search results rather than blindly trusting the top one, and only
    // accepts a result whose own video title actually resembles the game
    // title and whose duration is within maxDurationSeconds — a search can
    // just as easily land on a Let's Play or full walkthrough as an actual
    // trailer, and those tend to run far longer than any real trailer. On
    // success, returns a TrailerRendition wrapping the durable
    // watch URL (approxHeight is left at 0 — resolution capping for this
    // rendition happens via mpv's ytdl-format option at playback time, not
    // here) and fills outVideoId/outQueryUsed so the caller
    // (SteamTrailerSource) can cache them via
    // CacheRepository::cacheFallbackVideoId. Returns std::nullopt on any
    // yt-dlp failure, or if nothing found resembles the game closely enough
    // to trust — callers must treat that as "skip this app's fallback",
    // never as blocking playback.
    std::optional<TrailerRendition> resolve(const QString& gameTitle, const QString& developer,
                                             QString* outVideoId, QString* outQueryUsed);

    // Rebuilds a playable rendition from an already-cached video id,
    // without shelling out to yt-dlp again.
    static TrailerRendition renditionForVideoId(const QString& videoId);

private:
    QString m_ytDlpPath;
    int m_maxDurationSeconds;
};

} // namespace ssv
