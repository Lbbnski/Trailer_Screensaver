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

#include <algorithm>

#include <memory>

namespace ssv {

namespace {
constexpr int kPageSize = 48; // catalog.gog.com's default page size
const QString kSourceId = QStringLiteral("gog");
const QString kPopularPseudoGenre = QStringLiteral("__popular__");
const QString kRecentPseudoGenre = QStringLiteral("__recent__");
const QString kUpcomingPseudoGenre = QStringLiteral("__upcoming__");
} // namespace

GogCandidateFinder::GogCandidateFinder(QNetworkAccessManager& networkManager)
    : m_networkManager(networkManager)
{
}

TrailerCandidate GogCandidateFinder::parseProduct(const QJsonObject& item)
{
    TrailerCandidate candidate;
    candidate.sourceId = kSourceId;
    candidate.nativeId = item.value("id").toString();
    candidate.title = item.value("title").toString();
    const auto developers = item.value("developers").toArray();
    if (!developers.isEmpty())
        candidate.developer = developers.first().toString();
    candidate.storeUrl = item.value("storeLink").toString();

    // A product carries both its (few, top-level) genres and its
    // (many) tags; both map through the same table, and labels with no
    // canonical equivalent are dropped rather than stored as "genres".
    for (const auto* field : {"genres", "tags"}) {
        for (const auto& g : item.value(QLatin1String(field)).toArray()) {
            if (const auto canonical = GogGenreMap::canonicalIfKnown(g.toObject().value("name").toString()))
                candidate.canonicalGenres << *canonical;
        }
    }
    candidate.canonicalGenres.removeDuplicates();

    // Highest minimum age across the rating boards GOG lists (PEGI,
    // ESRB, USK, ...); each entry's ageRating is a plain number string.
    // A rating GOG doesn't publish leaves it at 0 (no restriction).
    for (const auto& r : item.value("ratings").toArray())
        candidate.ageRating = std::max(candidate.ageRating, r.toObject().value("ageRating").toString().toInt());

    // "coming-soon" and "preorder" are the two unreleased states
    // (verified live; everything else is "default").
    const auto state = item.value("productState").toString();
    candidate.comingSoon = state == QStringLiteral("coming-soon") || state == QStringLiteral("preorder");

    // needsFallbackResolution stays true (default) - videos aren't in
    // this response at all, only GogTrailerSource::fetchDetails() (a
    // separate per-id request) can fill renditions in.
    candidate.needsFallbackResolution = true;

    return candidate;
}

QStringList GogCandidateFinder::fetchPage(const QString& filter, const QString& order, int page, bool* hasMore)
{
    *hasMore = false;

    QUrl url(QStringLiteral("https://catalog.gog.com/v1/catalog"));
    QUrlQuery query;
    query.addQueryItem("productType", "in:game");
    query.addQueryItem("locale", "en-US");
    query.addQueryItem("countryCode", "US");
    query.addQueryItem("currencyCode", "USD");
    if (!filter.isEmpty()) {
        const int eq = filter.indexOf(QLatin1Char('='));
        query.addQueryItem(filter.left(eq), filter.mid(eq + 1));
    }
    query.addQueryItem("order", order);
    query.addQueryItem("page", QString::number(page));
    query.addQueryItem("limit", QString::number(kPageSize));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("SteamTrailerScreensaver/1.0"));

    std::unique_ptr<QNetworkReply> reply(m_networkManager.get(request));
    if (!awaitReply(reply.get()) || reply->error() != QNetworkReply::NoError) {
        const auto httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        logWarning(QStringLiteral("gog: catalog request failed (filter=%1, page=%2): %3 (http %4)")
                       .arg(filter).arg(page).arg(reply->errorString())
                       .arg(httpStatus.isValid() ? httpStatus.toString() : QStringLiteral("n/a")));
        return {};
    }

    const auto root = QJsonDocument::fromJson(reply->readAll()).object();
    const auto products = root.value("products").toArray();
    const int totalPages = root.value("pages").toInt(1);
    *hasMore = page < totalPages;

    QStringList ids;
    for (const auto& v : products) {
        const auto item = v.toObject();
        const QString nativeId = item.value("id").toString();
        if (nativeId.isEmpty() || nativeId == QStringLiteral("0"))
            continue;
        ids << nativeId;

        const TrailerCandidate candidate = parseProduct(item);
        m_pendingMetadata.insert(nativeId, candidate);
    }

    return ids;
}

TrailerCandidate GogCandidateFinder::lookupMetadata(const QString& nativeId, const QString& title)
{
    if (title.isEmpty())
        return {};

    QUrl url(QStringLiteral("https://catalog.gog.com/v1/catalog"));
    QUrlQuery query;
    query.addQueryItem("productType", "in:game");
    query.addQueryItem("locale", "en-US");
    query.addQueryItem("countryCode", "US");
    query.addQueryItem("currencyCode", "USD");
    query.addQueryItem("query", QStringLiteral("like:") + title);
    query.addQueryItem("limit", QString::number(kPageSize));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("SteamTrailerScreensaver/1.0"));

    std::unique_ptr<QNetworkReply> reply(m_networkManager.get(request));
    if (!awaitReply(reply.get()) || reply->error() != QNetworkReply::NoError)
        return {};

    const auto products = QJsonDocument::fromJson(reply->readAll()).object().value("products").toArray();
    for (const auto& v : products) {
        if (v.toObject().value("id").toString() == nativeId)
            return parseProduct(v.toObject());
    }
    return {};
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

    const auto catalogFilter = GogGenreMap::catalogFilterFor(canonicalGenre);
    if (!catalogFilter) {
        logInfo(QStringLiteral("gog: no genre or tag for \"%1\" — nothing to discover").arg(canonicalGenre));
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
        const auto pageIds = fetchPage(QStringLiteral("%1=in:%2").arg(catalogFilter->param, catalogFilter->slug),
                                        QStringLiteral("desc:trending"), page, &hasMore);
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
    const auto ids = fetchPage(QString(), QStringLiteral("desc:trending"), 1, &hasMore);

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
    const auto ids = fetchPage(QString(), QStringLiteral("desc:releaseDate"), 1, &hasMore);

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
        const auto ids = fetchPage(QStringLiteral("releaseStatuses=in:upcoming"), QStringLiteral("desc:trending"),
                                    page, &hasMore);
        int upcomingOnPage = 0;
        for (const auto& id : ids) {
            if (m_pendingMetadata.value(id).comingSoon) {
                upcomingIds << id;
                ++upcomingOnPage;
            }
        }
        // (every item of an upcoming-filtered page is unreleased; the check
        // just stops the walk if GOG ever returns something else.)
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
