#include "sources/igdb/IgdbTrailerSource.h"
#include "sources/GenreTaxonomy.h"
#include "sources/igdb/IgdbAgeRatingMap.h"
#include "sources/igdb/IgdbGenreMap.h"
#include "sources/steam/YoutubeFallbackResolver.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QRandomGenerator>

#include <algorithm>

namespace ssv {

namespace {

QStringList namesFrom(const QJsonArray& arr, const QString& key = QStringLiteral("name"))
{
    QStringList out;
    for (const auto& v : arr) {
        const auto s = v.toObject().value(key).toString();
        if (!s.isEmpty())
            out << s;
    }
    return out;
}

TrailerCandidate parseGame(const QJsonObject& game)
{
    TrailerCandidate candidate;
    candidate.sourceId = QStringLiteral("igdb");
    candidate.nativeId = QString::number(game.value("id").toVariant().toLongLong());
    candidate.title = game.value("name").toString();

    // IGDB spreads genre-ish information across three separate vocabularies
    // (see IgdbGenreMap's header comment) — run every label from all three
    // through the same map and merge.
    for (const auto& label : namesFrom(game.value("genres").toArray()))
        candidate.canonicalGenres << IgdbGenreMap::toCanonical(label);
    for (const auto& label : namesFrom(game.value("themes").toArray()))
        candidate.canonicalGenres << IgdbGenreMap::toCanonical(label);
    for (const auto& label : namesFrom(game.value("game_modes").toArray()))
        candidate.canonicalGenres << IgdbGenreMap::toCanonical(label);
    candidate.canonicalGenres.removeDuplicates();

    // Take the strictest (highest minimum age) across every regional rating
    // board IGDB reports for this game.
    int maxAge = 0;
    for (const auto& v : game.value("age_ratings").toArray()) {
        const auto category = v.toObject().value("rating_category").toObject();
        const auto rating = category.value("rating").toString();
        const auto org = category.value("organization").toObject().value("name").toString();
        maxAge = std::max(maxAge, IgdbAgeRatingMap::minimumAge(org, rating));
    }
    candidate.ageRating = maxAge;

    for (const auto& v : game.value("involved_companies").toArray()) {
        const auto entry = v.toObject();
        if (entry.value("developer").toBool()) {
            candidate.developer = entry.value("company").toObject().value("name").toString();
            break;
        }
    }

    for (const auto& v : game.value("videos").toArray()) {
        const auto videoId = v.toObject().value("video_id").toString();
        if (!videoId.isEmpty())
            candidate.renditions << YoutubeFallbackResolver::renditionForVideoId(videoId);
    }
    candidate.needsFallbackResolution = candidate.renditions.isEmpty();

    return candidate;
}

} // namespace

IgdbTrailerSource::IgdbTrailerSource(QNetworkAccessManager& networkManager, CacheRepository& repo,
                                      QString clientId, QString clientSecret,
                                      YoutubeFallbackResolver& youtubeFallback,
                                      qint64 candidateListTtlSeconds)
    : m_repo(repo)
    , m_auth(networkManager, std::move(clientId), std::move(clientSecret))
    , m_client(networkManager, m_auth)
    , m_candidateFinder(m_client)
    , m_youtubeFallback(youtubeFallback)
    , m_candidateListTtlSeconds(candidateListTtlSeconds)
{
}

QString IgdbTrailerSource::id() const
{
    return QStringLiteral("igdb");
}

QList<QString> IgdbTrailerSource::discoverCandidates(const GenreFilter& filter, int requestBudget)
{
    QStringList genresToSearch;
    if (filter.mode == GenreFilter::Mode::AllowList) {
        genresToSearch = filter.genres.isEmpty() ? GenreTaxonomy::canonicalGenres() : filter.genres;
    } else {
        for (const auto& g : GenreTaxonomy::canonicalGenres()) {
            if (!filter.genres.contains(g, Qt::CaseInsensitive))
                genresToSearch << g;
        }
    }
    if (genresToSearch.isEmpty() || requestBudget <= 0)
        return {};

    QStringList discovered;
    int remaining = requestBudget;

    if (filter.preferPopular && remaining > 0) {
        const int spend = std::min(1, remaining);
        discovered << m_candidateFinder.topPlayed(spend, m_repo, m_candidateListTtlSeconds);
        remaining -= spend;
    }

    if (remaining <= 0) {
        discovered.removeDuplicates();
        return discovered;
    }

    // Shuffled so a request budget smaller than genresToSearch.size() (the
    // common case once TrailerResolver's trickle-discovery budget kicks
    // in) doesn't always exhaust itself on the same first few genres in a
    // fixed order every run.
    std::shuffle(genresToSearch.begin(), genresToSearch.end(), *QRandomGenerator::global());

    const int perGenreBudget = std::max(1, remaining / static_cast<int>(genresToSearch.size()));
    for (const auto& genre : genresToSearch) {
        if (remaining <= 0)
            break;
        const int spend = std::min(perGenreBudget, remaining);
        const auto ids = m_candidateFinder.discover(genre, spend, m_repo, m_candidateListTtlSeconds);
        discovered << ids;
        remaining -= spend;
    }
    discovered.removeDuplicates();
    return discovered;
}

std::optional<TrailerCandidate> IgdbTrailerSource::fetchDetails(const QString& nativeId)
{
    const QString body = QStringLiteral(
        "fields name, genres.name, themes.name, game_modes.name, "
        "age_ratings.rating_category.rating, age_ratings.rating_category.organization.name, "
        "videos.video_id, involved_companies.company.name, involved_companies.developer; "
        "where id = %1;").arg(nativeId);

    const auto results = m_client.query(QStringLiteral("games"), body);
    if (results.isEmpty())
        return std::nullopt;

    return parseGame(results.first().toObject());
}

std::optional<TrailerRendition> IgdbTrailerSource::resolveFallback(const TrailerCandidate& candidate)
{
    if (const auto cachedId = m_repo.fallbackVideoId(id(), candidate.nativeId))
        return YoutubeFallbackResolver::renditionForVideoId(*cachedId);

    QString videoId, queryUsed;
    const auto rendition = m_youtubeFallback.resolve(candidate.title, candidate.developer, &videoId, &queryUsed);
    if (rendition)
        m_repo.cacheFallbackVideoId(id(), candidate.nativeId, queryUsed, videoId, QDateTime::currentSecsSinceEpoch());
    return rendition;
}

} // namespace ssv
