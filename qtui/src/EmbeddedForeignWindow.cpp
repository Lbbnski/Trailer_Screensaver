#include "EmbeddedForeignWindow.h"

#include <QWidget>

#if defined(SSV_PLATFORM_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(SSV_PLATFORM_LINUX)
#include <QWindow>
#endif

namespace ssv::EmbeddedForeignWindow {

#if defined(SSV_PLATFORM_WINDOWS)

bool embed(QWidget* widget, WId foreignWinId)
{
    if (!widget || foreignWinId == 0)
        return false;

    const HWND foreignHwnd = reinterpret_cast<HWND>(foreignWinId);
    if (!IsWindow(foreignHwnd))
        return false;

    // Qt only fully initializes a widget's native window (and starts
    // delivering paint/resize/GL-init events to it) once it has actually
    // been shown through Qt's own show() — do that before touching the raw
    // Win32 window relationship below, per QWindow::fromWinId's documented
    // caveat that a window embedded while hidden won't appear until shown.
    widget->show();

    const HWND ownHwnd = reinterpret_cast<HWND>(widget->winId());

    // Re-parent under the foreign window (the Display Settings preview
    // thumbnail) and switch from a top-level popup to a real child window
    // — this, not createWindowContainer()/QWindow::fromWinId() (which is
    // for embedding a *foreign* window into *our own* widget hierarchy,
    // the opposite direction), is what actually gets our rendering to show
    // up inside a native HWND owned by another process.
    SetParent(ownHwnd, foreignHwnd);
    LONG_PTR style = GetWindowLongPtr(ownHwnd, GWL_STYLE);
    style = (style & ~WS_POPUP) | WS_CHILD;
    SetWindowLongPtr(ownHwnd, GWL_STYLE, style);

    RECT clientRect{};
    GetClientRect(foreignHwnd, &clientRect);
    SetWindowPos(ownHwnd, nullptr, 0, 0, clientRect.right, clientRect.bottom,
                 SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    return true;
}

#elif defined(SSV_PLATFORM_LINUX)

bool embed(QWidget* widget, WId foreignWinId)
{
    if (!widget || foreignWinId == 0)
        return false;

    widget->show();

    QWindow* ownWindow = widget->windowHandle();
    if (!ownWindow)
        return false;

    // Qt's xcb platform plugin supports parenting one of our own QWindows
    // under a QWindow that merely wraps a foreign (non-Qt-owned) window id
    // — no raw Xlib reparenting needed here, unlike on Windows.
    // `foreignWindow` is intentionally never deleted: it must outlive the
    // embedded window, and this process is short-lived, spawned and killed
    // by xscreensaver's driver for the lifetime of one hack invocation.
    QWindow* foreignWindow = QWindow::fromWinId(foreignWinId);
    if (!foreignWindow)
        return false;

    ownWindow->setParent(foreignWindow);
    ownWindow->setGeometry(QRect(QPoint(0, 0), foreignWindow->size()));
    return true;
}

#endif

} // namespace ssv::EmbeddedForeignWindow
