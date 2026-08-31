#pragma once

#include <QEventLoop>
#include <QNetworkReply>
#include <QTimer>

namespace ssv {

// Blocks the calling thread's event loop until `reply` finishes or
// `timeoutMs` elapses, returning true on completion (caller still must
// check reply->error()) and false on timeout (reply is aborted).
//
// This project intentionally does its Steam/network fetching on a
// dedicated worker thread (see TrailerResolver), each with its own
// QNetworkAccessManager and its own Qt event loop — blocking *that*
// thread's loop while a single small JSON request completes keeps
// SteamAppDetailsClient/SteamGenreCandidateFinder/YoutubeFallbackResolver
// straightforward call/return code instead of a callback pyramid, without
// blocking the UI or playback thread.
inline bool awaitReply(QNetworkReply* reply, int timeoutMs = 10000)
{
    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    bool timedOut = false;
    QObject::connect(&timeoutTimer, &QTimer::timeout, [&]() {
        timedOut = true;
        reply->abort();
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    timeoutTimer.start(timeoutMs);
    loop.exec();

    return !timedOut;
}

} // namespace ssv
