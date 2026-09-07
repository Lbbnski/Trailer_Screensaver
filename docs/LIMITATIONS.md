# Known Limitations

- **Windows TLS deployment needs OpenSSL's runtime DLLs shipped alongside
  the exe, or every HTTPS discovery request silently breaks.** Qt's default
  TLS backend selection tries its "openssl" plugin first and, if that
  plugin fails to *load* (as distinct from failing to negotiate a
  connection), falls straight through to the built-in "cert-only" backend
  — which cannot perform a real TLS handshake at all — rather than trying
  any other backend that IS loadable (e.g. "schannel", confirmed
  independently loadable in this environment but never even attempted once
  openssl's load failed). `qopensslbackend.dll` needs `libssl-3-x64.dll`
  and `libcrypto-3-x64.dll` on the loader's search path, but vcpkg's
  applocal deployment step only follows an executable's own *direct*
  link-time dependencies, not a plugin loaded dynamically at runtime via
  `QPluginLoader` — the same class of gap `sqlite3.dll` needed a manual
  `copy_if_different` for (see `CMakeLists.txt`'s `ssv_deploy_qt_plugins`).
  Observed directly: without this, GOG discovery failed on literally every
  run, and occasional Steam/SteamSpy calls failed intermittently too — with
  no error surfaced beyond an easy-to-miss log line, indistinguishable from
  a source just legitimately having nothing to offer.

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
  found. The listing endpoint's `category` facet turned out to be a small,
  genuinely fixed vocabulary — much like Steam's own official `genres` ids
  (see IgdbGenreMap's comment on the same problem) — and, critically, an
  *unrecognized* `category` value returns a hard HTTP 500, not a graceful
  fallback to default ordering as originally assumed. `GogGenreMap` only
  maps a canonical genre to GOG's `category` facet once verified live to
  return real results; several genres this app supports (Horror, Puzzle,
  Platformer, Fighting, Massively Multiplayer, Sci-Fi, Fantasy, Survival,
  Stealth, Sandbox, Visual Novel, at last check) have no confirmed working
  facet at all and are left unmapped — GOG just contributes nothing extra
  for those specific genre searches (still contributes normally for
  `preferPopular`'s unfiltered listing, and for every genre it does have a
  facet for) rather than failing.
- **GOG trailers hosted on Wistia (rather than YouTube) are not resolved.**
  GOG's product-video field reports a `provider` per video; only
  `provider == "youtube"` entries are played directly. Wistia's own
  asset-resolution endpoint was found to be unreliable even for real
  trailers during this integration's design research, so Wistia-hosted
  trailers instead fall through to the same YouTube-search fallback every
  source shares — meaning some GOG games will look, from a filtering
  standpoint, like they had no trailer at all until that fallback resolves
  one.

- **Epic Games Store was evaluated and deliberately not implemented.**
  Epic's storefront GraphQL endpoint (`store.epicgames.com/graphql`)
  returned `403 Forbidden` to plain HTTP requests from two independent
  clients (`curl`, PowerShell's `Invoke-WebRequest`) even with realistic
  browser headers, consistent with Cloudflare bot/TLS-fingerprint
  protection — the same category of block a Qt `QNetworkAccessManager`
  request would very likely also hit, since neither replicates a real
  browser's TLS handshake. Steam, IGDB, and GOG's endpoints were all
  reachable the same way; Epic was the outlier. Separately, even Epic's
  documented GraphQL schema doesn't clearly show which field connects a
  catalog offer to its trailer video (`Media.getMediaRef`'s `mediaRefId` is
  confirmed to exist but wasn't traceable back to a product query from
  documentation/library source alone). Both problems would need to be
  solved to add Epic as a real source; neither was pursued further since
  the first one alone means a straightforward implementation likely
  wouldn't function.
