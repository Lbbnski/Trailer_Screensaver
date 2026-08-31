#pragma once

namespace ssv {

// Real screensavers must end the instant the user provides *any* input — a
// keypress or even a small mouse movement — regardless of which window (if
// any) currently has OS focus. FullscreenWindow deliberately never takes
// focus (Qt::WA_ShowWithoutActivating, Qt::Tool — a screensaver shouldn't
// steal focus from whatever was running underneath), so ordinary Qt
// key/mouse event handlers on the window itself would never fire; only
// Alt+F4 worked before this existed, since that's handled by Windows
// itself rather than routed through the (unfocused) window.
//
// This installs low-level global input hooks (WH_KEYBOARD_LL/WH_MOUSE_LL)
// that see input system-wide regardless of focus, and posts a queued
// QCoreApplication::quit() the moment real input is seen. Windows-only —
// on Linux, xscreensaver's own driver already watches for input and kills
// the hack process itself, so no equivalent is needed there.
class InputExitWatcher {
public:
    InputExitWatcher();
    ~InputExitWatcher();

    InputExitWatcher(const InputExitWatcher&) = delete;
    InputExitWatcher& operator=(const InputExitWatcher&) = delete;

private:
    void* m_keyboardHook = nullptr;
    void* m_mouseHook = nullptr;
};

} // namespace ssv
