#pragma once

#include <QWidget>

namespace ssv {

// A semi-transparent, click-through panel showing the tail of the app's
// live log (see util/Logging.h's LogFeed), layered above whatever else is
// in the window it's added to. Opt-in via DebugOverlay::isEnabled(), which
// checks the SSV_DEBUG_OVERLAY environment variable — deliberately not a
// persisted Config/Settings option, since this is a developer/troubleshoot
// tool, not a user-facing feature.
class DebugOverlay : public QWidget {
    Q_OBJECT
public:
    // True if SSV_DEBUG_OVERLAY is set to anything non-empty.
    static bool isEnabled();

    explicit DebugOverlay(QWidget* parent);
    ~DebugOverlay() override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void refresh();

    int m_logListenerToken = -1;
};

} // namespace ssv
