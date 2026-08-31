# Known Limitations

- **Steam's `appdetails` and `search/results` endpoints are unofficial and
  undocumented.** There's no SLA, and Valve could change the response
  format or rate-limit/soft-ban aggressive callers with no appeal path.
  Mitigated by conservative per-run request budgeting
  (`advanced.maxCatalogRequestsPerRun`), a distinct User-Agent, and graceful
  degradation to cache-only playlist building on repeated failures — never
  a hard crash.

- **GNOME/Wayland has no OS-native screensaver-hosting mechanism.** Modern
  GNOME dropped the classic screensaver-plugin model entirely — it just
  blanks/locks the session and can't host a third-party screensaver like
  this one. This project only targets Windows and X11 (via the classic
  xscreensaver framework); an X11 session running under XWayland still
  works since that's genuinely still X11 hosting, but there is no supported
  install path for a native-Wayland/GNOME session.

- **yt-dlp is inherently fragile against YouTube's frequent extractor
  changes.** A search or stream-resolution failure degrades to "skip this
  app's fallback, try the next playlist candidate" rather than blocking
  playback. `advanced.ytDlpPath` is configurable so a self-updated system
  install of yt-dlp can be used independent of whatever ships with the app.

- **Steam's age-gate cookie approach may not be bulletproof for
  Adults-Only-Sexual-Content titles specifically** — some titles impose
  additional server-side checks beyond the standard age-gate cookies. Any
  unexpected missing-`movies`/403 response on a mature app is treated as
  "skip this app," never a hard error.

- **Windows `.scr`-in-`System32` DLL placement** is the messiest part of
  the Windows packaging story: Windows' screensaver picker only scans
  top-level `*.scr` files in `System32`, so every runtime dependency of the
  installed `.scr` must be resolvable from there too. Release builds use a
  statically-linked Qt specifically to keep this to one companion file
  (`libmpv-2.dll`) rather than a full dynamic Qt6 DLL set, but this remains
  a maintenance burden across future Qt version upgrades.

- **Independent per-monitor playback multiplies concurrent network/decode
  load with monitor count.** `playback.monitorMode` defaults to
  `independent` (a different trailer per monitor, matching the original
  Steam Big Picture screensaver's behavior) but can be set to `primaryOnly`
  for lower resource use on multi-monitor setups — the non-primary monitors
  are just a black surface in that mode rather than an idle/duplicated
  decode.

- **YouTube-fallback genre/age metadata is only as good as its Steam
  cross-reference.** The fallback path only ever triggers for a game that
  *does* have a Steam page (just no Steam-hosted trailer) — its genre/age
  filtering still comes from Steam's own metadata for that app, so this is
  a minor accuracy note rather than a real gap: games with no Steam page at
  all are excluded from the catalog entirely, by design.
