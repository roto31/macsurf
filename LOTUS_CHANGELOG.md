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
