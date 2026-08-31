#include "sources/steam/SteamGenreCandidateFinder.h"
#include "sources/steam/SteamGenreMap.h"
#include "util/Logging.h"
#include "util/NetworkAwait.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

#include <memory>

namespace ssv {

namespace {
constexpr int kSearchPageSize = 50;

// A cache key that can never collide with a real canonical genre name
// (GenreTaxonomy's names are all plain title-cased words), so it can share
// the genre_candidates/candidate_pages tables without a schema change.
const QString kPopularPseudoGenre = QStringLiteral("__popular__");
}

SteamGenreCandidateFinder::SteamGenreCandidateFinder(QNetworkAccessManager& networkManager)
    : m_networkManager(networkManager)
{
}

QStringList SteamGenreCandidateFinder::searchStorePage(int genreId, int start, int count, bool* hasMore)
{
    *hasMore = false;

    QUrl url(QStringLiteral("https://store.steampowered.com/search/results/"));
    QUrlQuery query;
    query.addQueryItem("start", QString::number(start));
    query.addQueryItem("count", QString::number(count));
    query.addQueryItem("genre", QString::number(genreId));
    query.addQueryItem("sort_by", "Reviews_DESC");
    query.addQueryItem("json", "1");
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("SteamTrailerScreensaver/1.0"));

    std::unique_ptr<QNetworkReply> reply(m_networkManager.get(request));
    if (!awaitReply(reply.get()) || reply->error() != QNetworkReply::NoError) {
        logWarning(QStringLiteral("store search request failed for genre id %1").arg(genreId));
        return {};
    }

    const auto root = QJsonDocument::fromJson(reply->readAll()).object();
    const QString html = root.value("html").toString();

    static const QRegularExpression appidPattern(QStringLiteral(R"re(data-ds-appid="(\d+)")re"));
    QStringList appids;
    auto it = appidPattern.globalMatch(html);
    while (it.hasNext())
        appids << it.next().captured(1);
    appids.removeDuplicates();

    *hasMore = appids.size() >= count; // Steam doesn't return a clean "has more" flag; infer from a full page.
    return appids;
}

QStringList SteamGenreCandidateFinder::steamSpyByGenre(const QString& canonicalGenre)
{
    QUrl url(QStringLiteral("https://steamspy.com/api.php"));
    QUrlQuery query;
    query.addQueryItem("request", "genre");
    query.addQueryItem("genre", canonicalGenre);
    url.setQuery(query);

    QNetworkRequest request(url);
    std::unique_ptr<QNetworkReply> reply(m_networkManager.get(request));
    if (!awaitReply(reply.get()) || reply->error() != QNetworkReply::NoError) {
        logWarning(QStringLiteral("SteamSpy genre request failed for %1").arg(canonicalGenre));
        return {};
    }

    const auto root = QJsonDocument::fromJson(reply->readAll()).object();
    return root.keys(); // object keyed by appid
}

QStringList SteamGenreCandidateFinder::steamSpyByTag(const QString& canonicalGenre)
{
    QUrl url(QStringLiteral("https://steamspy.com/api.php"));
    QUrlQuery query;
    query.addQueryItem("request", "tag");
    query.addQueryItem("tag", canonicalGenre);
    url.setQuery(query);

    QNetworkRequest request(url);
    std::unique_ptr<QNetworkReply> reply(m_networkManager.get(request));
    if (!awaitReply(reply.get()) || reply->error() != QNetworkReply::NoError) {
        logWarning(QStringLiteral("SteamSpy tag request failed for %1").arg(canonicalGenre));
        return {};
    }

    const auto root = QJsonDocument::fromJson(reply->readAll()).object();
    return root.keys();
}

QStringList SteamGenreCandidateFinder::steamSpyTop(const QString& request)
{
    QUrl url(QStringLiteral("https://steamspy.com/api.php"));
    QUrlQuery query;
    query.addQueryItem("request", request);
    url.setQuery(query);

    QNetworkRequest netRequest(url);
    std::unique_ptr<QNetworkReply> reply(m_networkManager.get(netRequest));
    if (!awaitReply(reply.get()) || reply->error() != QNetworkReply::NoError) {
        logWarning(QStringLiteral("SteamSpy %1 request failed").arg(request));
        return {};
    }

    const auto root = QJsonDocument::fromJson(reply->readAll()).object();
    return root.keys();
}

QStringList SteamGenreCandidateFinder::topPlayed(int requestBudget, CacheRepository& repo, qint64 candidateListTtlSeconds)
{
    if (requestBudget <= 0)
        return {};

    const QString sourceId = QStringLiteral("steam");
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    auto state = repo.candidatePageState(sourceId, kPopularPseudoGenre);

    if ((now - state.lastRefreshedAt) <= candidateListTtlSeconds)
        return {}; // still fresh — caller falls back to what's already cached

    QStringList discovered;
    if (requestBudget > 0) {
        discovered << steamSpyTop(QStringLiteral("top100in2weeks"));
        --requestBudget;
    }
    if (requestBudget > 0) {
        discovered << steamSpyTop(QStringLiteral("top100forever"));
        --requestBudget;
    }
    discovered.removeDuplicates();

    if (!discovered.isEmpty())
        repo.addGenreCandidates(sourceId, kPopularPseudoGenre, discovered, now);

    state.lastRefreshedAt = now;
    repo.setCandidatePageState(sourceId, kPopularPseudoGenre, state);

    return discovered;
}

QStringList SteamGenreCandidateFinder::discover(const QString& canonicalGenre, int requestBudget,
                                                 CacheRepository& repo, qint64 candidateListTtlSeconds)
{
    if (requestBudget <= 0)
        return {};

    const QString sourceId = QStringLiteral("steam");
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    auto state = repo.candidatePageState(sourceId, canonicalGenre);

    QStringList discovered;
    const bool candidateListStale = (now - state.lastRefreshedAt) > candidateListTtlSeconds;
    const auto officialGenreId = SteamGenreMap::officialGenreIdFor(canonicalGenre);

    // The SteamSpy bulk seed only needs to run once per TTL window, not
    // every call — it returns (close to) the whole genre/tag in one shot.
    if (candidateListStale && requestBudget > 0) {
        const QStringList spyIds = officialGenreId ? steamSpyByGenre(canonicalGenre) : steamSpyByTag(canonicalGenre);
        if (!spyIds.isEmpty()) {
            repo.addGenreCandidates(sourceId, canonicalGenre, spyIds, now);
            discovered << spyIds;
        }
        --requestBudget;
        state.lastRefreshedAt = now;
    }

    // Store search pagination only applies to genres with an official id;
    // tag-only genres rely solely on the SteamSpy bulk seed above.
    if (officialGenreId && !state.exhausted && requestBudget > 0) {
        bool hasMore = false;
        const QStringList pageIds = searchStorePage(*officialGenreId, state.lastSearchStart, kSearchPageSize, &hasMore);
        if (!pageIds.isEmpty()) {
            repo.addGenreCandidates(sourceId, canonicalGenre, pageIds, now);
            discovered << pageIds;
        }
        state.lastSearchStart += kSearchPageSize;
        state.exhausted = !hasMore;
        --requestBudget;
    }

    repo.setCandidatePageState(sourceId, canonicalGenre, state);
    discovered.removeDuplicates();
    return discovered;
}

} // namespace ssv
