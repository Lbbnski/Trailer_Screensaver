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
  a minor accuracy note rather than a real gap. (Games with no Steam page
  at all are still excluded from the *Steam* source specifically — but see
  IGDB below, which covers exactly that gap.)

- **IGDB's free tier is rate-limited, though exact numbers aren't
  published**, and its schema has already migrated the age-rating
  representation from an inline numeric enum to a reference-table string
  once (`AgeRating.rating` → `AgeRating.rating_category.rating`). Both
  `IgdbAgeRatingMap` and `IgdbGenreMap` treat any label they don't
  recognize as "no known value" rather than crashing or wrongly excluding
  a game, so a future IGDB schema change degrades filtering accuracy for
  the affected label rather than breaking the source outright — a
  `logWarning` line names the specific unrecognized label so the mapping
  table can be updated once observed. IGDB also requires a free Twitch
  developer app (Client ID + Secret, configured in Settings); until those
  are set, `sources.enabled` can list `"igdb"` with no effect other than an
  informational log line — Steam alone still works exactly as before.

- **GOG's API is unofficial and undocumented** (no published SLA, same
  category of risk as Steam's own storefront endpoints) — `embed.gog.com`
  and `api.gog.com` are community-reverse-engineered, not published by
  GOG/CDPR. No credentials are needed and no confirmed rate limit was
  found, but the exact accepted values for the listing endpoint's `sort`
  and `category` parameters were inferred by analogy with GOG's own
  storefront UI rather than confirmed against official documentation — an
  unrecognized value is expected to degrade to GOG's default ordering
  rather than error, so worst case is a less-ideal discovery order, not a
  failure.
- **GOG trailers hosted on Wistia (rather than YouTube) are not resolved.**
  GOG's product-video field reports a `provider` per video; only
  `provider == "youtube"` entries are played directly. Wistia's own
  asset-resolution endpoint was found to be unreliable even for real
  trailers during this integration's design research, so Wistia-hosted
  trailers instead fall through to the same YouTube-search fallback every
  source shares — meaning some GOG games will look, from a filtering
  standpoint, like they had no trailer at all until that fallback resolves
  one.
