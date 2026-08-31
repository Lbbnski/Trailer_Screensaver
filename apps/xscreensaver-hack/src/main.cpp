// Entry point for the Linux xscreensaver hack. Only ever meaningfully used
// under a real X11 session with xscreensaver's driver running — see
// docs/LIMITATIONS.md for why GNOME/Wayland isn't supported here.
#include "DebugOverlay.h"
#include "EmbeddedForeignWindow.h"
#include "HackArgParser.h"
#include "MpvGLWidget.h"
#include "PlaybackSession.h"
#include "SettingsDialog.h"
#include "config/Config.h"
#include "util/Logging.h"

#include <QApplication>
#include <QProcessEnvironment>

int main(int argc, char** argv)
{
    ssv::installFileLogging();

    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("SteamTrailerScreensaver"));
    QApplication::setApplicationName(QStringLiteral("steam-trailer-saver"));

    QStringList args;
    for (int i = 1; i < argc; ++i)
        args << QString::fromLocal8Bit(argv[i]);

    const auto hackArgs = ssv::parseHackArgs(args, QProcessEnvironment::systemEnvironment());

    if (hackArgs.mode == ssv::HackMode::Configure) {
        // The .xml capplet only exposes the settings that fit xscreensaver's
        // simple widget schema (see config/steam-trailer-saver.xml); this
        // flag is the escape hatch to the full genre allow/block-list UI.
        ssv::SettingsDialog dialog;
        dialog.exec();
        return 0;
    }

    auto* mpvWidget = new ssv::MpvGLWidget(nullptr);
    mpvWidget->setWindowFlags(Qt::FramelessWindowHint);

    // Layer the capplet's own flags (see config/steam-trailer-saver.xml) on
    // top of the saved config for this run only — see PlaybackSession::start
    // and HackArgParser::HackArgs for why these aren't persisted here.
    ssv::Config cfg = ssv::Config::load();
    if (hackArgs.use480p)
        cfg.playback.maxResolution = *hackArgs.use480p ? ssv::MaxResolution::P480 : ssv::MaxResolution::Max;
    if (hackArgs.maxAge)
        cfg.filter.maxAge = *hackArgs.maxAge;
    if (hackArgs.blacklistMode)
        cfg.filter.mode = *hackArgs.blacklistMode ? ssv::GenreFilter::Mode::BlockList : ssv::GenreFilter::Mode::AllowList;
    if (hackArgs.genres)
        cfg.filter.genres = *hackArgs.genres;

    auto* session = new ssv::PlaybackSession(&app);
    if (!session->start(cfg))
        return 1;

    // attach() (which calls initializePlayer()) must happen before the
    // widget is embedded/shown: MpvGLWidget::initializeGL() runs on first
    // paint and only ever gets one chance to find a valid mpv handle.
    session->attach(mpvWidget);

    if (!ssv::EmbeddedForeignWindow::embed(mpvWidget, hackArgs.windowId))
        return 1;

    if (ssv::DebugOverlay::isEnabled())
        new ssv::DebugOverlay(mpvWidget);

    return app.exec();
}
