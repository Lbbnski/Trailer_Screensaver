#include "sources/gog/GogCandidateFinder.h"
#include "sources/gog/GogGenreMap.h"
#include "util/Logging.h"
#include "util/NetworkAwait.h"

#include <QDateTime>
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
constexpr int kPageSize = 48; // embed.gog.com's own page size, observed directly
const QString kSourceId = QStringLiteral("gog");
const QString kPopularPseudoGenre = QStringLiteral("__popular__");
const QString kRecentPseudoGenre = QStringLiteral("__recent__");
const QString kUpcomingPseudoGenre = QStringLiteral("__upcoming__");
} // namespace

GogCandidateFinder::GogCandidateFinder(QNetworkAccessManager& networkManager)
    : m_networkManager(networkManager)
{
}

QStringList GogCandidateFinder::fetchPage(const QString& categoryParam, const QString& sort, int page, bool* hasMore)
{
    *hasMore = false;

    QUrl url(QStringLiteral("https://embed.gog.com/games/ajax/filtered"));
    QUrlQuery query;
    query.addQueryItem("mediaType", "game");
    if (!categoryParam.isEmpty())
        query.addQueryItem("category", categoryParam);
    // Both "popularity" and "date" were verified live to return real,
    // correctly-ordered results (unlike `category` — see GogGenreMap's
    // comment — an unrecognized `sort` value doesn't necessarily degrade
    // gracefully either, so don't add a new one here without checking it
    // returns 200 with real, correctly-ordered products first).
    if (!sort.isEmpty())
        query.addQueryItem("sort", sort);
    query.addQueryItem("page", QString::number(page));
    query.addQueryItem("limit", QString::number(kPageSize));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("SteamTrailerScreensaver/1.0"));

    std::unique_ptr<QNetworkReply> reply(m_networkManager.get(request));
    if (!awaitReply(reply.get()) || reply->error() != QNetworkReply::NoError) {
        const auto httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        logWarning(QStringLiteral("gog: listing request failed (category=%1, page=%2): %3 (http %4)")
                       .arg(categoryParam).arg(page).arg(reply->errorString())
                       .arg(httpStatus.isValid() ? httpStatus.toString() : QStringLiteral("n/a")));
        return {};
    }

    const auto root = QJsonDocument::fromJson(reply->readAll()).object();
    const auto products = root.value("products").toArray();
    const int totalPages = root.value("totalPages").toInt(1);
    *hasMore = page < totalPages;

    QStringList ids;
    for (const auto& v : products) {
        const auto item = v.toObject();
        const QString nativeId = QString::number(item.value("id").toVariant().toLongLong());
        if (nativeId.isEmpty() || nativeId == QStringLiteral("0"))
            continue;
        ids << nativeId;

        TrailerCandidate candidate;
        candidate.sourceId = kSourceId;
        candidate.nativeId = nativeId;
        candidate.title = item.value("title").toString();
        candidate.developer = item.value("developer").toString();
        const auto relativeUrl = item.value("url").toString();
        if (!relativeUrl.isEmpty())
            candidate.storeUrl = QStringLiteral("https://www.gog.com") + relativeUrl;
        for (const auto& g : item.value("genres").toArray())
            candidate.canonicalGenres << GogGenreMap::toCanonical(g.toString());
        candidate.canonicalGenres.removeDuplicates();
        // GOG's own simplified age-limit number — treated the same way as
        // Steam's required_age (a minimum-age cutoff), though GOG's exact
        // rating-board methodology behind this single number isn't
        // documented; worst case a slightly-off cutoff, never a crash.
        candidate.ageRating = item.value("ageLimit").toVariant().toInt();
        // Verified live: isComingSoon is true for exactly the items whose
        // releaseDate is in the future (22 of 22 on the first date-sorted
        // page), so it's a reliable unreleased marker.
        candidate.comingSoon = item.value("isComingSoon").toBool();
        // needsFallbackResolution stays true (default) — videos aren't in
        // this response at all, only GogTrailerSource::fetchDetails() (a
        // separate per-id request) can fill renditions in.
        candidate.needsFallbackResolution = true;

        m_pendingMetadata.insert(nativeId, candidate);
    }

    return ids;
}

TrailerCandidate GogCandidateFinder::pendingMetadata(const QString& nativeId) const
{
    return m_pendingMetadata.value(nativeId);
}

