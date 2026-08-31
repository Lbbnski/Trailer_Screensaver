// Entry point for the Windows .scr screensaver contract. This target only
// ever builds for WIN32 (see the root CMakeLists' BUILD_WIN_SCR guard).
#include "FullscreenController.h"
#include "PreviewEmbedWindow.h"
#include "ScrArgParser.h"
#include "SettingsDialog.h"
#include "util/Logging.h"

#include <QApplication>

#include <windows.h>
#include <shellapi.h>

namespace {

// Parsed independently of QApplication's own argv handling (rather than
// QApplication::arguments()) so this stays correct regardless of how Qt's
// argument parsing might treat "/c:1234" or "/p 1234"-shaped tokens — the
// Windows screensaver contract's switches are exactly what a real
// screensaver host (Display Settings, the preview thumbnail) will pass.
QStringList windowsCommandLineArgs()
{
    int wargc = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);

    QStringList args;
    for (int i = 1; i < wargc; ++i) // skip argv[0], the executable path
        args << QString::fromWCharArray(wargv[i]);

    if (wargv)
        LocalFree(wargv);
    return args;
}

} // namespace

int main(int argc, char** argv)
{
    ssv::installFileLogging();

    const QStringList rawArgs = windowsCommandLineArgs();

    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("SteamTrailerScreensaver"));
    QApplication::setApplicationName(QStringLiteral("SteamTrailerScreensaver"));

    const auto scrArgs = ssv::parseScrArgs(rawArgs);

    switch (scrArgs.mode) {
    case ssv::ScrMode::RunFullscreen: {
        auto* controller = new ssv::FullscreenController(&app);
        controller->start();
        return app.exec();
    }
    case ssv::ScrMode::Preview: {
        if (scrArgs.targetWindow == 0)
            return 0; // nothing to embed into — nothing to do
        new ssv::PreviewEmbedWindow(scrArgs.targetWindow);
        return app.exec();
    }
    case ssv::ScrMode::Configure:
    default: {
        ssv::SettingsDialog dialog;
        dialog.exec();
        return 0;
    }
    }
}
