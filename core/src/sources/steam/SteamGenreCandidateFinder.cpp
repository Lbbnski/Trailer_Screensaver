#include "sources/steam/SteamGenreCandidateFinder.h"
#include "sources/steam/SteamGenreMap.h"
#include "util/Logging.h"
#include "util/NetworkAwait.h"

#include <QDateTime>
#include <QJsonArray>
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
const QString kRecentPseudoGenre = QStringLiteral("__recent__");
const QString kUpcomingPseudoGenre = QStringLiteral("__upcoming__");
}

SteamGenreCandidateFinder::SteamGenreCandidateFinder(QNetworkAccessManager& networkManager)
    : m_networkManager(networkManager)
{
}

QStringList SteamGenreCandidateFinder::searchStore(const QList<QPair<QString, QString>>& filterParams,
                                                    int start, int count, bool* hasMore)
{
    *hasMore = false;

    QUrl url(QStringLiteral("https://store.steampowered.com/search/results/"));
    QUrlQuery query;
    query.addQueryItem("start", QString::number(start));
    query.addQueryItem("count", QString::number(count));
    for (const auto& [key, value] : filterParams)
        query.addQueryItem(key, value);
    query.addQueryItem("json", "1");
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("SteamTrailerScreensaver/1.0"));

    std::unique_ptr<QNetworkReply> reply(m_networkManager.get(request));
    if (!awaitReply(reply.get()) || reply->error() != QNetworkReply::NoError) {
        logWarning(QStringLiteral("store search request failed (start=%1)").arg(start));
        return {};
    }

    const auto root = QJsonDocument::fromJson(reply->readAll()).object();

    QStringList appids;

    // With json=1 Steam returns {"desc":"","items":[{"name":..,"logo":..}]} —
    // no `html` field, and the appid only appears inside each item's logo URL
    // (.../steam/apps/<appid>/...). This code originally read root["html"],
    // which that response doesn't have, so the paged store search returned
    // nothing at all (masked by SteamSpy's bulk lists doing the real work).
    static const QRegularExpression logoAppid(QStringLiteral(R"re(/apps/(\d+)/)re"));
    for (const auto& v : root.value("items").toArray()) {
        const auto match = logoAppid.match(v.toObject().value("logo").toString());
        if (match.hasMatch())
            appids << match.captured(1);
    }

    // Older/alternate response shape, kept as a fallback.
    if (appids.isEmpty()) {
        static const QRegularExpression appidPattern(QStringLiteral(R"re(data-ds-appid="(\d+)")re"));
        auto it = appidPattern.globalMatch(root.value("html").toString());
        while (it.hasNext())
            appids << it.next().captured(1);
    }
    appids.removeDuplicates();

    *hasMore = appids.size() >= count; // Steam doesn't return a clean "has more" flag; infer from a full page.
    return appids;
}

