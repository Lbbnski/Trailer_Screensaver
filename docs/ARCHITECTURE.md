# Architecture

Steam Trailer Screensaver plays streamed game trailers as a real,
OS-managed screensaver on Windows (a `.scr` binary) and Linux (an
xscreensaver hack), filtered by genre and age rating. See the repo root
`README`-equivalent context: this document assumes you've read the project
plan; it exists to orient a new contributor in the actual source tree.

## Layout

- `core/` (`ssvcore`, static lib, no GUI dependency) — config, the
  pluggable metadata-source abstraction (`sources/IMetadataSource.h`),
  the Steam source implementation (`sources/steam/`), the SQLite cache,
  the source-agnostic `TrailerResolver`, `PlaylistEngine` filtering, and
  the libmpv playback wrapper (`playback/MpvPlayer` + `MpvGlRenderer`).
- `qtui/` (`ssvsettingsui`, static lib, Qt Widgets) — `MpvGLWidget` (the
  libmpv/Qt-GL integration point), `EmbeddedForeignWindow` (foreign-window
  embedding shared by Windows preview and the Linux hack),
  `SettingsDialog` (the one settings UI, used from all three entry
  points), and `PlaybackSession` (the playlist-driving pipeline shared by
  both OS integrations).
- `apps/win-scr/` — the Windows `.scr` executable: `ScrArgParser` (the
  `/s /c /c:HWND /p HWND` contract), `FullscreenController` +
  `FullscreenWindow` (one window per monitor, wrapping a shared
  `PlaybackSession`), `PreviewEmbedWindow` (`/p HWND`, cache-only —
  never triggers network activity), an Inno Setup installer.
- `apps/xscreensaver-hack/` — the Linux hack: `HackArgParser`
  (`-window-id`/`$XSCREENSAVER_WINDOW`, `--configure`, and the capplet's
  own resolution/age/blacklist-mode/genres flags), the capplet XML
  (`config/steam-trailer-saver.xml`), `install.sh`, and Debian packaging.
- `apps/settings-gui/` — a one-file standalone launcher for
  `SettingsDialog`.
- `tests/core/` — QtTest-based unit tests for `PlaylistEngine` filtering,
  `Config` JSON round-tripping, and `CacheRepository` staleness/CRUD.

## Extensibility: adding a metadata source

Steam is the only registered `IMetadataSource` today, but the interface
(`core/src/sources/IMetadataSource.h`) is source-agnostic by design.
Adding a second source means:

1. Implement `IMetadataSource` (see `sources/steam/SteamTrailerSource.h`
   for the reference implementation).
2. Extend `sources/GenreTaxonomy.cpp` if the new source's vocabulary needs
   more canonical genres than Steam's already cover.
3. Register an instance in `PlaybackSession::start()` (currently the only
   place that constructs `SteamTrailerSource`) and add its id to
   `Config::sources.enabled`.

Nothing in the cache, `TrailerResolver`, `PlaylistEngine`, or either OS
integration layer needs to change — every cache table is already keyed (or
scoped) by `source_id` for exactly this reason.

## Build

Requires CMake 3.21+, a C++20 compiler, Qt6 (Core/Network/Sql/Widgets/
OpenGLWidgets), and libmpv:

- **Windows**: install Qt6 (e.g. via the official installer or vcpkg —
  `vcpkg.json` covers `qtbase`), then vendor a prebuilt libmpv under
  `third_party/mpv/win64/{include,lib,bin}` (community "mpv-dev" shared
  builds; see `cmake/FindLibmpv.cmake` for exact expected layout). Release
  builds of `win-scr` should use a static-Qt vcpkg triplet
  (`x64-windows-static`, `-DSSV_STATIC_QT=ON`) — see that target's
  CMakeLists comment for why.
- **Linux**: `apt install qt6-base-dev libmpv-dev` (or your distro's
  equivalent), then a normal CMake configure/build. `apps/xscreensaver-hack`
  additionally needs `xscreensaver` installed to actually test against, and
  `yt-dlp` on PATH (or pointed at via `advanced.ytDlpPath` in config) for
  the YouTube fallback path to work.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

## Idle-footprint invariant

Neither OS integration point is ever a resident process: Windows'
`winlogon`/Display Settings spawns and kills the `.scr`, and xscreensaver's
own driver spawns and kills the hack. All catalog/metadata refresh work
(`TrailerResolver::preparePool`) happens only inside calls made *during* an
already-running screensaver session, bounded by
`advanced.maxCatalogRequestsPerRun`, and persisted to SQLite so state
survives between those short-lived process runs without any process
needing to stay alive between them.
