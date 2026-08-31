#pragma once

#include <QObject>
#include <QString>

struct mpv_handle;

namespace ssv {

// Thin wrapper over libmpv's client API: creation/option setup, issuing
// commands (loadfile, ...), and pumping mpv's event queue into Qt signals.
// Deliberately separate from MpvGlRenderer (the render-API/GL half) so this
// class has no GL/windowing concerns and MpvGlRenderer has no
// playback-control concerns — MpvGLWidget (in qtui) owns one of each and
// wires them together.
class MpvPlayer : public QObject {
    Q_OBJECT
public:
    explicit MpvPlayer(QObject* parent = nullptr);
    ~MpvPlayer() override;

    // Creates and initializes the underlying mpv_handle with the app's
    // baseline options (keep-open=no so end-of-file fires reliably for
    // auto-advance, ytdl_hook wired to the configured yt-dlp path).
    // Returns false on failure (check log output for the mpv error).
    bool initialize(const QString& ytDlpPath);

    mpv_handle* handle() const;

    void loadFile(const QString& url);
    void setMuted(bool muted);
    void setHardwareDecodeEnabled(bool enabled);

    // Height cap applied via mpv's ytdl-format option — this is what
    // actually enforces the user's resolution setting for YouTube-fallback
    // renditions (Steam CDN renditions are instead capped by which
    // TrailerRendition gets chosen before loadFile() is even called; see
    // PlaylistEngine/TrailerResolver).
    void setYoutubeHeightCap(int maxHeight);

signals:
    void playbackEnded();
    void errorOccurred(QString message);

private:
    void processMpvEvents();
    static void wakeupCallbackTrampoline(void* ctx);

    mpv_handle* m_mpv = nullptr;
};

} // namespace ssv
