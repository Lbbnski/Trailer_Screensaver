#include "util/Logging.h"
#include "config/ConfigPaths.h"

#include <QLoggingCategory>
#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

namespace ssv {

namespace {
Q_LOGGING_CATEGORY(lcSsv, "ssv")

constexpr int kLogFeedMaxLines = 200;

QFile* g_logFile = nullptr;

const char* levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg: return "DEBUG";
    case QtInfoMsg: return "INFO";
    case QtWarningMsg: return "WARN";
    case QtCriticalMsg: return "ERROR";
    case QtFatalMsg: return "FATAL";
    }
    return "?";
}

void fileMessageHandler(QtMsgType type, const QMessageLogContext&, const QString& msg)
{
    const QString line = QDateTime::currentDateTime().toString(Qt::ISODate)
        + QStringLiteral(" [") + levelName(type) + QStringLiteral("] ") + msg;

    if (g_logFile && g_logFile->isOpen()) {
        QTextStream out(g_logFile);
        out << line << '\n';
        out.flush();
    }
    LogFeed::instance().add(line);

    if (type == QtFatalMsg)
        abort();
}

} // namespace

void logInfo(const QString& message)
{
    qCInfo(lcSsv).noquote() << message;
}

void logWarning(const QString& message)
{
    qCWarning(lcSsv).noquote() << message;
}

void logError(const QString& message)
{
    qCCritical(lcSsv).noquote() << message;
}

void installFileLogging()
{
    const QString path = ConfigPaths::logFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    // Cap growth: this app has no daemon/rotation to prune an ever-growing
    // file, so start fresh once it gets unreasonably large rather than
    // appending forever.
    if (QFileInfo(path).size() > 2 * 1024 * 1024)
        QFile::remove(path);

    g_logFile = new QFile(path);
    if (!g_logFile->open(QIODevice::Append | QIODevice::Text))
        return;

    qInstallMessageHandler(fileMessageHandler);
}

void appendDiagnosticRecord(const QString& path, const QJsonObject& record)
{
    QDir().mkpath(QFileInfo(path).absolutePath());

    // Same "start over rather than grow forever" cap as installFileLogging,
    // just larger — this trace is meant to hold enough real examples
    // (including the full per-video yt-dlp metadata) to actually analyze,
    // not just the last few lines.
    if (QFileInfo(path).size() > 10 * 1024 * 1024)
        QFile::remove(path);

    QFile file(path);
    if (!file.open(QIODevice::Append | QIODevice::Text))
        return;

    QJsonObject withTimestamp = record;
    withTimestamp.insert(QStringLiteral("ts"), QDateTime::currentDateTime().toString(Qt::ISODate));

    QTextStream out(&file);
    out << QJsonDocument(withTimestamp).toJson(QJsonDocument::Compact) << '\n';
}

LogFeed& LogFeed::instance()
{
    static LogFeed feed;
    return feed;
}

QStringList LogFeed::lines() const
{
    QMutexLocker lock(&m_mutex);
    return m_lines;
}

void LogFeed::add(const QString& line)
{
    QList<std::function<void(const QString&)>> listenersSnapshot;
    {
        QMutexLocker lock(&m_mutex);
        m_lines.append(line);
        if (m_lines.size() > kLogFeedMaxLines)
            m_lines.removeFirst();
        listenersSnapshot = m_listeners.values();
    }
    for (const auto& listener : listenersSnapshot)
        listener(line);
}

int LogFeed::addListener(std::function<void(const QString&)> listener)
{
    QMutexLocker lock(&m_mutex);
    const int token = m_nextToken++;
    m_listeners.insert(token, std::move(listener));
    return token;
}

void LogFeed::removeListener(int token)
{
    QMutexLocker lock(&m_mutex);
    m_listeners.remove(token);
}

} // namespace ssv
