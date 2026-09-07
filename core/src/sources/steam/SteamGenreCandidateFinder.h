#pragma once

#include "cache/CacheRepository.h"

#include <QHash>
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
    // path (steamSpyByTag) during the most recent discover() call on this
    // instance — empty if none. Steam's own appdetails `genres` field is
    // limited to a small fixed set (Action, Adventure, RPG, Strategy, ...)
    // that doesn't include tag-style genres at all: verified directly that
    // Resident Evil Village's own `genres` is just `["Action"]` despite
    // SteamSpy correctly tagging it "Horror". Without this, a candidate
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

private:
    QStringList searchStorePage(int genreId, int start, int count, bool* hasMore);
    QStringList steamSpyByGenre(const QString& canonicalGenre);
    QStringList steamSpyByTag(const QString& canonicalGenre);
    QStringList steamSpyTop(const QString& request);

    QNetworkAccessManager& m_networkManager;
    QHash<QString, QStringList> m_tagHints;
    QSet<QString> m_popularHints;
};

} // namespace ssv
