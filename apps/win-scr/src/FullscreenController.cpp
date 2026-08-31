#include "FullscreenController.h"

#include "FullscreenWindow.h"
#include "InputExitWatcher.h"
#include "PlaybackSession.h"
#include "config/Config.h"

#include <QGuiApplication>
#include <QScreen>

namespace ssv {

FullscreenController::FullscreenController(QObject* parent) : QObject(parent) {}

FullscreenController::~FullscreenController()
{
    qDeleteAll(m_windows);
}

void FullscreenController::start()
{
    m_inputExitWatcher = std::make_unique<InputExitWatcher>();

    m_session = new PlaybackSession(this);
    if (!m_session->start())
        return;

    const auto screens = QGuiApplication::screens();
    const bool independent = m_session->config().playback.monitorMode == MonitorMode::Independent;

    for (int i = 0; i < screens.size(); ++i) {
        const bool playsVideo = independent || i == 0;
        auto* window = new FullscreenWindow(screens[i], playsVideo, nullptr);
        m_windows.append(window);

        // MpvGLWidget::initializeGL() runs on the widget's first paint and
        // needs initializePlayer() (called inside attach()) to have already
        // happened — it can only ever try once, so this must come before
        // showFullScreen(), not after.
        if (playsVideo)
            m_session->attach(window->mpvWidget());

        window->showFullScreen();
    }
}

} // namespace ssv
