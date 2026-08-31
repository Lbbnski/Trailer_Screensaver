#include "MpvGLWidget.h"
#include "util/Logging.h"

#include <QOpenGLContext>

namespace ssv {

MpvGLWidget::MpvGLWidget(QWidget* parent) : QOpenGLWidget(parent)
{
    m_player = new MpvPlayer(this);
}

MpvGLWidget::~MpvGLWidget()
{
    // Tear down the GL-dependent renderer while our context is still
    // current; MpvPlayer (parented to `this`) is destroyed afterwards by
    // Qt's normal child cleanup.
    makeCurrent();
    delete m_renderer;
    m_renderer = nullptr;
    doneCurrent();
}

bool MpvGLWidget::initializePlayer(const QString& ytDlpPath)
{
    return m_player->initialize(ytDlpPath);
}

MpvPlayer* MpvGLWidget::player() const
{
    return m_player;
}

void* MpvGLWidget::getProcAddressTrampoline(void* ctx, const char* name)
{
    Q_UNUSED(ctx);
    auto* glContext = QOpenGLContext::currentContext();
    return glContext ? reinterpret_cast<void*>(glContext->getProcAddress(QByteArray(name))) : nullptr;
}

void MpvGLWidget::initializeGL()
{
    // initializeGL() runs exactly once per widget lifetime (Qt calls it on
    // the first paint after the GL context is created), so if
    // initializePlayer() hasn't run yet at that point, this widget will
    // never get a renderer at all — not "later", never. Every caller must
    // call initializePlayer() before the widget is ever shown/embedded.
    if (!m_player->handle()) {
        logError(QStringLiteral("MpvGLWidget::initializeGL: no mpv handle yet — "
                                 "initializePlayer() must be called before this widget is shown"));
        return;
    }

    m_renderer = new MpvGlRenderer(this);
    if (!m_renderer->initialize(m_player->handle(), &MpvGLWidget::getProcAddressTrampoline, this)) {
        logError(QStringLiteral("MpvGLWidget::initializeGL: MpvGlRenderer::initialize failed"));
        delete m_renderer;
        m_renderer = nullptr;
        return;
    }

    connect(m_renderer, &MpvGlRenderer::frameReady, this, QOverload<>::of(&QOpenGLWidget::update));
}

void MpvGLWidget::paintGL()
{
    if (!m_renderer)
        return;
    const qreal dpr = devicePixelRatioF();
    m_renderer->render(defaultFramebufferObject(), int(width() * dpr), int(height() * dpr));
}

} // namespace ssv
