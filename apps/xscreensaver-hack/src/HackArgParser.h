#pragma once

#include <QProcessEnvironment>
#include <QStringList>
#include <QtGui/qwindowdefs.h> // WId

#include <optional>

namespace ssv {

enum class HackMode {
    RunEmbedded, // -window-id <xid> (or $XSCREENSAVER_WINDOW)
    Configure,   // --configure — opens the shared SettingsDialog
};

struct HackArgs {
    HackMode mode = HackMode::Configure;
    WId windowId = 0;

    // Transient overrides sourced from the xscreensaver capplet's own
    // flags (see config/steam-trailer-saver.xml's <command>/arg-set
    // bindings) — xscreensaver's driver persists these itself (in
    // ~/.xscreensaver) and passes them on every invocation, which is a
    // separate mechanism from this project's own JSON config written by
    // SettingsDialog. These are applied on top of the loaded Config for
    // this run only, never written back to disk, so the two persistence
    // mechanisms can't fight over what's "correct" — SettingsDialog's
    // saved config always remains the baseline.
    std::optional<bool> use480p;
    std::optional<int> maxAge;
    std::optional<bool> blacklistMode;
    std::optional<QStringList> genres;
};

// Parses the xscreensaver hack contract: `-window-id <xid>` on argv is the
// convention this project commits to ($XSCREENSAVER_WINDOW checked as a
// fallback for hosts that only set the env var), a project-specific
// `--configure` flag that opens the full SettingsDialog, and the
// resolution/age/blacklist-mode/genres flags the capplet XML can emit.
HackArgs parseHackArgs(const QStringList& args, const QProcessEnvironment& env);

} // namespace ssv
