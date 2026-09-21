#pragma once

#include "sources/SourceTypes.h"

#include <QString>
#include <functional>
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
//
// IMPORTANT: whenever resolve()'s matching/filtering logic changes in a way
// that could change which video gets picked for the same query, bump
// CacheRepository::kCurrentFallbackResolverVersion — otherwise the change
// has no effect on anything already cached, only on brand-new resolutions,
// which is very easy to mistake for the fix not working at all.
class YoutubeFallbackResolver {
public:
    // maxDurationSeconds <= 0 disables the length filter entirely.
    explicit YoutubeFallbackResolver(QString ytDlpPath = QStringLiteral("yt-dlp"), int maxDurationSeconds = 600);

    // Searches for `gameTitle`'s official trailer, disambiguated with
    // `developer` when known (plenty of game titles collide with a movie,
    // book, or unrelated video of the same name — plain "<title> official
    // trailer" alone routinely picks up exactly that). Looks at several
    // search results rather than blindly trusting the top one, and only
    // accepts a result that passes every rule in TrailerHeuristics (its
    // title names the game — in full for a 1-2 word title — and contains a
    // trailer/announcement word, with no review/reaction/breakdown/
    // interview/movie wording), is within maxDurationSeconds, wasn't
    // reported by the user (see setRejectedVideoCheck), and — via one extra
    // full-detail yt-dlp fetch per otherwise-acceptable candidate, since
    // categories/tags/description aren't in the fast flat search results at
    // all — isn't categorized as film/TV, doesn't carry film-studio or
    // streaming-service phrasing, and mentions games at all when its
    // category isn't Gaming. A search can just as easily land on a Let's
    // Play, a dev talk, or a same-named movie's trailer as the actual game
    // trailer. Every result considered, with the specific rule that
    // rejected it (or "accepted"), and the full yt-dlp metadata (categories,
    // tags, channel, uploader, view count, description) for any result that
    // got far enough to have that fetched, is appended as one JSON-Lines
    // record to ConfigPaths::youtubeDiagnosticsLogPath(), regardless of
    // outcome; see util/Logging.h's appendDiagnosticRecord. On success, returns a
    // TrailerRendition wrapping the durable
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

    // The inverse of renditionForVideoId(): the video id in a
    // youtube.com/watch?v=<id> URL, or an empty string for anything else
    // (a Steam CDN mp4, a non-YouTube host, ...).
    static QString videoIdFromUrl(const QString& url);

    // Lets a caller veto specific video ids — resolve() skips any result
    // for which this returns true. PlaybackSession wires this to the
    // user's own "report as not a game trailer" list
    // (CacheRepository::isVideoRejected), so a video the user reported is
    // never picked again for any game.
    void setRejectedVideoCheck(std::function<bool(const QString&)> isRejected);

private:
    QString m_ytDlpPath;
    int m_maxDurationSeconds;
    std::function<bool(const QString&)> m_isVideoRejected;
};

} // namespace ssv
