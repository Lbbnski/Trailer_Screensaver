#include "playback/MpvGlRenderer.h"
#include "util/Logging.h"

#include <mpv/client.h>
#include <mpv/render_gl.h>

#include <QMetaObject>

namespace ssv {

MpvGlRenderer::MpvGlRenderer(QObject* parent) : QObject(parent) {}

MpvGlRenderer::~MpvGlRenderer()
{
    if (m_renderContext)
        mpv_render_context_free(m_renderContext);
}

bool MpvGlRenderer::initialize(mpv_handle* mpv, GetProcAddressFn getProcAddress, void* getProcAddressCtx)
{
    mpv_opengl_init_params glInitParams{getProcAddress, getProcAddressCtx};

    // Deliberately not requesting MPV_RENDER_PARAM_ADVANCED_CONTROL: that
    // mode requires also calling mpv_render_context_report_swap() after
    // each present and handling MPV_RENDER_UPDATE_FRAME polling manually,
    // which this simple update-callback-drives-Qt's-update() loop doesn't
    // do. Plain (non-advanced) rendering is a correct match for that
    // simpler loop.
    mpv_render_param params[]{
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &glInitParams},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };

    const int status = mpv_render_context_create(&m_renderContext, mpv, params);
    if (status < 0) {
        logError(QStringLiteral("mpv_render_context_create failed: %1").arg(mpv_error_string(status)));
        m_renderContext = nullptr;
        return false;
    }

    mpv_render_context_set_update_callback(m_renderContext, &MpvGlRenderer::updateCallbackTrampoline, this);
    return true;
}

void MpvGlRenderer::render(unsigned int fbo, int width, int height, bool flipY)
{
    if (!m_renderContext)
        return;

    mpv_opengl_fbo mpvFbo{static_cast<int>(fbo), width, height, 0};
    int flip = flipY ? 1 : 0;

    mpv_render_param params[]{
        {MPV_RENDER_PARAM_OPENGL_FBO, &mpvFbo},
        {MPV_RENDER_PARAM_FLIP_Y, &flip},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };
    mpv_render_context_render(m_renderContext, params);
}

void MpvGlRenderer::updateCallbackTrampoline(void* ctx)
{
    auto* self = static_cast<MpvGlRenderer*>(ctx);
    QMetaObject::invokeMethod(self, &MpvGlRenderer::frameReady, Qt::QueuedConnection);
}

} // namespace ssv
