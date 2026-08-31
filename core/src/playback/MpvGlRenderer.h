#pragma once

#include <QObject>

struct mpv_handle;
struct mpv_render_context;

namespace ssv {

// Wraps libmpv's render API (mpv_render_context) — the GL-frame-rendering
// half of playback, kept separate from MpvPlayer (playback control/events)
// so this class has zero knowledge of loadfile/properties/etc. and can be
// driven purely by a Qt GL widget's initializeGL/paintGL/resizeGL. Follows
// the pattern in mpv's own mpv-examples/libmpv/qt_opengl sample: render via
// MPV_RENDER_PARAM_OPENGL_FBO into a widget-owned FBO. Cross-platform with
// no #ifdefs since Qt owns GL context creation on both Windows and Linux.
class MpvGlRenderer : public QObject {
    Q_OBJECT
public:
    explicit MpvGlRenderer(QObject* parent = nullptr);
    ~MpvGlRenderer() override;

    // getProcAddress must remain valid for the renderer's lifetime — the
    // caller (MpvGLWidget) typically passes a static trampoline around
    // QOpenGLContext::getProcAddress.
    using GetProcAddressFn = void* (*)(void* ctx, const char* name);
    bool initialize(mpv_handle* mpv, GetProcAddressFn getProcAddress, void* getProcAddressCtx);

    // Renders the current video frame into the given FBO at (width, height).
    // Call from within an active GL context (i.e. from paintGL()).
    void render(unsigned int fbo, int width, int height, bool flipY = true);

signals:
    // Emitted (via a queued connection, from whatever thread mpv's internal
    // renderer-update thread runs on) whenever mpv has a new frame ready.
    // MpvGLWidget connects this to QOpenGLWidget::update().
    void frameReady();

private:
    static void updateCallbackTrampoline(void* ctx);

    mpv_render_context* m_renderContext = nullptr;
};

} // namespace ssv
