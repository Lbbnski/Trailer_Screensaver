#pragma once

#include "sources/SourceTypes.h"

#include <QList>
#include <QString>
#include <optional>

namespace ssv {

// The contract every trailer/metadata source implements. Steam is the only
// concrete implementation today (see sources/steam/SteamTrailerSource.h),
// but nothing outside this interface — SourceRegistry, TrailerResolver,
// CacheRepository, PlaylistEngine, or any playback/UI code — is allowed to
// know that. Adding a future source (GOG, Epic, IGDB, ...) means: implement
// this interface, add a genre mapping into GenreTaxonomy, and register an
// instance with SourceRegistry / list it in config's sources.enabled.
class IMetadataSource {
public:
    virtual ~IMetadataSource() = default;

    // Short stable identifier used as the source_id column in the cache and
    // in config's sources.enabled list, e.g. "steam".
    virtual QString id() const = 0;

    // Ask the source for a batch of native ids that plausibly match the
    // given genre filter, without exceeding requestBudget outbound network
    // requests. Returning fewer ids than requested is fine — callers treat
    // this as "found what was cheaply available this run" and are expected
    // to call again on a later run to discover more (see
    // SteamGenreCandidateFinder's resumable pagination for how the first
    // implementation does this).
    virtual QList<QString> discoverCandidates(const GenreFilter& filter, int requestBudget) = 0;

    // Fetch full details for one native id. May return std::nullopt if the
    // source can't produce details for this id (removed app, request
    // failure, etc.) — callers must treat that as "skip", not an error.
    virtual std::optional<TrailerCandidate> fetchDetails(const QString& nativeId) = 0;

    // Attempt to fill in a trailer for a candidate whose renditions are
    // empty (needsFallbackResolution == true). Sources that never need a
    // fallback (or don't support one) return std::nullopt unconditionally.
    virtual std::optional<TrailerRendition> resolveFallback(const TrailerCandidate& candidate) = 0;
};

} // namespace ssv
