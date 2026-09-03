#pragma once

#include "cache/CacheDatabase.h"
#include "cache/CacheRepository.h"
#include "config/Config.h"
#include "selection/PlaylistEngine.h"
#include "sources/SourceRegistry.h"
#include "trailer/TrailerResolver.h"

#include <QObject>

#include <memory>
#include <optional>

class QNetworkAccessManager;

namespace ssv {

class MpvGLWidget;
class YoutubeFallbackResolver;

// The data-and-playback pipeline shared by both OS integrations: loads
// config, opens the cache, registers the enabled IMetadataSource
// implementations, builds the initial playlist, and drives auto-advance
// for one or more attached MpvGLWidgets. Used by win-scr's
// FullscreenController (one session, one widget per monitor, all drawing
// from the same shared shuffled playlist) and by the xscreensaver hack's
// main.cpp (one session, one embedded widget) so the playlist-advance logic
// — including the bounded-retry-on-unplayable-candidate handling — exists
// in exactly one place rather than being duplicated per OS integration.
class PlaybackSession : public QObject {
    Q_OBJECT
public:
    explicit PlaybackSession(QObject* parent = nullptr);
    ~PlaybackSession() override;

    // Opens the cache database, registers sources, and builds the initial
    // playlist. Returns false only on an unrecoverable failure (the cache
    // database couldn't be opened at all) — an empty playlist (e.g. first
    // run, no cache yet, filter matches nothing) is not a failure here;
    // attach()'s advance() just has nothing to play until a later run
    // grows the cache.
    //
    // If `configOverride` is given, it's used instead of Config::load() —
    // the xscreensaver hack uses this to layer its capplet-XML-sourced
    // flags (see HackArgParser) on top of the on-disk config for this run
    // only, without persisting them (SettingsDialog's saved config remains
    // the baseline both OS integrations agree on).
    bool start(std::optional<Config> configOverride = std::nullopt);

    const Config& config() const;

    // Initializes `widget`'s MpvPlayer and begins playing the next playlist
    // entry into it, auto-advancing on playbackEnded for as long as the
    // session lives. Multiple attached widgets share one playlist
    // cursor — each attach()ed widget gets the next distinct entry, which
    // is what makes independent-per-monitor playback show different
    // trailers per monitor rather than the same one repeated.
    void attach(MpvGLWidget* widget);

private:
    void advance(MpvGLWidget* widget);
    void loadIntoPlayer(MpvGLWidget* widget, const TrailerCandidate& candidate, const TrailerRendition& rendition);

    // Resolving a candidate's playable rendition can mean a Steam API call
    // and/or a blocking yt-dlp subprocess invocation, each taking a couple
    // of seconds — doing that only *after* the current trailer ends is
    // what causes a visible black gap between trailers. schedulePrefetch()
    // arms a one-shot timer a few seconds into the *current* trailer to do
    // that resolution work for the *next* one ahead of time, so by the
    // time playback actually ends there's (usually) nothing left to wait
    // on. This trades a brief freeze-frame partway through the current
    // trailer (while the resolution call blocks this thread) for
    // eliminating the larger gap at the transition — a net improvement
    // since the freeze is far less noticeable mid-playback than a black
    // screen at the cut.
    void schedulePrefetch();
    void runPrefetch();

    struct Prefetch {
        int playlistIndex = -1;
        TrailerCandidate candidate;
        TrailerRendition rendition;
    };
    std::optional<Prefetch> m_prefetch;
    bool m_prefetchPending = false;

    Config m_config;
    CacheDatabase m_cacheDb;
    std::unique_ptr<CacheRepository> m_repo;
    QNetworkAccessManager* m_networkManager = nullptr;
    SourceRegistry m_registry;
    std::unique_ptr<YoutubeFallbackResolver> m_youtubeFallback;
    std::unique_ptr<TrailerResolver> m_resolver;
    std::unique_ptr<PlaylistEngine> m_playlistEngine;

    QList<TrailerCandidate> m_playlist;
    int m_playlistIndex = 0;
};

} // namespace ssv
