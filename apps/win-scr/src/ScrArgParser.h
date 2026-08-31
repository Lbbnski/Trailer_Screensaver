#pragma once

#include <QStringList>
#include <QtGui/qwindowdefs.h> // WId

namespace ssv {

enum class ScrMode {
    RunFullscreen, // /s
    Configure,     // /c, or bare/unrecognized args (per the documented Windows contract)
    Preview,       // /p HWND
};

struct ScrArgs {
    ScrMode mode = ScrMode::Configure;
    WId targetWindow = 0; // parent HWND for /c:HWND, or the preview HWND for /p HWND; 0 if none given
};

// Parses the Windows screensaver command-line contract: `/s` (run
// fullscreen), `/c` or `/c:HWND` (show settings, optionally modal to a
// parent), `/p HWND` (embed a live preview into the given HWND), and
// bare/unrecognized input falling back to Configure — matching the
// documented behavior real screensaver hosts (Display Settings, the
// screensaver preview thumbnail) rely on.
//
// Takes a plain QStringList (not main()'s raw argv) so it's testable
// independent of how main.cpp obtains the command line; see main.cpp for
// why GetCommandLineW() is used instead of argv there.
ScrArgs parseScrArgs(const QStringList& args);

} // namespace ssv
