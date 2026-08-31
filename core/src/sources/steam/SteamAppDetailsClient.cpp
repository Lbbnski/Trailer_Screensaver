#include "sources/steam/SteamAppDetailsClient.h"
#include "sources/steam/SteamGenreMap.h"
#include "util/Logging.h"
#include "util/NetworkAwait.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

#include <memory>

namespace ssv {

namespace {

// Per Valve's Steamworks documentation on content descriptors. Ids not in
// this table (Valve has occasionally added more) pass through as
// "descriptor-<id>" rather than being silently dropped, so filter config
// can still reference them once observed.
QString contentDescriptorName(int id)
{
    switch (id) {
    case 1: return QStringLiteral("some-nudity-or-sexual-content");
    case 2: return QStringLiteral("frequent-violence-or-gore");
    case 3: return QStringLiteral("frequent-nudity-or-sexual-content");
    case 4: return QStringLiteral("general-mature-content");
    case 5: return QStringLiteral("adult-only-sexual-content");
    default: return QStringLiteral("descriptor-%1").arg(id);
    }
}

QList<TrailerRendition> parseMovies(const QJsonArray& movies)
{
    QList<TrailerRendition> renditions;
    if (movies.isEmpty())
        return renditions;

    // Steam typically lists one primary trailer; take the first entry
    // (or the one flagged "highlight" if present) rather than concatenating
    // every movie's renditions into the playlist.
    QJsonObject chosen = movies.first().toObject();
    for (const auto& m : movies) {
        if (m.toObject().value("highlight").toBool()) {
            chosen = m.toObject();
            break;
        }
    }

    const auto mp4 = chosen.value("mp4").toObject();
    const auto webm = chosen.value("webm").toObject();

    auto addIfPresent = [&](const QJsonObject& obj, const QString& key, int height, const QString& container) {
        const auto url = obj.value(key).toString();
        if (!url.isEmpty())
            renditions.append(TrailerRendition{url, height, container});
    };

    // Prefer mp4 (broader mpv/ffmpeg demuxer support with fewer surprises)
    // but keep webm too so MpvPlayer's resolution-cap logic has both
    // renditions to choose from if one is missing.
    addIfPresent(mp4, "max", 0 /* Steam doesn't report exact height for "max" */, QStringLiteral("mp4"));
    addIfPresent(mp4, "480", 480, QStringLiteral("mp4"));
    addIfPresent(webm, "max", 0, QStringLiteral("webm"));
    addIfPresent(webm, "480", 480, QStringLiteral("webm"));

    return renditions;
}

} // namespace

SteamAppDetailsClient::SteamAppDetailsClient(QNetworkAccessManager& networkManager, QString language, QString countryCode)
    : m_networkManager(networkManager)
    , m_language(std::move(language))
    , m_countryCode(std::move(countryCode))
{
}

std::optional<TrailerCandidate> SteamAppDetailsClient::fetchDetails(const QString& appid)
{
    QUrl url(QStringLiteral("https://store.steampowered.com/api/appdetails"));
    QUrlQuery query;
    query.addQueryItem("appids", appid); // one appid per request — see header comment
    query.addQueryItem("l", m_language);
    query.addQueryItem("cc", m_countryCode);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("SteamTrailerScreensaver/1.0"));

    std::unique_ptr<QNetworkReply> reply(m_networkManager.get(request));
    if (!awaitReply(reply.get())) {
        logWarning(QStringLiteral("appdetails request for %1 timed out").arg(appid));
        return std::nullopt;
    }
    if (reply->error() != QNetworkReply::NoError) {
        logWarning(QStringLiteral("appdetails request for %1 failed: %2").arg(appid, reply->errorString()));
        return std::nullopt;
    }

    const auto doc = QJsonDocument::fromJson(reply->readAll());
    const auto root = doc.object();
    const auto entry = root.value(appid).toObject();
    if (!entry.value("success").toBool())
        return std::nullopt;

    const auto data = entry.value("data").toObject();
    if (data.isEmpty())
        return std::nullopt;

    TrailerCandidate candidate;
    candidate.sourceId = QStringLiteral("steam");
    candidate.nativeId = appid;
    candidate.title = data.value("name").toString();

    // Used to disambiguate the YouTube-fallback search query (see
    // YoutubeFallbackResolver) — plenty of game titles collide with a
    // movie, book, or unrelated video of the same name, and adding the
    // developer is what actually distinguishes the search.
    const auto developers = data.value("developers").toArray();
    if (!developers.isEmpty())
        candidate.developer = developers.first().toString();

    for (const auto& g : data.value("genres").toArray())
        candidate.canonicalGenres << SteamGenreMap::toCanonical(g.toObject().value("description").toString());
    candidate.canonicalGenres.removeDuplicates();

    // required_age is documented as a string in some responses and a
    // number in others depending on locale — QJsonValue::toVariant()
    // handles both without a manual type check.
    candidate.ageRating = data.value("required_age").toVariant().toInt();

    for (const auto& id : data.value("content_descriptors").toObject().value("ids").toArray())
        candidate.contentDescriptors << contentDescriptorName(id.toInt());

    candidate.renditions = parseMovies(data.value("movies").toArray());
    candidate.needsFallbackResolution = candidate.renditions.isEmpty();

    return candidate;
}

} // namespace ssv
