#pragma once

#include <QtGlobal>
#include <QtGui/qwindowdefs.h>

class QWidget;

namespace ssv {

// Reparents one of our own widgets into a foreign, OS-owned window (a
// Windows HWND handed to us via the `/p HWND` screensaver-preview contract,
// or an X11 window id handed to us via xscreensaver's `-window-id`), so
// both the Windows preview pane and the Linux xscreensaver hack can reuse
// the same embedding code instead of two platform-specific implementations.
namespace EmbeddedForeignWindow {

// Makes `widget`'s own native window a child of the foreign window
// `foreignWinId` (an HWND cast to WId on Windows, an X11 XID cast to WId
// on Linux), sized to fill it, and shows it. Returns false if `widget` is
// null, `foreignWinId` is invalid, or embedding otherwise fails.
bool embed(QWidget* widget, WId foreignWinId);

} // namespace EmbeddedForeignWindow

} // namespace ssv
