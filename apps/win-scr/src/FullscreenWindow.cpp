#include "FullscreenWindow.h"

#include "DebugOverlay.h"
#include "MpvGLWidget.h"

#include <QScreen>
#include <QVBoxLayout>

namespace ssv {

FullscreenWindow::FullscreenWindow(QScreen* screen, bool playsVideo, QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setCursor(Qt::BlankCursor);
    setGeometry(screen->geometry());

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    if (playsVideo) {
        m_mpvWidget = new MpvGLWidget(this);
        layout->addWidget(m_mpvWidget);
    } else {
        setStyleSheet(QStringLiteral("background-color: black;"));
    }

    if (DebugOverlay::isEnabled())
        new DebugOverlay(this);
}

MpvGLWidget* FullscreenWindow::mpvWidget() const
{
    return m_mpvWidget;
}

} // namespace ssv
