# CapyBrowser documentation

CapyBrowser is the external browser-core repository for CapyOS.

## CapyOS reference version

Pinned for this release: `0.8.0-alpha.263+20260606`. Update this section together with `docs/compatibility.md` whenever the CapyOS core version, ABI or canonical manifest format changes.

## Migrated content

- `docs/capyos-migration.md`
- `docs/compatibility.md`

## Planning

- `docs/roadmap.md` — short/medium/long-term roadmap (subordinate to the contract docs above; non-authoritative for ABI/limits until reflected in `docs/compatibility.md` and the CapyOS matrix).

## Source ownership

CapyBrowser owns portable browser logic. CapyCodecs owns portable codec logic. CapyOS owns networking, TLS policy, rendering backend, windows, input, cache/cookie storage and sandbox integration.

## Current status

The historical CapyOS browser application source is not present in active `src/apps`. Browser core work should be rebuilt here as URL, HTML-to-text and static display-list logic. Codecs are now owned canonically by `CapyCodecs`.

## Pending extractions

- HTML-to-text core as new implementation.
- Static HTML/CSS display list core as new implementation.
- Adapter to consume `CapyCodecs` image decoders when CapyOS integration stages permit it.

## Integration contracts

- `CapyOS/docs/reference/integration/browser-core-integration-contract.md`
- `CapyOS/docs/reference/integration/media-codec-integration-contract.md`
- `CapyOS/docs/reference/integration/modular-installation-architecture.md`
- `CapyOS/docs/reference/integration/compatibility-matrix.md`
- `CapyOS/docs/reference/integration/capypkg-publisher-manifest-format.md`
- `CapyOS/docs/operations/manual-module-deploy-runbook.md`
