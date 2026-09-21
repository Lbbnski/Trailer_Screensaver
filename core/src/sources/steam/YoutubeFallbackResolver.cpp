#include "sources/steam/YoutubeFallbackResolver.h"
#include "config/ConfigPaths.h"
#include "sources/steam/TrailerHeuristics.h"
#include "util/Logging.h"
#include "util/ProcessRunner.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

namespace ssv {

namespace {

// How many top search results to look at before giving up on finding one
// that actually looks like the game — one subprocess call regardless, so
// this costs nothing extra beyond a slightly larger response to parse.
constexpr int kResultsToConsider = 5;

// One extra (full, non-flat) yt-dlp invocation per candidate that already
// passed the cheap title/duration checks — only run for a result that
// would otherwise be accepted, so this costs nothing for candidates
// already rejected on title or length. Returns the full parsed yt-dlp JSON
// object (empty on any fetch failure — network hiccup, video removed
// between the search and this call — treated as "unknown, don't block" by
// the caller, the same "never over-filter on missing data" behavior every
// other source's mapping table uses for a value it doesn't recognize).
// Categories/tags/description are only available this way: confirmed
// empirically that the fast flat-playlist search results leave them empty.
QJsonObject fetchYoutubeFullInfo(const QString& ytDlpPath, const QString& videoId)
{
    const QStringList args{
        "--dump-json",
        "--no-warnings",
        QStringLiteral("https://www.youtube.com/watch?v=%1").arg(videoId),
    };
    const auto result = ProcessRunner::run(ytDlpPath, args);
    if (!result.ok())
        return {};

    return QJsonDocument::fromJson(result.stdOut).object();
}

QStringList toStringList(const QJsonArray& arr)
{
    QStringList out;
    for (const auto& v : arr)
        out << v.toString();
    return out;
}

} // namespace

YoutubeFallbackResolver::YoutubeFallbackResolver(QString ytDlpPath, int maxDurationSeconds)
    : m_ytDlpPath(std::move(ytDlpPath))
    , m_maxDurationSeconds(maxDurationSeconds)
{
}

void YoutubeFallbackResolver::setRejectedVideoCheck(std::function<bool(const QString&)> isRejected)
{
    m_isVideoRejected = std::move(isRejected);
}

TrailerRendition YoutubeFallbackResolver::renditionForVideoId(const QString& videoId)
{
    return TrailerRendition{
        QStringLiteral("https://www.youtube.com/watch?v=%1").arg(videoId),
        0, // resolution capping happens via mpv's ytdl-format at playback time
        QStringLiteral("youtube"),
    };
}

QString YoutubeFallbackResolver::videoIdFromUrl(const QString& url)
{
    const QUrl parsed(url);
    if (!parsed.host().contains(QStringLiteral("youtube.com"), Qt::CaseInsensitive))
        return {};
    return QUrlQuery(parsed).queryItemValue(QStringLiteral("v"));
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
        appendDiagnosticRecord(ConfigPaths::youtubeDiagnosticsLogPath(), QJsonObject{
            {"gameTitle", gameTitle}, {"developer", developer}, {"query", query},
            {"outcome", "search_failed"}, {"error", QString::fromUtf8(result.stdErr)},
        });
        return std::nullopt;
    }

    // Every result yt-dlp's search returned, with whatever was learned about
    // it and why it was accepted/rejected — logged in full below regardless
    // of outcome, so a case where every result gets rejected (or the wrong
    // one gets accepted) can be inspected after the fact instead of only
    // guessed at from the one-line logInfo/logWarning summaries.
    QJsonArray evaluated;
    QString closestTitle;
    std::optional<TrailerRendition> accepted;
    QString acceptedId;

    // Records a rejection: the decision string in the diagnostics record is
    // the specific rule that fired, which is what makes a wrong accept or a
    // wrongly-rejected real trailer diagnosable after the fact.
    auto reject = [&](QJsonObject& entry, const QString& decision, const QString& detail = QString()) {
        entry["decision"] = decision;
        if (!detail.isEmpty())
            entry["reasonDetail"] = detail;
        evaluated.append(entry);
        logInfo(QStringLiteral("yt-dlp: \"%1\" rejected for \"%2\" (%3%4)")
                    .arg(entry.value("title").toString(), gameTitle, decision,
                         detail.isEmpty() ? QString() : QStringLiteral(": ") + detail));
    };

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

        QJsonObject entry{{"id", id}, {"title", title}, {"duration", obj.value("duration")}};

