#include "sources/igdb/IgdbAuthClient.h"
#include "util/Logging.h"
#include "util/NetworkAwait.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

#include <memory>

namespace ssv {

namespace {
// Refresh a little before Twitch's own reported expiry rather than exactly
// at it, so a token never gets used right as it expires mid-request.
constexpr int kExpirySafetyMarginSeconds = 60;
} // namespace

IgdbAuthClient::IgdbAuthClient(QNetworkAccessManager& networkManager, QString clientId, QString clientSecret)
    : m_networkManager(networkManager)
    , m_clientId(std::move(clientId))
    , m_clientSecret(std::move(clientSecret))
{
}

void IgdbAuthClient::invalidate()
{
    m_cachedToken.clear();
}

QString IgdbAuthClient::accessToken()
{
    if (!m_cachedToken.isEmpty() && QDateTime::currentDateTimeUtc() < m_expiresAt)
        return m_cachedToken;

    if (m_clientId.isEmpty() || m_clientSecret.isEmpty())
        return {}; // not configured — caller (IgdbClient) treats this as "can't query"

    QUrl url(QStringLiteral("https://id.twitch.tv/oauth2/token"));
    QUrlQuery query;
    query.addQueryItem("client_id", m_clientId);
    query.addQueryItem("client_secret", m_clientSecret);
    query.addQueryItem("grant_type", "client_credentials");
    url.setQuery(query);

    QNetworkRequest request(url);
    std::unique_ptr<QNetworkReply> reply(m_networkManager.post(request, QByteArray()));
    if (!awaitReply(reply.get())) {
        logWarning(QStringLiteral("igdb: Twitch token request timed out"));
        return {};
    }
    if (reply->error() != QNetworkReply::NoError) {
        logWarning(QStringLiteral("igdb: Twitch token request failed: %1").arg(reply->errorString()));
        return {};
    }

    const auto root = QJsonDocument::fromJson(reply->readAll()).object();
    const QString token = root.value("access_token").toString();
    const int expiresInSeconds = root.value("expires_in").toInt();
    if (token.isEmpty()) {
        logWarning(QStringLiteral("igdb: Twitch token response had no access_token — check the configured client id/secret"));
        return {};
    }

    m_cachedToken = token;
    m_expiresAt = QDateTime::currentDateTimeUtc().addSecs(qMax(0, expiresInSeconds - kExpirySafetyMarginSeconds));
    return m_cachedToken;
}

} // namespace ssv
