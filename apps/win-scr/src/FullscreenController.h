#pragma once

#include <QList>
#include <QObject>

#include <memory>

namespace ssv {

class FullscreenWindow;
class InputExitWatcher;
class PlaybackSession;

// Owns the full playback session for `/s`: one FullscreenWindow per monitor
// (or just the primary, per playback.monitorMode), backed by a single
// shared PlaybackSession (qtui) that drives the data pipeline and
// auto-advance for whichever windows are attached to it.
class FullscreenController : public QObject {
    Q_OBJECT
public:
    explicit FullscreenController(QObject* parent = nullptr);
    ~FullscreenController() override;

    // Loads config, opens the cache, builds the initial playlist, creates
    // one window per monitor, and starts playback. Windows are shown
    // fullscreen immediately; a cache-first playlist means playback
    // typically starts without waiting on network.
    void start();

private:
    PlaybackSession* m_session = nullptr;
    QList<FullscreenWindow*> m_windows;
    std::unique_ptr<InputExitWatcher> m_inputExitWatcher;
};

} // namespace ssv