        // Cheap title-only checks first — all free, no extra yt-dlp call.
        if (m_isVideoRejected && m_isVideoRejected(id)) {
            reject(entry, QStringLiteral("rejected_reported"));
            continue;
        }
        if (!TrailerHeuristics::titleMatchesGame(gameTitle, title)) {
            reject(entry, QStringLiteral("rejected_title"));
            continue;
        }
        if (!TrailerHeuristics::hasTrailerWord(title)) {
            reject(entry, QStringLiteral("rejected_no_trailer_word"));
            continue;
        }
        if (const auto word = TrailerHeuristics::nonTrailerWord(gameTitle, title); !word.isEmpty()) {
            reject(entry, QStringLiteral("rejected_non_trailer_content"), word);
            continue;
        }
        if (const auto word = TrailerHeuristics::movieWordInTitle(gameTitle, title); !word.isEmpty()) {
            reject(entry, QStringLiteral("rejected_movie_title"), word);
            continue;
        }

        // A missing/null duration (e.g. an unusual entry type) isn't
        // treated as disqualifying on its own — only a duration we
        // actually know exceeds the cap rejects the result.
        const auto durationValue = obj.value("duration");
        if (m_maxDurationSeconds > 0 && durationValue.isDouble() && durationValue.toInt() > m_maxDurationSeconds) {
            reject(entry, QStringLiteral("rejected_duration"), QString::number(durationValue.toInt()));
            continue;
        }

        // Only now spend the extra full-detail fetch.
        const auto fullInfo = fetchYoutubeFullInfo(m_ytDlpPath, id);
        const auto categories = toStringList(fullInfo.value("categories").toArray());
        const auto tags = toStringList(fullInfo.value("tags").toArray());
        const QString description = fullInfo.value("description").toString();
        entry["categories"] = QJsonArray::fromStringList(categories);
        entry["tags"] = QJsonArray::fromStringList(tags);
        entry["channel"] = fullInfo.value("channel").toString();
        entry["uploader"] = fullInfo.value("uploader").toString();
        entry["viewCount"] = fullInfo.value("view_count");
        entry["uploadDate"] = fullInfo.value("upload_date").toString();
        entry["description"] = description.left(500);
        const int ageLimit = fullInfo.value("age_limit").toInt();
        entry["ageLimit"] = ageLimit;

        // An age-restricted video can't be played at all without a logged-in
        // YouTube session ("Sign in to confirm your age" — seen in a real
        // log, where mpv then failed with "unrecognized file format" and the
        // slot was wasted), and this app has no cookies to offer. Picking one
        // just produces a dead slot, so treat it as unresolvable and let the
        // next search result (or a later retry) find a playable one.
        if (ageLimit >= 18) {
            reject(entry, QStringLiteral("rejected_age_restricted"), QString::number(ageLimit));
            continue;
        }

        // A same-named movie's/show's trailer: YouTube's own category says
        // so, or its description/tags carry film/TV-studio/streaming-service
        // phrasing. One such signal is enough on a non-Gaming video; on a
        // Gaming one it takes two, since a real game trailer's description
        // can legitimately mention a streaming service once.
        if (TrailerHeuristics::isFilmCategory(categories)) {
            reject(entry, QStringLiteral("rejected_category"), categories.join(QStringLiteral(", ")));
            continue;
        }
        const QString blob = title + QLatin1Char(' ') + description + QLatin1Char(' ') + tags.join(QLatin1Char(' '));
        const int movieSignals = TrailerHeuristics::movieSignalCount(description + QLatin1Char(' ') + tags.join(QLatin1Char(' ')));
        if (movieSignals >= 2 || (movieSignals >= 1 && !categories.contains(QStringLiteral("Gaming")))) {
            reject(entry, QStringLiteral("rejected_movie_signals"), QString::number(movieSignals));
            continue;
        }
        if (TrailerHeuristics::lacksGameContext(categories, blob)) {
            reject(entry, QStringLiteral("rejected_not_game_content"), categories.join(QStringLiteral(", ")));
            continue;
        }

        entry["decision"] = "accepted";
        evaluated.append(entry);
        accepted = renditionForVideoId(id);
        acceptedId = id;
        break;
    }

    appendDiagnosticRecord(ConfigPaths::youtubeDiagnosticsLogPath(), QJsonObject{
        {"gameTitle", gameTitle}, {"developer", developer}, {"query", query},
        {"outcome", accepted ? "accepted" : "no_match"},
        {"acceptedId", acceptedId},
        {"results", evaluated},
    });

    if (accepted) {
        if (outVideoId) *outVideoId = acceptedId;
        if (outQueryUsed) *outQueryUsed = query;
        return accepted;
    }

    if (!closestTitle.isEmpty()) {
        logWarning(QStringLiteral("yt-dlp: no result for \"%1\" looked like a real, right-length match "
                                   "(closest: \"%2\") — skipping fallback for this app")
                       .arg(gameTitle, closestTitle));
    }
    return std::nullopt;
}

} // namespace ssv
