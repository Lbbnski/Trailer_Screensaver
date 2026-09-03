#pragma once

#include "sources/SourceTypes.h"

#include <QString>
#include <QStringList>

namespace ssv {

enum class MaxResolution { P480, Max };
enum class MonitorMode { Independent, PrimaryOnly };

struct PlaybackConfig {
    MaxResolution maxResolution = MaxResolution::Max;
    MonitorMode monitorMode = MonitorMode::Independent;
    bool muted = true;
    bool hardwareDecode = true;
};

struct FilterConfig {
    GenreFilter::Mode mode = GenreFilter::Mode::AllowList;
    QStringList genres;                     // canonical genre names; empty allow-list = "everything"
    int maxAge = 18;
    QStringList blockedContentDescriptors;   // e.g. "adult-only" — excluded regardless of maxAge
    bool preferPopular = false;              // see GenreFilter::preferPopular
};

struct SourcesConfig {
    QStringList enabled{QStringLiteral("steam")};
    QString steamLanguage = QStringLiteral("english");
    QString steamCountryCode = QStringLiteral("US");

    // A free Twitch developer app's credentials — required for "igdb" in
    // `enabled` to actually contribute anything (see
    // PlaybackSession::start()); both empty by default since this needs
    // manual per-user setup, same as advanced.ytDlpPath needing yt-dlp
    // installed separately.
    QString igdbClientId;
    QString igdbClientSecret;
};

struct AdvancedConfig {
    int cacheTtlDaysAppDetails = 21;
    int cacheTtlDaysCandidateList = 7;
    int maxCatalogRequestsPerRun = 40;
    QString ytDlpPath = QStringLiteral("yt-dlp");

    // Rejects YouTube-fallback search results longer than this — without
    // it, a search can just as easily land on a Let's Play or full
    // walkthrough as an actual trailer. 10 minutes comfortably covers even
    // long cinematic reveal trailers while still excluding that kind of
    // content.
    int maxTrailerDurationSeconds = 600;

    // Shows a live tail of the app log on top of the video (see
    // qtui/src/DebugOverlay.h) — a troubleshooting aid, off by default so
    // normal use never shows it.
    bool debugOverlay = false;
};

// Root config object, persisted as JSON at ConfigPaths::configFilePath().
// See docs/ARCHITECTURE.md for the on-disk shape this (de)serializes to/from.
struct Config {
    static constexpr int kCurrentVersion = 1;

    int version = kCurrentVersion;
    PlaybackConfig playback;
    FilterConfig filter;
    SourcesConfig sources;
    AdvancedConfig advanced;

    // Loads from ConfigPaths::configFilePath(), returning defaults (and
    // writing nothing) if the file doesn't exist yet or fails to parse.
    static Config load();

    // Writes to ConfigPaths::configFilePath(), creating parent directories
    // as needed. Returns false on I/O failure.
    bool save() const;

    QByteArray toJson() const;
    static Config fromJson(const QByteArray& json, bool* ok = nullptr);
};

} // namespace ssv
