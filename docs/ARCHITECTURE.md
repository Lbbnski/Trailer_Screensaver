# Architecture

Steam Trailer Screensaver plays streamed game trailers as a real,
OS-managed screensaver on Windows (a `.scr` binary) and Linux (an
xscreensaver hack), filtered by genre and age rating. See the repo root
`README`-equivalent context: this document assumes you've read the project
plan; it exists to orient a new contributor in the actual source tree.

## Layout

- `core/` (`ssvcore`, static lib, no GUI dependency) — config, the
  pluggable metadata-source abstraction (`sources/IMetadataSource.h`),
  the Steam (`sources/steam/`), IGDB (`sources/igdb/`), and GOG
  (`sources/gog/`) source implementations, the SQLite cache, the
  source-agnostic `TrailerResolver`, `PlaylistEngine` filtering, and the
  libmpv playback wrapper (`playback/MpvPlayer` + `MpvGlRenderer`).
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

The interface (`core/src/sources/IMetadataSource.h`) is source-agnostic by
design, and IGDB (`sources/igdb/`) is a second real implementation
alongside Steam (`sources/steam/`) proving it out — adding a source means:

1. Implement `IMetadataSource` (see `sources/steam/SteamTrailerSource.h`
   or `sources/igdb/IgdbTrailerSource.h`).
2. Add a genre-mapping table translating the new source's own vocabulary
   onto `GenreTaxonomy`'s canonical list (`sources/steam/SteamGenreMap.*`,
   `sources/igdb/IgdbGenreMap.*`) — extend `GenreTaxonomy::canonicalGenres()`
   itself only if the new source needs a genre neither existing source has
   any equivalent for.
3. Register an instance in `PlaybackSession::start()` and add its id to
   `Config::sources.enabled`. If the source needs its own credentials
   (IGDB's Twitch Client ID/Secret) or config, add fields to
   `SourcesConfig` and register conditionally — see how IGDB is skipped
   with a log line, not a hard failure, when unconfigured.

Nothing in the cache, `TrailerResolver`, `PlaylistEngine`, or either OS
integration layer needs to change — every cache table is already keyed (or
scoped) by `source_id` for exactly this reason. IGDB's addition needed zero
changes to any of those files, only new files under `sources/igdb/` plus
the `PlaybackSession`/`Config`/`SettingsDialog` wiring described above.

IGDB also illustrates a source that never needs `resolveFallback()` for
most candidates: every `Game.videos[]` entry it returns is already a
curated YouTube video id (`GameVideo.video_id`), so `IgdbTrailerSource`
only calls into the shared `YoutubeFallbackResolver` (owned by
`PlaybackSession`, passed by reference to every source that needs it) for
the rare candidate with no videos of its own — unlike Steam, which needs
it for every app with no Steam-hosted trailer.

GOG (`sources/gog/`) illustrates a source whose API shape doesn't map
cleanly onto the "cheap id list from discoverCandidates(), full details
from one fetchDetails() call" model the interface implies: GOG's listing
endpoint gives genres/developer/age for free during discovery, but
trailers only come from a *separate* per-id call that repeats none of that
metadata. `GogCandidateFinder` bridges this with an in-memory
(not persisted) map from native id to the metadata discovery already
found, which `GogTrailerSource::fetchDetails()` reads before making its
own one video-only request — safe specifically because
`TrailerResolver::preparePool()` always calls `fetchDetails()` for a
newly-discovered id within the same call that discovered it. A future
source with an even more different shape doesn't have to reuse this exact
trick, but it's a working example of adapting an odd API to the interface
without changing the interface itself.

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
