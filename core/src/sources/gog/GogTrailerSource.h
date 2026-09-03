#pragma once

#include "cache/CacheRepository.h"
#include "sources/IMetadataSource.h"
#include "sources/gog/GogCandidateFinder.h"

class QNetworkAccessManager;

namespace ssv {

class YoutubeFallbackResolver;

// A third IMetadataSource implementation, alongside Steam and IGDB — see
// docs/ARCHITECTURE.md#extensibility. No API key or auth needed;
// embed.gog.com/games/ajax/filtered and api.gog.com/products/{id} are both
// fully public.
//
// GOG's API shape is genuinely different from Steam's and IGDB's: one
// listing-endpoint response already carries genres/developer/age for every
// item (no separate per-id "details" call needed for those), while trailer
// videos live *only* behind a separate per-id api.gog.com/products/{id}
// call that carries no genre/age/developer data at all. fetchDetails()
// bridges this by consuming the metadata GogCandidateFinder stashed
// in-memory during the discoverCandidates() call that just found this id
// (see GogCandidateFinder's header comment for why that's always safe:
// TrailerResolver::preparePool() calls fetchDetails() for a new id only
// within the same preparePool() call that discovered it), and spending its
// own one network request purely on videos.
//
// Only GOG's `provider == "youtube"` trailers are resolved directly (the
// real YouTube id is embedded in video_url); `provider == "wistia"`
// entries (GOG's other common video host) are deliberately not handled —
// Wistia's own asset-resolution endpoint was found to be unreliable even
// for real videos during this source's design research, so those
// candidates fall through to the same shared YouTube-search fallback every
// source uses, rather than building fragile extraction logic against an
// unconfirmed mechanism.
class GogTrailerSource : public IMetadataSource {
public:
    GogTrailerSource(QNetworkAccessManager& networkManager, CacheRepository& repo,
                      YoutubeFallbackResolver& youtubeFallback, qint64 candidateListTtlSeconds);

    QString id() const override;
    QList<QString> discoverCandidates(const GenreFilter& filter, int requestBudget) override;
    std::optional<TrailerCandidate> fetchDetails(const QString& nativeId) override;
    std::optional<TrailerRendition> resolveFallback(const TrailerCandidate& candidate) override;

private:
    QNetworkAccessManager& m_networkManager;
    CacheRepository& m_repo;
    GogCandidateFinder m_candidateFinder;
    YoutubeFallbackResolver& m_youtubeFallback;
    qint64 m_candidateListTtlSeconds;
};

} // namespace ssv
