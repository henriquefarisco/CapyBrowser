# CapyBrowser documentation

CapyBrowser is the external browser-core repository for CapyOS.

## CapyOS reference version

Pinned for this release: `0.8.0-alpha.309+20260702`. `make version-check`
enforces this value against `docs/compatibility.md`, `docs/roadmap.md` and the
sibling `CapyOS/VERSION.yaml` when that repository is available.

## Migrated content

- `docs/capyos-migration.md`
- `docs/compatibility.md`

## Planning

- `docs/roadmap.md` — short/medium/long-term roadmap (subordinate to the contract docs above; non-authoritative for ABI/limits until reflected in `docs/compatibility.md` and the CapyOS matrix).

## Source ownership

CapyBrowser owns portable browser logic. CapyCodecs owns portable codec logic. CapyOS owns networking, TLS policy, rendering backend, windows, input, cache/cookie storage and sandbox integration.

## Current status

The historical CapyOS browser application source is not present in active
`src/apps`. CapyBrowser 0.6.7 provides URL and HTML-to-text cores, a static
HTML/CSS page pipeline, layout/display-list output and the host-testable
download/session/forms decision surfaces. CapyOS consumes the text and
graphical cores in Etapas 6-7; codecs remain canonically owned by CapyCodecs.

## Remaining integration work

- Wire form controls/submission and streaming downloads through CapyOS host
  callbacks.
- Complete private-session storage/transport enforcement in CapyOS.
- Promote signed package/index publication and keep release assets plus the
  production modules index synchronized through `make release-check-remote`.
- Expand the static HTML/CSS subset before the separately gated JavaScript and
  dynamic-DOM stages.

## Integration contracts

- [`release-readiness.md`](release-readiness.md) — current completion,
  publication blockers and the ordered P0/P1/P2 browser roadmap.

- `CapyOS/docs/reference/integration/browser-core-integration-contract.md`
- `CapyOS/docs/reference/integration/media-codec-integration-contract.md`
- `CapyOS/docs/reference/integration/modular-installation-architecture.md`
- `CapyOS/docs/reference/integration/compatibility-matrix.md`
- `CapyOS/docs/reference/integration/capypkg-publisher-manifest-format.md`
- `CapyOS/docs/operations/manual-module-deploy-runbook.md`
