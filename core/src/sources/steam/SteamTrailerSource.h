#pragma once

#include "cache/CacheRepository.h"
#include "sources/IMetadataSource.h"
#include "sources/steam/SteamAppDetailsClient.h"
#include "sources/steam/SteamGenreCandidateFinder.h"
#include "sources/steam/YoutubeFallbackResolver.h"

class QNetworkAccessManager;

namespace ssv {

// The first (and, for now, only) IMetadataSource implementation. Composes
// the Steam-specific pieces (appdetails fetch, genre/tag candidate
// discovery, age gate, YouTube fallback) behind the source-agnostic
// interface so TrailerResolver/PlaylistEngine never see anything
// Steam-specific.
//
// Holds a reference to the shared CacheRepository to persist its own
// discovery bookkeeping (genre_candidates/candidate_pages pagination state,
// resolved YouTube fallback ids) — safe to share with future sources since
// every table is scoped by source_id.
class SteamTrailerSource : public IMetadataSource {
public:
    SteamTrailerSource(QNetworkAccessManager& networkManager, CacheRepository& repo,
                        QString language, QString countryCode, QString ytDlpPath,
                        qint64 candidateListTtlSeconds, int maxTrailerDurationSeconds = 600);

    QString id() const override;
    QList<QString> discoverCandidates(const GenreFilter& filter, int requestBudget) override;
    std::optional<TrailerCandidate> fetchDetails(const QString& nativeId) override;
    std::optional<TrailerRendition> resolveFallback(const TrailerCandidate& candidate) override;

private:
    CacheRepository& m_repo;
    SteamAppDetailsClient m_detailsClient;
    SteamGenreCandidateFinder m_candidateFinder;
    YoutubeFallbackResolver m_youtubeFallback;
    qint64 m_candidateListTtlSeconds;
};

} // namespace ssv
