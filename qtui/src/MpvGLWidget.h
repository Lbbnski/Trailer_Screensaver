#pragma once

#include "playback/MpvGlRenderer.h"
#include "playback/MpvPlayer.h"

#include <QOpenGLWidget>

namespace ssv {

// The one libmpv/Qt-GL integration point shared by fullscreen playback,
// Windows preview embedding (/p HWND, via EmbeddedForeignWindow), the
// Linux xscreensaver hack's -window-id embedding, and the settings
// dialog's live preview pane. Owns an MpvPlayer (playback control) and an
// MpvGlRenderer (render-API/GL glue) and wires them to Qt's GL widget
// lifecycle, following the pattern in mpv's own
// mpv-examples/libmpv/qt_opengl sample.
class MpvGLWidget : public QOpenGLWidget {
    Q_OBJECT
public:
    explicit MpvGLWidget(QWidget* parent = nullptr);
    ~MpvGLWidget() override;

    // Creates and initializes the underlying MpvPlayer. Must be called
    // before the widget is shown (initializeGL() depends on the player
    // already having a valid mpv_handle to attach the render context to).
    // Returns false on failure.
    bool initializePlayer(const QString& ytDlpPath);

    MpvPlayer* player() const;

protected:
    void initializeGL() override;
    void paintGL() override;

private:
    static void* getProcAddressTrampoline(void* ctx, const char* name);

    MpvPlayer* m_player = nullptr;
    MpvGlRenderer* m_renderer = nullptr;
};

} // namespace ssv
