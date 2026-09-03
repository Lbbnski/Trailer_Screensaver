#pragma once

#include "cache/CacheRepository.h"
#include "sources/IMetadataSource.h"
#include "sources/igdb/IgdbAuthClient.h"
#include "sources/igdb/IgdbCandidateFinder.h"
#include "sources/igdb/IgdbClient.h"

class QNetworkAccessManager;

namespace ssv {

class YoutubeFallbackResolver;

// The second IMetadataSource implementation, alongside SteamTrailerSource —
// see docs/ARCHITECTURE.md#extensibility for the recipe this follows.
// Composes the IGDB-specific pieces (OAuth, Apicalypse queries, genre/age
// mapping) behind the source-agnostic interface; TrailerResolver and
// PlaylistEngine never see anything IGDB-specific.
//
// Every Game.videos[] entry IGDB returns is already a curated YouTube video
// id, so the common case here never touches YoutubeFallbackResolver at all
// — resolveFallback() only fires for the rare candidate with no videos of
// its own, reusing the same shared resolver instance SteamTrailerSource
// uses (owned by PlaybackSession, passed in by reference) rather than
// duplicating the search/title-matching logic.
class IgdbTrailerSource : public IMetadataSource {
public:
    IgdbTrailerSource(QNetworkAccessManager& networkManager, CacheRepository& repo,
                       QString clientId, QString clientSecret,
                       YoutubeFallbackResolver& youtubeFallback,
                       qint64 candidateListTtlSeconds);

    QString id() const override;
    QList<QString> discoverCandidates(const GenreFilter& filter, int requestBudget) override;
    std::optional<TrailerCandidate> fetchDetails(const QString& nativeId) override;
    std::optional<TrailerRendition> resolveFallback(const TrailerCandidate& candidate) override;

private:
    CacheRepository& m_repo;
    IgdbAuthClient m_auth;
    IgdbClient m_client;
    IgdbCandidateFinder m_candidateFinder;
    YoutubeFallbackResolver& m_youtubeFallback;
    qint64 m_candidateListTtlSeconds;
};

} // namespace ssv
