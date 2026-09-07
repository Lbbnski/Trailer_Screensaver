#pragma once

#include <QHash>
#include <QMutex>
#include <QString>
#include <QStringList>

#include <functional>

class QJsonObject;

namespace ssv {

// Thin wrapper over qWarning/qInfo/qDebug so call sites don't couple
// directly to Qt's logging macros and log lines get a consistent category
// prefix. Deliberately not a full logging framework — this app has no
// resident process to justify one.
void logInfo(const QString& message);
void logWarning(const QString& message);
void logError(const QString& message);

// Installs a Qt message handler that appends every log line — including
// ones from Qt's own internals (e.g. "QSqlDatabase: can not load requested
// driver 'QSQLITE'") — to ConfigPaths::logFilePath(). Call once near the
// top of main(). Without this, all three executables are WIN32-subsystem
// (or console-less-in-practice on Linux, spawned by xscreensaver's driver)
// with no attached console, so this file is the only way to ever see a
// failure after the fact.
void installFileLogging();

// Appends one JSON object (with a "ts" timestamp field added automatically)
// as a single line to `path`, for a structured diagnostic trace that's
// meant to be read back and analyzed rather than skimmed like an ordinary
// log line — e.g. YoutubeFallbackResolver recording the full yt-dlp
// metadata it saw for every candidate it evaluated. Capped the same way
// installFileLogging() caps app.log (starts over once the file gets
// unreasonably large) since nothing in this app rotates logs. Safe to call
// often; opens/appends/closes the file each time rather than holding it
// open, since this is expected to be called far less often than
// logInfo/logWarning/logError.
void appendDiagnosticRecord(const QString& path, const QJsonObject& record);

// A small in-process, thread-safe feed of the same lines written to the
// log file, capped to the most recent lines — lets a debug overlay widget
// (see qtui/src/DebugOverlay.h) show what's happening live without tailing
// the log file from outside the process. Populated automatically once
// installFileLogging() has been called.
//
// Deliberately NOT a QObject/Q_OBJECT class: ssvcore already contains a
// couple of QObject classes (MpvPlayer, MpvGlRenderer) whose moc output
// CMake's AUTOMOC bundles into one shared mocs_compilation.cpp per target.
// Since nearly every file in ssvcore calls logWarning/logError, making
// LogFeed a QObject too would mean *any* consumer of this header — even
// one that never touches playback — pulls in that whole bundled
// translation unit at link time, and with it MpvPlayer's vtable and its
// libmpv dependency. Observed directly: adding a Q_OBJECT LogFeed here
// once made the PlaylistEngine/CacheRepository unit tests (which have
// nothing to do with playback) fail to even start, because they'd
// suddenly acquired a hard dependency on libmpv-2.dll. A plain callback
// list avoids the whole problem.
class LogFeed {
public:
    static LogFeed& instance();

    // Snapshot of the buffered lines, oldest first.
    QStringList lines() const;

    void add(const QString& line);

    // Registers a callback invoked (on whatever thread calls add()) for
    // every new line from this point on. Returns a token for
    // removeListener(); listeners are never called while add()'s internal
    // lock is held.
    int addListener(std::function<void(const QString&)> listener);
    void removeListener(int token);

private:
    mutable QMutex m_mutex;
    QStringList m_lines;
    QHash<int, std::function<void(const QString&)>> m_listeners;
    int m_nextToken = 0;
};

} // namespace ssv
