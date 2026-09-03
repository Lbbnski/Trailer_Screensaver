#include "sources/steam/YoutubeFallbackResolver.h"
#include "util/Logging.h"
#include "util/ProcessRunner.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace ssv {

namespace {

// How many top search results to look at before giving up on finding one
// that actually looks like the game — one subprocess call regardless, so
// this costs nothing extra beyond a slightly larger response to parse.
constexpr int kResultsToConsider = 5;

// YouTube's own topic category for a video — not present in flat-playlist
// search results at all (confirmed empirically: the "categories"/"tags"
// fields are simply empty there), only via a full per-video extraction.
// These two are the categories a real movie/film trailer with a
// same-named game reliably lands in — observed directly against a search
// that mixed both ("Alone in the Dark" returns both the 2005 film's
// trailer, categorized "Film & Animation", and the actual game's reveal
// trailer, categorized "Gaming", for the same query). Deliberately a small
// blocklist rather than an allow-list of "Gaming" only: real game trailers
// are sometimes categorized "Entertainment" or others by their uploader,
// and rejecting anything not exactly "Gaming" would throw those out too.
const QSet<QString>& blockedYoutubeCategories()
{
    static const QSet<QString> categories{
        QStringLiteral("Film & Animation"),
        QStringLiteral("Movies"),
    };
    return categories;
}

// One extra (full, non-flat) yt-dlp invocation per candidate that already
// passed the cheap title/duration checks — only run for a result that
// would otherwise be accepted, so this costs nothing for candidates
// already rejected on title or length. A fetch failure (network hiccup,
// video removed between the search and this call) returns an empty list,
// which is treated as "unknown, don't block" by the caller — the same
// "never over-filter on missing data" behavior every other source's
// mapping table uses for a value it doesn't recognize.
QStringList fetchYoutubeCategories(const QString& ytDlpPath, const QString& videoId)
{
    const QStringList args{
        "--dump-json",
        "--no-warnings",
        QStringLiteral("https://www.youtube.com/watch?v=%1").arg(videoId),
    };
    const auto result = ProcessRunner::run(ytDlpPath, args);
    if (!result.ok())
        return {};

    QStringList categories;
    for (const auto& c : QJsonDocument::fromJson(result.stdOut).object().value("categories").toArray())
        categories << c.toString();
    return categories;
}

// True if at least half of the game title's non-trivial words show up in
// the video's own title. Deliberately not an exact/full-title substring
// match: a real trailer's video title routinely adds "Official Trailer",
// drops a subtitle, or reorders words, and requiring an exact match would
// reject plenty of correct results. But requiring *some* real overlap is
// exactly what filters out an unrelated video or a same-named movie
// trailer that happens to rank first for the search query.
bool titleLooksLikeMatch(const QString& gameTitle, const QString& videoTitle)
{
    // UseUnicodePropertiesOption so \w covers non-ASCII letters (accented
    // Latin, CJK, ...) instead of just [A-Za-z0-9_] — otherwise a title
    // like "细胞战争" (no internal spaces/punctuation at all) would split
    // into zero words and silently skip this check entirely.
    static const QRegularExpression wordSplit(QStringLiteral("[^\\w]+"),
                                                QRegularExpression::UseUnicodePropertiesOption);
    const QStringList gameWords = gameTitle.toLower().split(wordSplit, Qt::SkipEmptyParts);
    const QString normVideo = videoTitle.toLower();

    int meaningfulWords = 0;
    int matched = 0;
    for (const auto& w : gameWords) {
        if (w.length() < 3)
            continue; // skip short/common words that would match almost anything
        ++meaningfulWords;
        if (normVideo.contains(w))
            ++matched;
    }

    if (meaningfulWords == 0)
        return true; // title is all short/symbolic words — nothing meaningful to check, don't block

    return matched * 2 >= meaningfulWords;
}

} // namespace

YoutubeFallbackResolver::YoutubeFallbackResolver(QString ytDlpPath, int maxDurationSeconds)
    : m_ytDlpPath(std::move(ytDlpPath))
    , m_maxDurationSeconds(maxDurationSeconds)
{
}

