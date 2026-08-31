#pragma once

#include <QString>
#include <QStringList>

namespace ssv {

struct ProcessResult {
    bool started = false;
    int exitCode = -1;
    QByteArray stdOut;
    QByteArray stdErr;

    bool ok() const { return started && exitCode == 0; }
};

// Thin QProcess wrapper for shelling out to yt-dlp. Blocks the calling
// thread until the process exits or `timeoutMs` elapses — used from the
// same background worker thread as the network calls in
// SteamAppDetailsClient/SteamGenreCandidateFinder, never from the UI or
// playback thread.
namespace ProcessRunner {

ProcessResult run(const QString& program, const QStringList& arguments, int timeoutMs = 15000);

} // namespace ProcessRunner

} // namespace ssv
