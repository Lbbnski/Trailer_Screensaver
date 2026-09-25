#pragma once

#include "sources/SourceTypes.h"

#include <QString>
#include <QStringList>
#include <optional>

class QNetworkAccessManager;

namespace ssv {

// Wraps Steam's undocumented storefront `appdetails` endpoint. This is the
// single most load-bearing piece of the Steam source: everything else
// (candidate discovery, caching, filtering, playback) ultimately consumes
// what this returns.
//
// Hard constraint (confirmed against the live API, not a design choice):
// requesting details for more than one appid per call silently degrades to
// a stub response containing only `price_overview` — full details
// (genres/required_age/content_descriptors/movies) are one appid per HTTP
// request. This is exactly why fetching happens lazily/opportunistically
// (see TrailerResolver's request budget) rather than as a bulk warm of the
// whole catalog.
class SteamAppDetailsClient {
public:
    explicit SteamAppDetailsClient(QNetworkAccessManager& networkManager,
                                    QString language = QStringLiteral("english"),
                                    QString countryCode = QStringLiteral("US"));

    // Fetches and parses details for one Steam appid. Returns std::nullopt
    // if the app doesn't exist, the request fails, or Steam's response has
    // `success: false` — all treated identically by callers as "skip this
    // app", per the age-gate/unofficial-API risk noted in the plan.
    std::optional<TrailerCandidate> fetchDetails(const QString& appid);

private:
    // The canonical genres an app's Steam *user tags* map to (Roguelike,
    // Cyberpunk, Cozy, ...). appdetails only returns Steam's dozen official
    // genres, so tags come from a second keyless call,
    // IStoreBrowseService/GetItems, which returns the app's tag ids ranked
    // by vote weight. Empty on any failure — the official genres alone are
    // still a usable (if coarser) classification.
    QStringList fetchTagGenres(const QString& appid);

    QNetworkAccessManager& m_networkManager;
    QString m_language;
    QString m_countryCode;
};

} // namespace ssv