TrailerRendition YoutubeFallbackResolver::renditionForVideoId(const QString& videoId)
{
    return TrailerRendition{
        QStringLiteral("https://www.youtube.com/watch?v=%1").arg(videoId),
        0, // resolution capping happens via mpv's ytdl-format at playback time
        QStringLiteral("youtube"),
    };
}

std::optional<TrailerRendition> YoutubeFallbackResolver::resolve(const QString& gameTitle, const QString& developer,
                                                                   QString* outVideoId, QString* outQueryUsed)
{
    // Including the developer is what actually disambiguates a search from
    // an unrelated same-named movie/book/video — plain "<title> official
    // trailer" alone has no way to tell those apart. Fall back to the
    // old-style query when the developer isn't known (older cache rows
    // fetched before this field existed, or a source that doesn't report
    // one).
    const QString query = developer.isEmpty()
        ? QStringLiteral("%1 official trailer").arg(gameTitle)
        : QStringLiteral("%1 %2 game trailer").arg(gameTitle, developer);

    // --flat-playlist avoids a full per-video extraction just to see
    // candidates (fast, one HTTP round trip) — for a YouTube search
    // specifically, the flat entries already include "duration", so this
    // stays cheap even with the length filter below. With multiple results
    // requested it prints one JSON object per line (JSON Lines) rather than
    // a single document.
    const QStringList args{
        "--dump-json",
        "--flat-playlist",
        "--playlist-items", QStringLiteral("1-%1").arg(kResultsToConsider),
        "--no-warnings",
        QStringLiteral("ytsearch%1:%2").arg(kResultsToConsider).arg(query),
    };

    const auto result = ProcessRunner::run(m_ytDlpPath, args);
    if (!result.ok()) {
        logWarning(QStringLiteral("yt-dlp search failed for \"%1\": %2").arg(query, QString::fromUtf8(result.stdErr)));
        return std::nullopt;
    }

    QString closestTitle;
    for (const auto& line : result.stdOut.split('\n')) {
        if (line.trimmed().isEmpty())
            continue;
        const auto obj = QJsonDocument::fromJson(line).object();
        const QString id = obj.value("id").toString();
        const QString title = obj.value("title").toString();
        if (id.isEmpty())
            continue;
        if (closestTitle.isEmpty())
            closestTitle = title;

        if (!titleLooksLikeMatch(gameTitle, title))
            continue;

        // A missing/null duration (e.g. an unusual entry type) isn't
        // treated as disqualifying on its own — only a duration we
        // actually know exceeds the cap rejects the result.
        const auto durationValue = obj.value("duration");
        if (m_maxDurationSeconds > 0 && durationValue.isDouble() && durationValue.toInt() > m_maxDurationSeconds) {
            logInfo(QStringLiteral("yt-dlp: \"%1\" is %2s, over the %3s cap — skipping (likely a Let's Play/walkthrough, not a trailer)")
                        .arg(title).arg(durationValue.toInt()).arg(m_maxDurationSeconds));
            continue;
        }

        const auto categories = fetchYoutubeCategories(m_ytDlpPath, id);
        bool isBlockedCategory = false;
        for (const auto& c : categories) {
            if (blockedYoutubeCategories().contains(c)) {
                isBlockedCategory = true;
                break;
            }
        }
        if (isBlockedCategory) {
            logInfo(QStringLiteral("yt-dlp: \"%1\" is categorized \"%2\" on YouTube — skipping "
                                    "(looks like non-game content, e.g. a movie trailer sharing the game's title)")
                        .arg(title, categories.join(QStringLiteral(", "))));
            continue;
        }

        if (outVideoId) *outVideoId = id;
        if (outQueryUsed) *outQueryUsed = query;
        return renditionForVideoId(id);
    }

    if (!closestTitle.isEmpty()) {
        logWarning(QStringLiteral("yt-dlp: no result for \"%1\" looked like a real, right-length match "
                                   "(closest: \"%2\") — skipping fallback for this app")
                       .arg(gameTitle, closestTitle));
    }
    return std::nullopt;
}

} // namespace ssv
