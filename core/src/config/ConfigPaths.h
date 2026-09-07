#pragma once

#include <QString>

namespace ssv::ConfigPaths {

// %LOCALAPPDATA%\SteamTrailerScreensaver\config.json on Windows,
// $XDG_CONFIG_HOME/steam-trailer-screensaver/config.json (falling back to
// ~/.config/...) on Linux.
QString configFilePath();

// Same directory family as configFilePath() but for the SQLite cache:
// ...\SteamTrailerScreensaver\cache.sqlite3 on Windows,
// $XDG_CACHE_HOME/steam-trailer-screensaver/cache.sqlite3 (falling back to
// ~/.cache/...) on Linux.
QString cacheDatabasePath();

// Same directory family, for the diagnostic log file (see util/Logging.h)
// — the only way to see failures from any of the three GUI-subsystem
// executables, none of which has a console attached.
QString logFilePath();

// Same directory family: presence of this (empty, content-ignored) file
// turns on the on-screen debug log overlay (see qtui/src/DebugOverlay.h).
// A marker file rather than an environment variable specifically because
// nothing that actually launches this app in practice — winlogon/Display
// Settings for `/s` and `/p`, DisplayFusion, xscreensaver's driver —
// inherits environment variables set in a user's own shell.
QString debugOverlayFlagPath();

// Same directory family: a dedicated JSON-Lines diagnostic trace (distinct
// from logFilePath()'s plain-text app log) recording, for every video
// YoutubeFallbackResolver evaluates, whatever metadata yt-dlp reported about
// it and the decision made — see util/Logging.h's appendDiagnosticRecord().
// Kept separate so this (comparatively verbose, structured) trace can be
// read/parsed on its own without wading through ordinary log lines.
QString youtubeDiagnosticsLogPath();

} // namespace ssv::ConfigPaths
