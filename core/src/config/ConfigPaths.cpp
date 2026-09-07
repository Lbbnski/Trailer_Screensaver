#include "config/ConfigPaths.h"

#include <QDir>
#include <QStandardPaths>

namespace ssv::ConfigPaths {

namespace {

QString appDirName()
{
    return QStringLiteral("SteamTrailerScreensaver");
}

// QStandardPaths::AppConfigLocation/AppDataLocation append the
// QCoreApplication org/app name automatically *if one is set*, but our
// three executables (win-scr, xscreensaver-hack, settings-gui) are
// deliberately separate binaries that may not all set identical
// QCoreApplication metadata, so we resolve the shared directory explicitly
// instead of relying on that.
QString baseConfigDir()
{
#if defined(SSV_PLATFORM_WINDOWS)
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return QDir(base).filePath(appDirName());
#else
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return QDir(base).filePath(QStringLiteral("steam-trailer-screensaver"));
#endif
}

QString baseCacheDir()
{
#if defined(SSV_PLATFORM_WINDOWS)
    // Windows has no separate cache/config split in practice; keep the
    // cache DB alongside the config file under the same app directory.
    return baseConfigDir();
#else
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    return QDir(base).filePath(QStringLiteral("steam-trailer-screensaver"));
#endif
}

} // namespace

QString configFilePath()
{
    return QDir(baseConfigDir()).filePath(QStringLiteral("config.json"));
}

QString cacheDatabasePath()
{
    return QDir(baseCacheDir()).filePath(QStringLiteral("cache.sqlite3"));
}

QString logFilePath()
{
    return QDir(baseConfigDir()).filePath(QStringLiteral("app.log"));
}

QString debugOverlayFlagPath()
{
    return QDir(baseConfigDir()).filePath(QStringLiteral("debug_overlay.flag"));
}

QString youtubeDiagnosticsLogPath()
{
    return QDir(baseConfigDir()).filePath(QStringLiteral("youtube_candidates.jsonl"));
}

} // namespace ssv::ConfigPaths
