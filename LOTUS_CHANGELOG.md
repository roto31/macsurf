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
- View menu: Use Proxy When Available, Use Proxy for Secure Sites, Set Proxy Server…, and a
  status line (none / checking / WebOne or Squid address / not answering).
- `PERFACC` log line gains `connect=` (time the http fetcher spent blocked in Open Transport
  connect; not part of `total=`) and `via=direct|proxy|mixed`.

### Changed

- `http_proxy` now defaults to on. With no proxy address configured it does nothing.

### Fixed

- `misc.c`'s Internet Config calls compiled without prototypes: `macos9.h` suppresses
  `InternetConfig.h` for Carbon.h, so the include was a no-op. Declarations now come from
  [`macos9_ic.h`](browser/netsurf/frontends/macos9/macos9_ic.h).
- The http fetcher's request-size guard ignored the caller headers and Sec-Fetch/Origin headers
  (their lengths were never assigned), so a large cookie plus large XHR headers could overrun the
  8 KB request buffer.
- An https URL that reached the http fetcher by mistake was treated as an open connection instead
  of failing.

### Security

- The plain-HTTP "Send Debug Log" upload is compiled out. The menu item stays disabled unless a
  build defines an HTTPS `LOTUS_DEBUG_LOG_UPLOAD_URL`.
- Removed distrusted or expired roots: Atos TrustedRoot 2011, Entrust Root CA, SecureSign Root CA12,
  ePKI Root.

### Data

- CA anchors dataRevision 2: 121 anchors (82 RSA, 39 EC) from the Mozilla root store as of 2026-09-25.
  Added SECOM TLS RSA Root CA 2024, SECOM TLS ECC Root CA 2024, Telia RSA TLS Root CA v3 and Telia
  EC TLS Root CA v3. See [`ostls_b3_anchors.c`](macTLS/os9/ostls_b3_anchors.c).

### Known issues

- `macos9_woff2.c` is described as a port of google/woff2 (Apache-2.0); its compatibility with
  GPL-2.0-only is unresolved, so binary releases are on hold.
- macTLS states MIT in its README but ships no LICENSE file.
