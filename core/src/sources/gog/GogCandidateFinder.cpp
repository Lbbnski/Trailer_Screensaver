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
    // "popularity" is a reasonable-looking sort value by analogy with GOG's
    // own storefront UI, but wasn't independently confirmed against a
    // documented list of accepted values — if GOG doesn't recognize it, the
    // endpoint is expected to just fall back to its own default ordering
    // rather than error, which only costs the (purely cosmetic)
    // preferPopular bias, not correctness.
    if (!sort.isEmpty())
        query.addQueryItem("sort", sort);
    query.addQueryItem("page", QString::number(page));
    query.addQueryItem("limit", QString::number(kPageSize));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("SteamTrailerScreensaver/1.0"));

    std::unique_ptr<QNetworkReply> reply(m_networkManager.get(request));
    if (!awaitReply(reply.get()) || reply->error() != QNetworkReply::NoError) {
        logWarning(QStringLiteral("gog: listing request failed (category=%1, page=%2)").arg(categoryParam).arg(page));
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
        for (const auto& g : item.value("genres").toArray())
            candidate.canonicalGenres << GogGenreMap::toCanonical(g.toString());
        candidate.canonicalGenres.removeDuplicates();
        // GOG's own simplified age-limit number — treated the same way as
        // Steam's required_age (a minimum-age cutoff), though GOG's exact
        // rating-board methodology behind this single number isn't
        // documented; worst case a slightly-off cutoff, never a crash.
        candidate.ageRating = item.value("ageLimit").toVariant().toInt();
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

    if (!ids.isEmpty())
        repo.addGenreCandidates(kSourceId, kPopularPseudoGenre, ids, now);

    state.lastRefreshedAt = now;
    repo.setCandidatePageState(kSourceId, kPopularPseudoGenre, state);
    return ids;
}

} // namespace ssv
