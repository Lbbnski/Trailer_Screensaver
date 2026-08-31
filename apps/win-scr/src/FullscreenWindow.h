#pragma once

#include <QWidget>

class QScreen;

namespace ssv {

class MpvGLWidget;

// One borderless, always-on-top, screen-filling window per monitor. When
// `playsVideo` is true it hosts an MpvGLWidget; when false (used for
// non-primary monitors under playback.monitorMode == PrimaryOnly) it's just
// a black surface, so every monitor is still covered without paying for an
// extra concurrent decode/stream.
class FullscreenWindow : public QWidget {
    Q_OBJECT
public:
    FullscreenWindow(QScreen* screen, bool playsVideo, QWidget* parent = nullptr);

    // nullptr if this window was constructed with playsVideo == false.
    MpvGLWidget* mpvWidget() const;

private:
    MpvGLWidget* m_mpvWidget = nullptr;
};

} // namespace ssv