QStringList GogCandidateFinder::discover(const QString& canonicalGenre, int requestBudget,
                                          CacheRepository& repo, qint64 candidateListTtlSeconds)
{
    if (requestBudget <= 0)
        return {};

    const auto categoryParam = GogGenreMap::categoryParamFor(canonicalGenre);
    if (!categoryParam) {
        logInfo(QStringLiteral("gog: no genre facet for \"%1\" — nothing to discover").arg(canonicalGenre));
        return {};
    }

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    auto state = repo.candidatePageState(kSourceId, canonicalGenre);
    if (state.exhausted && (now - state.lastRefreshedAt) <= candidateListTtlSeconds)
        return {};

    QStringList discovered;
    while (requestBudget > 0) {
        bool hasMore = false;
        const int page = state.lastSearchStart / kPageSize + 1;
        const auto pageIds = fetchPage(*categoryParam, QStringLiteral("popularity"), page, &hasMore);
        --requestBudget;
        if (pageIds.isEmpty())
            break;

        repo.addGenreCandidates(kSourceId, canonicalGenre, pageIds, now);
        discovered << pageIds;
        state.lastSearchStart += kPageSize;
        state.exhausted = !hasMore;
        if (!hasMore)
            break;
    }

    state.lastRefreshedAt = now;
    repo.setCandidatePageState(kSourceId, canonicalGenre, state);
    discovered.removeDuplicates();
    return discovered;
}

QStringList GogCandidateFinder::topPlayed(int requestBudget, CacheRepository& repo, qint64 candidateListTtlSeconds)
{
    if (requestBudget <= 0)
        return {};

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    auto state = repo.candidatePageState(kSourceId, kPopularPseudoGenre);
    if ((now - state.lastRefreshedAt) <= candidateListTtlSeconds)
        return {};

    bool hasMore = false;
    const auto ids = fetchPage(QString(), QStringLiteral("popularity"), 1, &hasMore);

    if (!ids.isEmpty()) {
        repo.addGenreCandidates(kSourceId, kPopularPseudoGenre, ids, now);
        // fetchPage() already stashed each id's metadata in m_pendingMetadata
        // (see the class comment) — mark those entries popular directly
        // rather than tracking a separate hint set, since GogTrailerSource::
        // fetchDetails() reads the whole TrailerCandidate from there anyway.
        for (const auto& id : ids) {
            auto it = m_pendingMetadata.find(id);
            if (it != m_pendingMetadata.end())
                it->discoveredAsPopular = true;
        }
    }

    state.lastRefreshedAt = now;
    repo.setCandidatePageState(kSourceId, kPopularPseudoGenre, state);
    return ids;
}

QStringList GogCandidateFinder::newAndTrending(int requestBudget, CacheRepository& repo, qint64 candidateListTtlSeconds)
{
    if (requestBudget <= 0)
        return {};

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    auto state = repo.candidatePageState(kSourceId, kRecentPseudoGenre);
    if ((now - state.lastRefreshedAt) <= candidateListTtlSeconds)
        return {};

    bool hasMore = false;
    const auto ids = fetchPage(QString(), QStringLiteral("date"), 1, &hasMore);

    if (!ids.isEmpty())
        repo.addGenreCandidates(kSourceId, kRecentPseudoGenre, ids, now);

    state.lastRefreshedAt = now;
    repo.setCandidatePageState(kSourceId, kRecentPseudoGenre, state);
    return ids;
}

QStringList GogCandidateFinder::upcoming(int requestBudget, int maxIds, CacheRepository& repo)
{
    if (requestBudget <= 0 || maxIds <= 0)
        return {};

    const qint64 now = QDateTime::currentSecsSinceEpoch();

    // Listing pages are re-fetched on every call rather than TTL-gated like
    // the other sources' lists: GogTrailerSource::fetchDetails() depends on
    // this instance's in-memory pendingMetadata (a listing page carries the
    // genres/age/developer that the per-id product endpoint doesn't repeat),
    // so an id can only be detail-fetched in the same run its page was
    // fetched. That's one request per call in mix-in mode.
    QStringList upcomingIds;
    for (int page = 1; page <= requestBudget; ++page) {
        bool hasMore = false;
        const auto ids = fetchPage(QString(), QStringLiteral("date"), page, &hasMore);
        int upcomingOnPage = 0;
        for (const auto& id : ids) {
            if (m_pendingMetadata.value(id).comingSoon) {
                upcomingIds << id;
                ++upcomingOnPage;
            }
        }
        // date-sorted, so once a page has no unreleased items we're past them.
        if (!hasMore || upcomingOnPage == 0)
            break;
    }
    upcomingIds.removeDuplicates();

    if (!upcomingIds.isEmpty()) {
        repo.addGenreCandidates(kSourceId, kUpcomingPseudoGenre, upcomingIds, now);
        repo.markComingSoon(kSourceId, upcomingIds);
    }

    QStringList unfetched;
    for (const auto& id : upcomingIds) {
        if (unfetched.size() >= maxIds)
            break;
        if (!repo.hasFreshDetails(kSourceId, id, now))
            unfetched << id;
    }
    return unfetched;
}

} // namespace ssv
