#!/usr/bin/env bash
# Registers the built steam-trailer-saver hack with the current user's
# xscreensaver installation. Intended for a from-source/manual install;
# the packaged .deb (see ../packaging/debian) registers system-wide via
# postinst instead, without touching the user's own ~/.xscreensaver.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${1:-$SCRIPT_DIR/../../../build}"

BIN_SRC="$BUILD_DIR/apps/xscreensaver-hack/steam-trailer-saver"
XML_SRC="$SCRIPT_DIR/../config/steam-trailer-saver.xml"

if [[ ! -x "$BIN_SRC" ]]; then
    echo "error: built binary not found at $BIN_SRC (build the project first, or pass its build dir as \$1)" >&2
    exit 1
fi

BIN_DEST_DIR="$HOME/.local/lib/xscreensaver"
CONFIG_DEST_DIR="$HOME/.xscreensaver/config"
mkdir -p "$BIN_DEST_DIR" "$CONFIG_DEST_DIR"

install -m 755 "$BIN_SRC" "$BIN_DEST_DIR/steam-trailer-saver"
install -m 644 "$XML_SRC" "$CONFIG_DEST_DIR/steam-trailer-saver.xml"

XSCREENSAVER_RC="$HOME/.xscreensaver"
PROGRAM_LINE="  \"Steam Trailer Screensaver\" \"$BIN_DEST_DIR/steam-trailer-saver\" -root \\n"

if [[ -f "$XSCREENSAVER_RC" ]] && grep -q "steam-trailer-saver" "$XSCREENSAVER_RC"; then
    echo "steam-trailer-saver is already registered in $XSCREENSAVER_RC"
else
    if [[ -f "$XSCREENSAVER_RC" ]] && grep -q "^programs:" "$XSCREENSAVER_RC"; then
        # Insert right after the "programs:" line so xscreensaver-settings
        # picks it up alongside the stock hacks.
        awk -v line="$PROGRAM_LINE" '
            { print }
            /^programs:/ && !inserted { print line; inserted=1 }
        ' "$XSCREENSAVER_RC" > "$XSCREENSAVER_RC.tmp" && mv "$XSCREENSAVER_RC.tmp" "$XSCREENSAVER_RC"
    else
        echo "warning: could not find a 'programs:' block in $XSCREENSAVER_RC — add this line to it manually:" >&2
        echo "  $PROGRAM_LINE" >&2
    fi
fi

DESKTOP_DIR="$HOME/.local/share/applications"
mkdir -p "$DESKTOP_DIR"
cat > "$DESKTOP_DIR/steam-trailer-saver-settings.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Steam Trailer Screensaver Settings
Comment=Configure genre/age filters and resolution for the Steam Trailer Screensaver
Exec=$BIN_DEST_DIR/steam-trailer-saver --configure
Icon=preferences-desktop-screensaver
Categories=Settings;
EOF

echo "Installed. Open xscreensaver-settings (or 'Screensaver' in your settings app) to select it,"
echo "or run '$BIN_DEST_DIR/steam-trailer-saver --configure' directly for the full genre picker."
