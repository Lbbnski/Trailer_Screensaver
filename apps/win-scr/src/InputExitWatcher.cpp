#include "InputExitWatcher.h"
#include "util/Logging.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace ssv {

namespace {

// Ignore tiny/jittery reported movement (some mice/trackpads/RDP sessions
// report a pixel or two of noise on their own) so the screensaver doesn't
// end itself on nothing; any real drag or click still ends it immediately.
constexpr int kMouseMoveThresholdPx = 8;
POINT g_originPos{};
bool g_haveOrigin = false;

void requestQuit(const QString& reason)
{
    logInfo(QStringLiteral("InputExitWatcher: exiting (%1)").arg(reason));

    // Deliberately not qApp->quit(): that only posts a quit event onto the
    // main thread's event loop, and this app spends long stretches inside
    // *synchronous* blocking calls on that same thread (network requests
    // in TrailerResolver, the yt-dlp subprocess in
    // YoutubeFallbackResolver) that don't process posted events at all.
    // Observed directly: a genuine mouse-move dismiss got queued, but the
    // app kept running one or more further advance()/loadFile() cycles —
    // each gated on a multi-second network/subprocess call — before it
    // finally exited, which is exactly the "screensaver ignores input for
    // a long time, then vanishes" experience a screensaver must never
    // have. ExitProcess terminates every thread immediately regardless of
    // what any of them are doing, which is the actual contract a
    // screensaver dismiss needs; forgoing mpv's graceful shutdown here is
    // an accepted tradeoff for that.
    ExitProcess(0);
}

LRESULT CALLBACK keyboardHookProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION) {
        const auto* info = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
        requestQuit(QStringLiteral("keyboard, vkCode=%1 flags=%2").arg(info->vkCode).arg(info->flags));
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT CALLBACK mouseHookProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION) {
        if (wParam == WM_MOUSEMOVE) {
            const auto* info = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);
            if (!g_haveOrigin) {
                g_originPos = info->pt;
                g_haveOrigin = true;
            } else {
                const long dx = info->pt.x - g_originPos.x;
                const long dy = info->pt.y - g_originPos.y;
                if (dx * dx + dy * dy > kMouseMoveThresholdPx * kMouseMoveThresholdPx) {
                    requestQuit(QStringLiteral("mouse move, dx=%1 dy=%2 flags=%3")
                                    .arg(dx).arg(dy).arg(info->flags));
                }
            }
        } else {
            // Any click/wheel event ends it immediately, no threshold.
            requestQuit(QStringLiteral("mouse event wParam=%1").arg(wParam));
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

} // namespace

InputExitWatcher::InputExitWatcher()
{
    g_haveOrigin = false;
    m_keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, keyboardHookProc, nullptr, 0);
    m_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, mouseHookProc, nullptr, 0);
}

InputExitWatcher::~InputExitWatcher()
{
    if (m_keyboardHook)
        UnhookWindowsHookEx(static_cast<HHOOK>(m_keyboardHook));
    if (m_mouseHook)
        UnhookWindowsHookEx(static_cast<HHOOK>(m_mouseHook));
}

} // namespace ssv
