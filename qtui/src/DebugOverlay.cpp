#include "DebugOverlay.h"
#include "config/Config.h"
#include "config/ConfigPaths.h"
#include "util/Logging.h"

#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QMetaObject>
#include <QPainter>
#include <QProcessEnvironment>

#include <algorithm>

namespace ssv {

bool DebugOverlay::isEnabled()
{
    // Primary, user-facing toggle: Settings > Playback > "Show debug log
    // overlay" (advanced.debugOverlay), off by default.
    if (Config::load().advanced.debugOverlay)
        return true;

    // Two more ways to force it on without touching saved settings, kept
    // around as developer/troubleshooting escape hatches: a marker file
    // (works regardless of which process tree launches this app — nothing
    // that does in practice, winlogon/Display Settings, DisplayFusion,
    // xscreensaver's driver, inherits a shell's env vars, so an env-var-only
    // toggle is invisible outside of manual testing from a terminal), and
    // the env var itself for quick shell-launched testing.
    if (QFileInfo::exists(ConfigPaths::debugOverlayFlagPath()))
        return true;
    return !QProcessEnvironment::systemEnvironment().value(QStringLiteral("SSV_DEBUG_OVERLAY")).isEmpty();
}

DebugOverlay::DebugOverlay(QWidget* parent) : QWidget(parent)
{
    // Never intercepts input: the whole point is to observe without
    // interfering (mouse-move-to-exit in particular must keep working
    // exactly as if this weren't here).
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setGeometry(parent->rect());
    raise();
    show();

    m_logListenerToken = LogFeed::instance().addListener([this](const QString&) {
        QMetaObject::invokeMethod(this, &DebugOverlay::refresh, Qt::QueuedConnection);
    });
    refresh();
}

DebugOverlay::~DebugOverlay()
{
    LogFeed::instance().removeListener(m_logListenerToken);
}

void DebugOverlay::refresh()
{
    update();
}

void DebugOverlay::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(0, 0, 0, 170));

    const QFont font(QStringLiteral("Consolas"), 10);
    painter.setFont(font);
    const QFontMetrics fm(font);
    const int lineHeight = fm.height() + 2;
    const int maxLines = std::max(1, (height() - 16) / lineHeight);

    const auto lines = LogFeed::instance().lines();
    const int start = std::max(0, static_cast<int>(lines.size()) - maxLines);

    int y = 8 + fm.ascent();
    painter.setPen(Qt::white);
    for (int i = start; i < lines.size(); ++i) {
        painter.drawText(8, y, lines.at(i));
        y += lineHeight;
    }

    if (lines.isEmpty()) {
        painter.setPen(Qt::lightGray);
        painter.drawText(8, 8 + fm.ascent(), QStringLiteral("SSV_DEBUG_OVERLAY: waiting for log output..."));
    }
}

} // namespace ssv
