#include "sources/igdb/IgdbClient.h"
#include "sources/igdb/IgdbAuthClient.h"
#include "util/Logging.h"
#include "util/NetworkAwait.h"

#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include <memory>

namespace ssv {

IgdbClient::IgdbClient(QNetworkAccessManager& networkManager, IgdbAuthClient& auth)
    : m_networkManager(networkManager)
    , m_auth(auth)
{
}

QJsonArray IgdbClient::postOnce(const QString& endpoint, const QString& apicalypseBody, int* httpStatus)
{
    *httpStatus = 0;

    const QString token = m_auth.accessToken();
    if (token.isEmpty())
        return {}; // not configured, or Twitch auth failed — already logged by IgdbAuthClient

    QNetworkRequest request(QUrl(QStringLiteral("https://api.igdb.com/v4/%1").arg(endpoint)));
    request.setRawHeader("Client-ID", m_auth.clientId().toUtf8());
    request.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("text/plain"));

    std::unique_ptr<QNetworkReply> reply(m_networkManager.post(request, apicalypseBody.toUtf8()));
    if (!awaitReply(reply.get())) {
        logWarning(QStringLiteral("igdb: request to %1 timed out").arg(endpoint));
        return {};
    }

    *httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();

    if (reply->error() != QNetworkReply::NoError && *httpStatus != 401) {
        logWarning(QStringLiteral("igdb: request to %1 failed: %2").arg(endpoint, reply->errorString()));
        return {};
    }
    if (*httpStatus == 401)
        return {};

    const auto doc = QJsonDocument::fromJson(body);
    return doc.isArray() ? doc.array() : QJsonArray{};
}

QJsonArray IgdbClient::query(const QString& endpoint, const QString& apicalypseBody)
{
    int httpStatus = 0;
    QJsonArray result = postOnce(endpoint, apicalypseBody, &httpStatus);
    if (httpStatus != 401)
        return result;

    // Cached token was rejected — could be genuinely expired despite our
    // margin, or revoked server-side. Worth exactly one refresh + retry,
    // same "degrade gracefully" budget every other source's error paths use.
    logInfo(QStringLiteral("igdb: token rejected (401) for %1 — refreshing and retrying once").arg(endpoint));
    m_auth.invalidate();
    return postOnce(endpoint, apicalypseBody, &httpStatus);
}

} // namespace ssv
