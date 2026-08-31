#include "sources/steam/SteamAgeGate.h"

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkCookie>
#include <QNetworkCookieJar>

namespace ssv::SteamAgeGate {

void apply(QNetworkAccessManager& networkManager)
{
    auto* jar = networkManager.cookieJar();
    if (!jar) {
        jar = new QNetworkCookieJar(&networkManager);
        networkManager.setCookieJar(jar);
    }

    // An arbitrary birthdate comfortably over 18/21 depending on region,
    // plus the two flags Steam's own age-gate form sets on submission.
    const QDateTime birth(QDate(1990, 1, 1).startOfDay());
    const QList<QNetworkCookie> cookies{
        QNetworkCookie("birthtime", QByteArray::number(birth.toSecsSinceEpoch())),
        QNetworkCookie("lastagecheckage", "1-January-1990"),
        QNetworkCookie("wants_mature_content", "1"),
        QNetworkCookie("mature_content", "1"),
    };

    const QUrl storeUrl(QStringLiteral("https://store.steampowered.com"));
    for (auto cookie : cookies) {
        cookie.setDomain(QStringLiteral(".steampowered.com"));
        cookie.setPath(QStringLiteral("/"));
        jar->insertCookie(cookie);
    }
    Q_UNUSED(storeUrl);
}

} // namespace ssv::SteamAgeGate
