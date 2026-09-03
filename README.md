# Steam Trailer Screensaver

A game-trailer screensaver in the spirit of the old Steam Big Picture
trailer screensaver — plays streamed game trailers full-screen, filtered
by genre and age rating, as a real OS-managed screensaver on Windows and
Linux.

## Features

- **Real OS screensaver integration**, not a standalone app you have to
  launch yourself: a genuine Windows `.scr` (works with Display Settings'
  screensaver picker, including the small preview thumbnail) and a classic
  X11 xscreensaver hack on Linux.
- **Steam as the primary catalog**, with optional **IGDB** and **GOG**
  sources that add games regardless of storefront. Trailers are discovered
  and filtered using each source's own genre/age metadata, normalized onto
  one shared vocabulary. If a Steam game has no Steam-hosted trailer, it
  falls back to a YouTube search for that specific game; IGDB games instead
  carry a curated YouTube video id directly, so no search is needed for
  them at all; GOG games carry their own trailer field too (YouTube-hosted
  ones play directly, others fall back the same way Steam's do). Both extra
  sources are off by default — IGDB needs a free Twitch developer app, GOG
  needs no setup at all (see Settings below).
- **Genre and age filtering**, allow-list or block-list, plus a "prefer
  popular / most-played games" option (backed by SteamSpy's top-played
  lists).
- **Quality-checked YouTube fallback**: the search query is disambiguated
  with the game's developer name (so a same-named movie or unrelated video
  doesn't win), several results are checked against the game's actual
  title, and anything longer than a configurable cap (10 minutes by
  default) is rejected — so a stray Let's Play or full walkthrough doesn't
  end up standing in for a trailer.
- **Configurable resolution cap and multi-monitor behavior** (a different
  trailer per monitor, or the primary monitor only).
- **No idle daemon.** Nothing runs in the background when the screensaver
  isn't active — see [Idle-footprint invariant](docs/ARCHITECTURE.md#idle-footprint-invariant).
- **Built to add more sources later.** The metadata/catalog layer is a
  pluggable interface; Steam is the first implementation, not a hardcoded
  assumption — see [Extensibility](docs/ARCHITECTURE.md#extensibility-adding-a-metadata-source).

## How it works, briefly

Discovery and filtering are driven entirely by Steam's own storefront data
(genres, age rating, content descriptors) plus SteamSpy for bulk
genre/tag/popularity lookups. Trailer video comes from Steam's CDN when a
game has one; otherwise `yt-dlp` searches YouTube for it, and mpv (via its
render API, embedded directly in the app's window rather than opening its
own) plays the result. Everything discovered is cached in a local SQLite
database, so breadth builds up opportunistically across many short-lived
screensaver activations rather than needing a background service.

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the source layout
and [`docs/LIMITATIONS.md`](docs/LIMITATIONS.md) for known constraints
(GNOME/Wayland isn't supported, Steam's endpoints are unofficial, etc.).

## Requirements

- [`yt-dlp`](https://github.com/yt-dlp/yt-dlp) installed and either on
  `PATH` or pointed at via `advanced.ytDlpPath` in the config — required
  for the YouTube-fallback path (Steam's, and IGDB's rare no-video case) to
  work at all.
- Windows 10/11, or Linux with a running X11 session and `xscreensaver`
  installed (X11 via XWayland works; native Wayland/GNOME sessions have no
  screensaver-hosting mechanism to integrate with at all).
- Optional, to enable the IGDB source: a free Twitch developer app —
  create one at [dev.twitch.tv/console/apps](https://dev.twitch.tv/console/apps)
  and enter its Client ID and Secret in Settings.

## Building from source

Requires CMake 3.21+, a C++20 compiler, Qt6 (Core, Network, Sql, Widgets,
OpenGLWidgets), and libmpv.

**Windows** — dependencies are managed via vcpkg (`vcpkg.json`); libmpv has
no reliable vcpkg port, so vendor a prebuilt shared library under
`third_party/mpv/win64/{include,lib,bin}` (a community "mpv-dev" build —
see `cmake/FindLibmpv.cmake` for the exact expected layout). Release builds
of the `.scr` should use a static-Qt vcpkg triplet
(`-DSSV_STATIC_QT=ON`) — see that target's `CMakeLists.txt` for why.

**Linux** — `apt install qt6-base-dev libmpv-dev` (or your distro's
equivalent) covers the library side; `apps/xscreensaver-hack` additionally
needs `xscreensaver` installed to actually test against.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

CMake options: `BUILD_WIN_SCR`, `BUILD_XSCREENSAVER_HACK`,
`BUILD_SETTINGS_GUI`, `BUILD_TESTS` (all default on for the relevant
platform), `SSV_STATIC_QT` (default off).

## Installing

- **Windows**: build, then run `apps/win-scr/installer/installer.iss`
  through Inno Setup. It installs to Program Files and additionally copies
  the `.scr` and `libmpv-2.dll` into `System32`, since that's the only
  place Windows' screensaver picker actually scans.
- **Linux**: `apps/xscreensaver-hack/install/install.sh` for a per-user
  install, or build the `.deb` under
  `apps/xscreensaver-hack/packaging/debian/` for a system-wide one — both
  register the hack with xscreensaver automatically.

## Settings

One settings dialog is shared across all three entry points (Windows `/c`,
the Linux hack's `--configure`, and the standalone `settings-gui`):
resolution cap, multi-monitor mode, mute/hardware-decode toggles, genre
allow/block-list, maximum age rating, "prefer popular games", a debug log
overlay toggle (off by default — shows a live tail of the app's log on top
of the video, for troubleshooting), the IGDB source toggle with its Twitch
Client ID/Secret fields, and a GOG source toggle (no fields needed).

A few settings are config-file-only for now (no UI control yet): trailer
length cap (`advanced.maxTrailerDurationSeconds`, default 600s), cache TTLs,
and the per-run network request budget. The config file lives at
`%LOCALAPPDATA%\SteamTrailerScreensaver\config.json` on Windows or
`$XDG_CONFIG_HOME/steam-trailer-screensaver/config.json` on Linux.

## License

No license has been chosen for this project yet.