QStringList SteamGenreCandidateFinder::searchStorePage(int genreId, int start, int count, bool* hasMore)
{
    return searchStore({{QStringLiteral("genre"), QString::number(genreId)},
                        {QStringLiteral("sort_by"), QStringLiteral("Reviews_DESC")}},
                       start, count, hasMore);
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

    if (!discovered.isEmpty()) {
        repo.addGenreCandidates(sourceId, kPopularPseudoGenre, discovered, now);
        for (const auto& id : discovered)
            m_popularHints.insert(id);
    }

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

            // Only the tag-only path needs a hint: an official-genre-id
            // discovery's candidates will already correctly carry that
            // genre in Steam's own appdetails response, but a tag-only
            // genre (Horror, Shooter, Sci-Fi, ...) structurally can't come
            // back from that same field — see tagHintsFor()'s comment.
            if (!officialGenreId) {
                for (const auto& id : spyIds)
                    m_tagHints[id] << canonicalGenre;
            }
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

QStringList SteamGenreCandidateFinder::upcoming(int requestBudget, int maxIds, CacheRepository& repo,
                                                 qint64 candidateListTtlSeconds)
{
    if (requestBudget <= 0 || maxIds <= 0)
        return {};

    const QString sourceId = QStringLiteral("steam");
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    auto state = repo.candidatePageState(sourceId, kUpcomingPseudoGenre);

    // Re-list at most once per TTL window: each page is one request and the
    // list of most-wishlisted unreleased games changes slowly.
    if ((now - state.lastRefreshedAt) > candidateListTtlSeconds) {
        QStringList listed;
        for (int page = 0; page < requestBudget; ++page) {
            bool hasMore = false;
            const auto ids = searchStore({{QStringLiteral("filter"), QStringLiteral("popularcomingsoon")}},
                                          page * kSearchPageSize, kSearchPageSize, &hasMore);
            listed << ids;
            if (!hasMore)
                break;
        }
        listed.removeDuplicates();

        if (!listed.isEmpty()) {
            repo.addGenreCandidates(sourceId, kUpcomingPseudoGenre, listed, now);
            repo.markComingSoon(sourceId, listed);
            state.lastRefreshedAt = now;
            repo.setCandidatePageState(sourceId, kUpcomingPseudoGenre, state);
        }
    }

    // Hand back only what doesn't have details yet, a few per run, so the
    // listed games drain into the pool across runs instead of all but the
    // first detail-fetch-budget's worth being dropped.
    return repo.unfetchedCandidates(sourceId, kUpcomingPseudoGenre, now, maxIds);
}

QStringList SteamGenreCandidateFinder::tagHintsFor(const QString& nativeId) const
{
    return m_tagHints.value(nativeId);
}

bool SteamGenreCandidateFinder::isPopularHint(const QString& nativeId) const
{
    return m_popularHints.contains(nativeId);
}

QStringList SteamGenreCandidateFinder::newAndTrending(int requestBudget, CacheRepository& repo, qint64 candidateListTtlSeconds)
{
    if (requestBudget <= 0)
        return {};

    const QString sourceId = QStringLiteral("steam");
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    auto state = repo.candidatePageState(sourceId, kRecentPseudoGenre);
    if ((now - state.lastRefreshedAt) <= candidateListTtlSeconds)
        return {};

    QUrl url(QStringLiteral("https://store.steampowered.com/api/featuredcategories"));
    QUrlQuery query;
    query.addQueryItem("l", "english");
    query.addQueryItem("cc", "US");
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("SteamTrailerScreensaver/1.0"));

    std::unique_ptr<QNetworkReply> reply(m_networkManager.get(request));
    if (!awaitReply(reply.get()) || reply->error() != QNetworkReply::NoError) {
        logWarning(QStringLiteral("Steam featuredcategories request failed"));
        return {};
    }

    const auto root = QJsonDocument::fromJson(reply->readAll()).object();
    QStringList discovered;
    for (const auto& section : {QStringLiteral("new_releases"), QStringLiteral("coming_soon")}) {
        for (const auto& v : root.value(section).toObject().value("items").toArray()) {
            const auto appId = v.toObject().value("id").toVariant().toLongLong();
            if (appId > 0)
                discovered << QString::number(appId);
        }
    }

    // featuredcategories only lists a few dozen; the store search's
    // "popular new releases" filter, newest first, adds a broader (and
    // already popularity-screened, so not just shovelware) recent slice.
    for (int page = 0; page < 2; ++page) {
        bool hasMore = false;
        discovered << searchStore({{QStringLiteral("filter"), QStringLiteral("popularnew")},
                                    {QStringLiteral("sort_by"), QStringLiteral("Released_DESC")}},
                                   page * kSearchPageSize, kSearchPageSize, &hasMore);
        if (!hasMore)
            break;
    }
    discovered.removeDuplicates();

    if (!discovered.isEmpty())
        repo.addGenreCandidates(sourceId, kRecentPseudoGenre, discovered, now);

    state.lastRefreshedAt = now;
    repo.setCandidatePageState(sourceId, kRecentPseudoGenre, state);
    return discovered;
}

} // namespace ssv
