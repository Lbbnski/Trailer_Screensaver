#pragma once

#include <QDateTime>
#include <QString>

class QNetworkAccessManager;

namespace ssv {

// Twitch OAuth2 client-credentials flow, used only to obtain the bearer
// token IgdbClient attaches to every api.igdb.com request. Requires a free
// Twitch developer app (Client ID + Secret) — see the "Sources" section of
// SettingsDialog and README.md#requirements for how a user obtains one;
// this class never creates or validates the app itself, only exchanges its
// credentials for a token.
class IgdbAuthClient {
public:
    IgdbAuthClient(QNetworkAccessManager& networkManager, QString clientId, QString clientSecret);

    // Returns a cached, still-valid bearer token, transparently fetching a
    // fresh one via a blocking POST to id.twitch.tv when none is cached yet
    // or the cached one is at/past its expiry. Returns an empty string on
    // any failure (bad credentials, network error) — callers must treat
    // that as "can't authenticate this run", not a fatal error, exactly
    // like every other network failure path in this codebase.
    QString accessToken();

    // Discards the cached token so the next accessToken() call re-fetches
    // one. Used by IgdbClient after a 401, in case the cached token was
    // revoked server-side before its reported expiry.
    void invalidate();

    const QString& clientId() const { return m_clientId; }

private:
    QNetworkAccessManager& m_networkManager;
    QString m_clientId;
    QString m_clientSecret;

    QString m_cachedToken;
    QDateTime m_expiresAt;
};

} // namespace ssv
