#pragma once

#include "cache/CacheDatabase.h"
#include "cache/CacheRepository.h"
#include "config/Config.h"
#include "selection/PlaylistEngine.h"

#include <QWidget>
#include <QtGui/qwindowdefs.h>

#include <memory>

namespace ssv {

class MpvGLWidget;

// Handles the `/p HWND` contract: embeds into the small preview thumbnail
// Windows' Display Settings screensaver dropdown shows. Reuses
// EmbeddedForeignWindow (shared with the Linux xscreensaver hack's
// -window-id embedding) plus MpvGLWidget for actual playback, but
// deliberately plays from cache only — the preview can be repainted often
// while a user browses Display Settings, so it must never trigger network
// activity of its own (no SourceRegistry/TrailerResolver involved here;
// this reads directly from the cache CacheRepository already has).
class PreviewEmbedWindow : public QWidget {
public:
    explicit PreviewEmbedWindow(WId previewHwnd, QWidget* parent = nullptr);

private:
    void loadCachedPreviewClip();

    Config m_config;
    CacheDatabase m_cacheDb;
    std::unique_ptr<CacheRepository> m_repo;
    std::unique_ptr<PlaylistEngine> m_playlistEngine;
    MpvGLWidget* m_mpvWidget = nullptr;
};

} // namespace ssv
