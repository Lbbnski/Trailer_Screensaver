#include "selection/PlaylistEngine.h"

#include <QDateTime>
#include <QRandomGenerator>

namespace ssv {

PlaylistEngine::PlaylistEngine(CacheRepository& repo) : m_repo(repo) {}

bool PlaylistEngine::passesFilter(const TrailerCandidate& candidate, const FilterConfig& filter) const
{
    for (const auto& blocked : filter.blockedContentDescriptors) {
        if (candidate.contentDescriptors.contains(blocked, Qt::CaseInsensitive))
            return false;
    }

    if (candidate.ageRating > filter.maxAge)
        return false;

    if (filter.mode == GenreFilter::Mode::AllowList) {
        if (filter.genres.isEmpty())
            return true; // no restriction configured
        for (const auto& g : filter.genres) {
            if (candidate.canonicalGenres.contains(g, Qt::CaseInsensitive))
                return true;
        }
        return false;
    }

    // BlockList: excluded if it matches ANY blocked genre.
    for (const auto& g : filter.genres) {
        if (candidate.canonicalGenres.contains(g, Qt::CaseInsensitive))
            return false;
    }
    return true;
}

QList<TrailerCandidate> PlaylistEngine::buildPlaylist(const QList<TrailerCandidate>& pool, const FilterConfig& filter,
                                                       qint64 noRepeatWindowSeconds) const
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const qint64 since = now - noRepeatWindowSeconds;

    QList<TrailerCandidate> playlist;
    for (const auto& candidate : pool) {
        if (!passesFilter(candidate, filter))
            continue;
        // needsFallbackResolution is always true here whenever renditions is
        // empty (see CacheRepository::rowToCandidate) — it can't tell
        // "not yet tried" apart from "tried and definitely failed". The
        // fallback_trailers retry-after marker can, so use that instead to
        // actually stop offering a candidate whose fallback resolution just
        // failed: without this, TrailerResolver::ensurePlayable() would
        // re-attempt (and re-fail) the exact same yt-dlp search for it every
        // time it came up, and PlaylistEngine::advance()'s bounded retry
        // would keep burning attempts on it instead of reaching a candidate
        // that's actually playable.
        if (candidate.renditions.isEmpty() && m_repo.fallbackRecentlyFailed(candidate.sourceId, candidate.nativeId, now))
            continue; // known unplayable as of a recent attempt
        if (m_repo.playedSince(candidate.sourceId, candidate.nativeId, since))
            continue;
        playlist.append(candidate);
    }

    auto* rng = QRandomGenerator::global();
    for (int i = playlist.size() - 1; i > 0; --i) {
        const int j = rng->bounded(i + 1);
        playlist.swapItemsAt(i, j);
    }
    return playlist;
}

} // namespace ssv
