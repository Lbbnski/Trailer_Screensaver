// Standalone settings launcher. Also the same SettingsDialog invoked from
// the Windows .scr's `/c` handler and the Linux xscreensaver hack's
// `--configure` flag — see qtui/src/SettingsDialog.h.
#include "SettingsDialog.h"
#include "util/Logging.h"

#include <QApplication>

int main(int argc, char** argv)
{
    ssv::installFileLogging();

    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("SteamTrailerScreensaver"));
    QApplication::setApplicationName(QStringLiteral("SteamTrailerScreensaverSettings"));

    ssv::SettingsDialog dialog;
    dialog.show();
    return app.exec();
}
