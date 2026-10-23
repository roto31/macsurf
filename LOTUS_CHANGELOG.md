# Lotus Navigator Carbon Edition — changelog

Downstream of [MacSurf](https://github.com/mplsllc/macsurf) v2.3 by mplsllc. Upstream history stays in
the MacSurf release notes; this file lists only the changes on the `downstream/main` branch.
Format: [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/); versions follow
[SemVer 2.0.0](https://semver.org/spec/v2.0.0.html). Each release also states the CA anchor
`dataRevision`; any anchor change requires at least a PATCH bump.

## Documentation

- [MacSurf README](README.md) — features, build notes, and credits inherited from upstream
- [macTLS README](macTLS/README.md) — TLS 1.2/1.3 stack
- [Branding constants](browser/netsurf/frontends/macos9/lotus_brand.h)

## [Unreleased]

Planned: **0.1.0** (CA anchors dataRevision 2). Not yet built with CodeWarrior 8.3 or validated on
hardware; no binary has been published.

### Added

- Lotus Navigator branding: app name, About box (MacSurf and NetSurf credits kept), creator code
  `LtsN`, Finder icons, and `vers`/`plst` resources at 0.1.0. See
  [`lotus_brand.h`](browser/netsurf/frontends/macos9/lotus_brand.h).
- Links the browser cannot handle open in the default helper app via Internet Config `ICLaunchURL`.
- TLS clock gate: certificate validation refuses system clocks earlier than 2026-01-01
  (`OSTLS_CLOCK_FLOOR_UNIX`) and accepts a trusted time source through
  `OSTLS_SetTrustedUnixTime`. See [`ostls_time.h`](macTLS/os9/ostls_time.h).
- LAN proxy support that never gets in the way. The browser probes a configured proxy (View >
  Set Proxy Server…, or the HTTP proxy in Internet Config) in the background with a 1.5-second
  non-blocking connect and a `HEAD /`. Plain-http pages go through it only while it answers; a
  failed proxy connect retries the request direct and re-checks the proxy after a minute. Only
  private IPv4 addresses (10/8, 172.16/12, 192.168/16, 169.254/16, 127/8) are accepted, and LAN,
  `localhost`, and `http_proxy_noproxy` destinations always go direct. See
  [`macos9_proxy.h`](browser/netsurf/frontends/macos9/macos9_proxy.h).
- Secure sites through a WebOne proxy, with consent. When the proxy identifies as WebOne, the
  browser asks once per proxy address before sending https GETs through it (the proxy does the
  TLS handshake instead of this Mac). Answer is stored in the `http_proxy_https_consent` option and
  can be changed under View > Use Proxy for Secure Sites. Requests with a body, Facebook hosts,
  and any request the proxy cannot take stay on macTLS end-to-end.
- Cookie guard for WebOne. WebOne 0.18.3 merges all `Set-Cookie` headers of a response into one
  unparseable line (seen on GitHub and httpbin; Squid and direct fetches are unaffected). When a
  response through WebOne carries a merged cookie line, nothing reaches the page: the request is
  re-issued without the proxy (direct for http, macTLS for https) and that host skips WebOne for
  the rest of the session. A single cookie (apple.com's `geo`) passes intact. Requests with a body
  never use WebOne, for http as well as https.
- The proxy consent dialog plays the system alert sound, since it opens on its own once the
  proxy answers.
- View menu: Use Proxy When Available, Use Proxy for Secure Sites, Set Proxy Server…, and a
  status line (none / checking / WebOne or Squid address / not answering).
- `PERFACC` log line gains `connect=` (time the http fetcher spent blocked in Open Transport
  connect; not part of `total=`) and `via=direct|proxy|mixed`.
- Built-in notifications (Lotus Notify N2). Banners float above browser windows in any of six
  positions (the four corners, top centre or centre), at most three stacked per position with the
  rest queued; they close after 3–15 seconds or on click, and a click opens the notice's link.
  Pop-up boxes stay until OK; progress notices show a Leopard-blue bar. Questions are movable
  modal dialogs in the standard alert position (centred, upper third of the screen) with up to
  three buttons, an icon, wrapped long text and an optional checkbox (Return picks the default
  button, Esc or ⌘-. the cancel button); they always appear, even with notifications off. See [`macos9_notify.h`](browser/netsurf/frontends/macos9/macos9_notify.h).
- Preferences: a Notifications tab (position, banner duration, style per notice type, sound, the
  downloads/proxy/security/housekeeping switches) with a Show Examples button. Settings are
  stored as `notify_*` options in `desktop/options.h`.
- Notices for finished downloads (with a Show in Finder button) and failed downloads; for the
  proxy stopping or answering again; for cache cleared and bookmarks imported or exported; and,
  once per session, "Check this Mac's clock" with an Open Date & Time button when a secure site
  fails on dates and the clock reads a year earlier than the build.

### Changed

- The default home page is now `about:lotus`, a built-in Lotus Navigator start page (under 2 KB,
  no network needed, no outside links) with the Open Location, Reload and Home shortcuts and links
  to `about:cache` and `about:memory`. It replaces MacSurf's web home page. A home page set in
  Preferences is kept. The Show Examples information notice now reads "Click to open your start
  page" and opens `about:lotus`. See
  [`macsurf_config.h`](browser/netsurf/frontends/macos9/macsurf_config.h).
- Page `alert()` / `confirm()`, the delete confirmation, cache and bookmark messages and the About
  fallback use Lotus Notify instead of `StandardAlert`. Page dialogs are titled "*host* says:",
  show long text in full and offer "Stop dialogs from this page" from the second dialog (a stopped
  `confirm()` returns `false`). `StandardAlert` remains only in `macos9_notify_emergency_alert`
  (out of memory, About fallback).

- Keyboard, menu and window routing use `FrontNonFloatingWindow()` instead of `FrontWindow()`
  (in `main.c`, `macos9_font.c`, `macos9_download.c` and `macsurf_debug.c`) so a visible banner
  never takes commands meant for the browser window.
- Preferences tabs use the small tab style so six tabs fit the existing window width.

- `http_proxy` now defaults to on. With no proxy address configured it does nothing.
- New chrome colour scheme matched to Mac OS X 10.5 Leopard's Aqua Blue, replacing MacSurf's
  orange and gold: toolbar buttons, loading spinner, progress bar, URL-field focus ring, hover
  highlight, the History, Bookmarks, Downloads and Preferences title banners, list selection and
  stripes, and the default favicon (a blue globe instead of the puffin). One accent colour, used
  only for interactive and selected states; every text and control pairing meets its contrast
  minimum (4.5:1 text, 3:1 large text and controls). Colours live in
  [`lotus_theme.h`](browser/netsurf/frontends/macos9/lotus_theme.h); the image headers are generated.

### Removed

- The MacSurf orange/gold scheme and its artwork: orange toolbar icons, the puffin loading
  animation and favicon, and the gold banners, buttons and list colours.

### Fixed

- Pages taller than 32,767 pixels can be scrolled to the end. The scroll bars hold 16-bit values, so
  the scroll range was cut off at y=32767 (a 1,100-row table stopped at row 384); the full range is
  now mapped onto the scroll bar.
- Pages taller than 200,000 pixels are drawn. A defensive clamp in the redraw walk reset any box
  taller or lower than 200,000 px to 0, so a 4,500-row table (258,000 px) showed a blank page; the
  limit is now 1,000,000 px, the same as the layout limit.
- Clean CodeWarrior 8.3 builds (no cached object files): `file.c` and `resource.c` had a
  mis-escaped `SLEN(\"If-None-Match:\")` and included the renamed `utils/time.h` (now
  `utils/ns_time.h`); `about.c` and `about/config.c` include `content/fetchers/about/config.h`
  by full path so another library's `config.h` is not picked up; `image_cache.c` calls
  `(*centry->convert)(...)`, which MWCC 8.3 accepts.
- Link error `undefined: 'mkdir'` from `ns_file.c`: MSL for Carbon has no `mkdir`, and the POSIX
  `nsmkdir` macro in `utils/utils.h` won once the include search order was fixed. On Mac OS 9
  `nsmkdir` is now the existing no-op stub in `misc_stub.c`, as in the MacSurf v2.3 binary.
- `misc.c`'s Internet Config calls compiled without prototypes: `macos9.h` suppresses
  `InternetConfig.h` for Carbon.h, so the include was a no-op. Declarations now come from
  [`macos9_ic.h`](browser/netsurf/frontends/macos9/macos9_ic.h).
- The http fetcher's request-size guard ignored the caller headers and Sec-Fetch/Origin headers
  (their lengths were never assigned), so a large cookie plus large XHR headers could overrun the
  8 KB request buffer.
- An https URL that reached the http fetcher by mistake was treated as an open connection instead
  of failing.

### Security

- WOFF2 web fonts are decoded by a new clean-room decoder written from the W3C WOFF 2.0
  Recommendation (GPL-2.0-only), replacing the google/woff2-derived port whose Apache-2.0 origin
  conflicted with GPL-2.0-only. Same entry point (`macos9_woff2_to_ttf`); output is capped at 32 MB
  and font collections are refused. See
  [`macos9_woff2.h`](browser/netsurf/frontends/macos9/macos9_woff2.h).
- The HTTPS proxy consent question has a "Remember for this proxy" checkbox, ticked by default;
  unticked, consent lasts until the browser quits and nothing is saved.
- A question that cannot be shown answers "no" (`macos9_notify_ask` returns −1) instead of the
  default button.
- The plain-HTTP "Send Debug Log" upload is compiled out. The menu item stays disabled unless a
  build defines an HTTPS `LOTUS_DEBUG_LOG_UPLOAD_URL`.
- Removed distrusted or expired roots: Atos TrustedRoot 2011, Entrust Root CA, SecureSign Root CA12,
  ePKI Root.

### Data

- CA anchors dataRevision 2: 121 anchors (82 RSA, 39 EC) from the Mozilla root store as of 2026-09-25.
  Added SECOM TLS RSA Root CA 2024, SECOM TLS ECC Root CA 2024, Telia RSA TLS Root CA v3 and Telia
  EC TLS Root CA v3. See [`ostls_b3_anchors.c`](macTLS/os9/ostls_b3_anchors.c).

### Known issues

- macTLS states MIT in its README but ships no LICENSE file.
