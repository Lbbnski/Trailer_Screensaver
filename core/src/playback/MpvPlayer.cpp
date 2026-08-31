#include "playback/MpvPlayer.h"
#include "util/Logging.h"

#include <mpv/client.h>

#include <QMetaObject>

namespace ssv {

namespace {

void setOption(mpv_handle* mpv, const char* name, const char* value)
{
    mpv_set_option_string(mpv, name, value);
}

} // namespace

MpvPlayer::MpvPlayer(QObject* parent) : QObject(parent) {}

MpvPlayer::~MpvPlayer()
{
    if (m_mpv)
        mpv_terminate_destroy(m_mpv);
}

bool MpvPlayer::initialize(const QString& ytDlpPath)
{
    m_mpv = mpv_create();
    if (!m_mpv) {
        emit errorOccurred(QStringLiteral("mpv_create failed"));
        return false;
    }

    // Render exclusively through the libmpv render API (MpvGlRenderer,
    // driven by MpvGLWidget) instead of mpv's normal video output, which
    // otherwise opens its own separate native window — unrelated to and
    // untracked by our own FullscreenWindow/PreviewEmbedWindow, so it
    // never renders anything into our embedded widget and never responds
    // to this app's own close/quit handling. Must be set before
    // mpv_initialize().
    setOption(m_mpv, "vo", "libmpv");

    // Let a file play to completion and fire MPV_EVENT_END_FILE instead of
    // pausing on the last frame — that event is what drives auto-advance to
    // the next trailer.
    setOption(m_mpv, "keep-open", "no");
    setOption(m_mpv, "idle", "yes");

    // ytdl_hook is what lets a plain youtube.com/watch?v=... URL passed to
    // loadFile() resolve to a fresh, non-expired stream URL every time
    // (see YoutubeFallbackResolver) — we point it at the configured yt-dlp
    // binary rather than assuming one named exactly "youtube-dl"/"yt-dlp"
    // is first on PATH.
    setOption(m_mpv, "script-opts", qPrintable(QStringLiteral("ytdl_hook-ytdl_path=%1").arg(ytDlpPath)));

    setOption(m_mpv, "hwdec", "auto-safe");
    setOption(m_mpv, "mute", "yes");

    mpv_set_wakeup_callback(m_mpv, &MpvPlayer::wakeupCallbackTrampoline, this);

    const int status = mpv_initialize(m_mpv);
    if (status < 0) {
        emit errorOccurred(QStringLiteral("mpv_initialize failed: %1").arg(mpv_error_string(status)));
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
        return false;
    }

    // Without this, mpv never emits MPV_EVENT_LOG_MESSAGE at all — the
    // handler in processMpvEvents() for that event existed but could never
    // fire, so real mpv-side diagnostics (codec/render/vo negotiation
    // failures) were silently invisible.
    mpv_request_log_messages(m_mpv, "warn");

    return true;
}

mpv_handle* MpvPlayer::handle() const
{
    return m_mpv;
}

void MpvPlayer::loadFile(const QString& url)
{
    if (!m_mpv)
        return;
    const QByteArray utf8 = url.toUtf8();
    const char* args[] = {"loadfile", utf8.constData(), nullptr};
    mpv_command_async(m_mpv, 0, args);
}

void MpvPlayer::setMuted(bool muted)
{
    if (!m_mpv)
        return;
    int flag = muted ? 1 : 0;
    mpv_set_property(m_mpv, "mute", MPV_FORMAT_FLAG, &flag);
}

void MpvPlayer::setHardwareDecodeEnabled(bool enabled)
{
    if (!m_mpv)
        return;
    setOption(m_mpv, "hwdec", enabled ? "auto-safe" : "no");
}

void MpvPlayer::setYoutubeHeightCap(int maxHeight)
{
    if (!m_mpv || maxHeight <= 0)
        return;
    const QString format = QStringLiteral("bestvideo[height<=?%1]+bestaudio/best[height<=?%1]").arg(maxHeight);
    mpv_set_option_string(m_mpv, "ytdl-format", qPrintable(format));
}

void MpvPlayer::wakeupCallbackTrampoline(void* ctx)
{
    // Called from an mpv-internal thread — hop back onto this object's
    // thread (Qt's UI/render thread) via a queued invoke before touching
    // anything, per libmpv's own threading requirements.
    auto* self = static_cast<MpvPlayer*>(ctx);
    QMetaObject::invokeMethod(self, &MpvPlayer::processMpvEvents, Qt::QueuedConnection);
}

void MpvPlayer::processMpvEvents()
{
    if (!m_mpv)
        return;

    while (true) {
        mpv_event* event = mpv_wait_event(m_mpv, 0);
        if (event->event_id == MPV_EVENT_NONE)
            break;

        switch (event->event_id) {
        case MPV_EVENT_END_FILE: {
            // Every end-of-file (natural EOF, an outright decode/network
            // error, redirect, ...) was previously treated identically —
            // just "move to the next candidate" — with no way to tell a
            // real failure apart from a genuinely short clip finishing.
            const auto* endFile = static_cast<mpv_event_end_file*>(event->data);
            if (endFile && endFile->reason == MPV_END_FILE_REASON_ERROR) {
                logWarning(QStringLiteral("mpv: playback ended with error: %1")
                               .arg(QString::fromUtf8(mpv_error_string(endFile->error))));
            } else if (endFile) {
                logInfo(QStringLiteral("mpv: end-of-file, reason=%1").arg(static_cast<int>(endFile->reason)));
            }
            emit playbackEnded();
            break;
        }
        case MPV_EVENT_LOG_MESSAGE: {
            auto* msg = static_cast<mpv_event_log_message*>(event->data);
            logWarning(QStringLiteral("mpv: %1").arg(QString::fromUtf8(msg->text).trimmed()));
            break;
        }
        default:
            break;
        }
    }
}

} // namespace ssv
