#pragma once

#include <QJsonArray>
#include <QString>

class QNetworkAccessManager;

namespace ssv {

class IgdbAuthClient;

// Low-level Apicalypse query executor shared by IgdbCandidateFinder and
// IgdbTrailerSource — everything either of them needs from api.igdb.com
// goes through query(). Not itself an IMetadataSource piece; just the HTTP
// plumbing, same role NetworkAwait.h plays for the Steam source's plain
// GET requests.
class IgdbClient {
public:
    IgdbClient(QNetworkAccessManager& networkManager, IgdbAuthClient& auth);

    // Posts `apicalypseBody` (e.g. "fields id,name; where genres.name = "
    // "\"Shooter\"; limit 50;") to https://api.igdb.com/v4/<endpoint> and
    // returns the parsed JSON array of results. A 401 (expired/revoked
    // token) triggers exactly one token refresh + retry via
    // IgdbAuthClient::invalidate(); any other failure, or a second 401,
    // returns an empty array — callers must treat that as "no results this
    // call", never a fatal error, matching every other network path in
    // this codebase.
    QJsonArray query(const QString& endpoint, const QString& apicalypseBody);

private:
    QJsonArray postOnce(const QString& endpoint, const QString& apicalypseBody, int* httpStatus);

    QNetworkAccessManager& m_networkManager;
    IgdbAuthClient& m_auth;
};

} // namespace ssv
