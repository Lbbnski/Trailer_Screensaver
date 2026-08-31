#include "PreviewEmbedWindow.h"

#include "DebugOverlay.h"
#include "EmbeddedForeignWindow.h"
#include "MpvGLWidget.h"
#include "config/ConfigPaths.h"

#include <QDateTime>
#include <QVBoxLayout>

namespace ssv {

PreviewEmbedWindow::PreviewEmbedWindow(WId previewHwnd, QWidget* parent) : QWidget(parent)
{
    // This widget is about to be reparented under the foreign preview HWND
    // rather than shown as its own top-level window, so it must carry no
    // title bar/border of its own.
    setWindowFlags(Qt::FramelessWindowHint);

    m_config = Config::load();
    m_cacheDb = CacheDatabase::open(ConfigPaths::cacheDatabasePath());
    if (m_cacheDb.isOpen()) {
        m_repo = std::make_unique<CacheRepository>(m_cacheDb);
        m_playlistEngine = std::make_unique<PlaylistEngine>(*m_repo);
    }

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_mpvWidget = new MpvGLWidget(this);
    layout->addWidget(m_mpvWidget);

    // Must happen before embed() (which shows the widget): MpvGLWidget's
    // initializeGL() runs on first paint and only ever gets one chance to
    // find a valid mpv handle — showing the widget before the player is
    // initialized means it never gets a renderer at all.
    m_mpvWidget->initializePlayer(m_config.advanced.ytDlpPath);

    if (!EmbeddedForeignWindow::embed(this, previewHwnd))
        return;

    if (DebugOverlay::isEnabled())
        new DebugOverlay(this);

    loadCachedPreviewClip();
}

void PreviewEmbedWindow::loadCachedPreviewClip()
{
    if (!m_repo || !m_playlistEngine || !m_mpvWidget)
        return;

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    QList<TrailerCandidate> pool;
    for (const auto& sourceId : m_config.sources.enabled)
        pool << m_repo->freshAppDetails(sourceId, now);

    const auto playlist = m_playlistEngine->buildPlaylist(pool, m_config.filter, /*noRepeatWindowSeconds=*/0);
    for (const auto& candidate : playlist) {
        if (candidate.renditions.isEmpty())
            continue; // preview never triggers a fallback resolution (no network)
        auto* player = m_mpvWidget->player();
        player->setMuted(true); // the preview thumbnail is always silent
        player->loadFile(candidate.renditions.first().url);
        return;
    }
}

} // namespace ssv
