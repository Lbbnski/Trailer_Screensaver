#pragma once

#include "cache/CacheRepository.h"

#include <QHash>
#include <QList>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;

namespace ssv {

// Discovers Steam appids plausibly matching a canonical genre, without ever
// iterating the full ~150k-app catalog. Combines two undocumented-but-widely-
// used endpoints:
//
//  - store.steampowered.com/search/results — paginated, popularity-sorted,
//    but only accepts Steam's official numeric genre ids (see
//    SteamGenreMap::officialGenreIdFor). Pagination state is persisted via
//    CacheRepository so a multi-run screensaver session picks up where the
//    last one left off instead of re-scanning from the start every time.
//  - steamspy.com/api.php (request=genre or request=tag) — a single call
//    returns an entire genre/tag's app list at once (community-maintained,
//    refreshed roughly daily), used both as a bulk seed for official genres
//    and as the *only* discovery path for tag-only canonical genres
//    (Horror, Shooter, Platformer, Fighting, Puzzle) that have no official
//    Steam genre id at all.
class SteamGenreCandidateFinder {
public:
    explicit SteamGenreCandidateFinder(QNetworkAccessManager& networkManager);

    // Discovers up to `requestBudget` outbound-request's worth of new
    // candidates for `canonicalGenre`, records them (and updated pagination
    // state) into `repo`, and returns the appids newly added this call.
    // Returns an empty list once discovery is exhausted for this genre or
    // the budget is 0 — callers should fall back to whatever's already
    // cached in that case.
    QStringList discover(const QString& canonicalGenre, int requestBudget,
                          CacheRepository& repo, qint64 candidateListTtlSeconds);

    // Seeds from SteamSpy's top-played-right-now and top-played-of-all-time
    // lists (used for GenreFilter::preferPopular) — genre-agnostic, so
    // results still need the normal per-app detail fetch (and the user's
    // genre/age filters) applied afterward like any other candidate.
    // Cached under the pseudo-genre "__popular__" via the same
    // genre_candidates/candidate_pages plumbing as a real genre, rather
    // than a bespoke cache table.
    QStringList topPlayed(int requestBudget, CacheRepository& repo, qint64 candidateListTtlSeconds);

    // Canonical genres a native id was discovered under via the tag-only
    // path (store search by tag id) during the most recent discover() call on this
    // instance — empty if none. Steam's own appdetails `genres` field is
    // limited to a small fixed set (Action, Adventure, RPG, Strategy, ...)
    // that doesn't include tag-style genres at all: verified directly that
    // Resident Evil Village's own `genres` is just `["Action"]` despite
    // Steam tagging it "Horror". Without this, a candidate
    // discovered specifically *because* it matched a tag-only genre would
    // never actually carry that genre in its cached canonicalGenres, and
    // silently fail the user's own filter for the genre that found it —
    // SteamTrailerSource::fetchDetails() merges this in as a fix.
    QStringList tagHintsFor(const QString& nativeId) const;

    // True if `nativeId` was returned by topPlayed() during the most recent
    // call on this instance — SteamTrailerSource::fetchDetails() merges this
    // into TrailerCandidate::discoveredAsPopular the same way tagHintsFor()
    // is merged into canonicalGenres, since a per-app appdetails fetch has
    // no notion of "popular" on its own.
    bool isPopularHint(const QString& nativeId) const;

    // Seeds from store.steampowered.com/api/featuredcategories' "new_releases"
    // and "coming_soon" sections — the same storefront-front-page endpoint
    // Steam's own site uses, one call for both lists. Genre-based discovery
    // (steamSpyByGenre/searchStore, all sorted by
    // reviews/popularity) structurally favors long-established games, so
    // without this a new or not-yet-released title essentially never
    // surfaces. Cached under the pseudo-genre "__recent__", same plumbing as
    // topPlayed()'s "__popular__".
    QStringList newAndTrending(int requestBudget, CacheRepository& repo, qint64 candidateListTtlSeconds);

    // Unreleased games, most-wishlisted first, from the storefront's own
    // "popular coming soon" search filter (50 per page — far more than the
    // ten `coming_soon` entries featuredcategories carries). At most
    // `requestBudget` pages are listed, at most once per TTL window; the
    // ids are recorded under the pseudo-genre "__upcoming__" and flagged
    // in the cache via CacheRepository::markComingSoon. Returns up to
    // `maxIds` of them that still lack details — repeat calls drain the
    // list, so a small maxIds (the default mix-in mode) still gets every
    // listed game fetched eventually without hogging the per-run detail
    // budget, while "only upcoming" mode passes a large one.
    QStringList upcoming(int requestBudget, int maxIds, CacheRepository& repo, qint64 candidateListTtlSeconds);

private:
    // One page of the storefront search with arbitrary filter parameters
    // (genre=..&sort_by=.., or filter=popularcomingsoon, ...).
    QStringList searchStore(const QList<QPair<QString, QString>>& filterParams,
                             int start, int count, bool* hasMore);
    QStringList searchStorePage(int genreId, int start, int count, bool* hasMore);
    QStringList steamSpyByGenre(const QString& canonicalGenre);
    QStringList steamSpyTop(const QString& request);

    QNetworkAccessManager& m_networkManager;
    QHash<QString, QStringList> m_tagHints;
    QSet<QString> m_popularHints;
};

} // namespace ssv
