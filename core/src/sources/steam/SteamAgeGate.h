#pragma once

class QNetworkAccessManager;

namespace ssv {

// Steam's storefront returns an interstitial "are you old enough" page in
// place of real appdetails/store data for mature-rated apps unless a
// handful of cookies are already present. Applying this once per
// QNetworkAccessManager avoids that interstitial for the whole session.
//
// Known limitation: some Adults-Only-Sexual-Content titles impose
// additional server-side checks beyond cookies and may still omit `movies`
// or 403 even with the gate applied — SteamAppDetailsClient treats that as
// "skip this app", not an error, rather than trying to defeat those checks.
namespace SteamAgeGate {

void apply(QNetworkAccessManager& networkManager);

} // namespace SteamAgeGate

} // namespace ssv
