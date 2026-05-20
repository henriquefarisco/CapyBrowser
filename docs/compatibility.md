# CapyBrowser compatibility and integration contract

CapyBrowser modules must remain portable browser-core logic and must not depend on CapyOS runtime internals.

## CapyOS reference version

- CapyOS core pinned for this contract: `0.8.0-alpha.241+20260519`
- Authoritative cross-repo matrix: `CapyOS/docs/reference/integration/compatibility-matrix.md`
- Canonical manifest format consumed by the in-tree adapter: `CapyOS/docs/reference/integration/capypkg-publisher-manifest-format.md`
- Manual deploy runbook: `CapyOS/docs/operations/manual-module-deploy-runbook.md`

## Authoritative CapyOS references

- `CapyOS/docs/reference/integration/modular-installation-architecture.md`
- `CapyOS/docs/reference/integration/browser-core-integration-contract.md`
- `CapyOS/docs/reference/integration/media-codec-integration-contract.md`
- `CapyOS/docs/reference/integration/compatibility-matrix.md`
- `CapyOS/docs/reference/integration/capypkg-publisher-manifest-format.md`

## Owned ABI

CapyBrowser owns the `capy-browser-core` ABI.

This ABI covers:

- URL parsing and normalization;
- HTML-to-text output;
- static HTML/CSS parse contracts;
- future display-list format;
- deterministic parse/layout errors.

CapyBrowser does not own codec ABIs. Image decode compatibility belongs to `CapyCodecs` and `capy-codec-image`.

## Compatibility rules

- Browser outputs must be deterministic for the same input and limits.
- Display-list changes must be versioned and additive until the integration stage allows migration.
- Browser logic must not call DNS/TCP/TLS, filesystem, compositor or input directly.
- Network, cache, cookies, sandbox and user-facing lifecycle belong to CapyOS adapters.
- Codec use must go through `capy-codec-image`, not browser-local snapshots.

## Install/update boundary

CapyBrowser may be an optional component. CapyOS owns:

- network transport and TLS policy;
- cache/cookie storage;
- window/input/render backend;
- sandbox and permissions;
- staging, activation and rollback.

## Dependency rules

A browser component may declare dependencies on:

- `capy-browser-core` ABI;
- `capy-codec-image` when image rendering is enabled;
- CapyOS UI/render ABIs only through documented adapters.

It must not depend on codec source files or GUI internals directly.

## Validation before CapyOS integration

Before CapyOS consumes a CapyBrowser release, externally validate:

- URL parse fixtures;
- HTML-to-text golden fixtures;
- static display-list fixtures when available;
- malformed/truncated input rejection;
- dependency declaration for codecs;
- no direct CapyOS kernel/runtime includes.

CapyBrowser integration is gated by Etapas 6-7.

## Publishing as a Capy package (future, when the relevant stage opens)

When CapyBrowser is delivered as a remote module to the CapyOS
`services/capypkg` adapter, the publisher must follow
`CapyOS/docs/reference/integration/capypkg-publisher-manifest-format.md`.
The key requirements that affect CapyBrowser are:

- `payload_url` must be HTTPS only;
- `payload_sha256` must be lowercase 64 hex of the published artifact;
- `payload_size` ≤ 1 MiB during the alpha streaming-buffer window;
- `name` must match the alphabet `[a-zA-Z0-9._-]`;
- `install_root` must live under `/var/capypkg` or `/opt/`;
- the Ed25519 signature must cover the canonical descriptor
  `name=N|version=V|payload_sha256=H|payload_url=U\n`;
- `depends` must declare `org.capyos.codecs.image-basic` (or the
  active `capy-codec-image` package name) when image rendering is
  enabled.

Until CapyAgent publishes its Ed25519 signer, CapyBrowser cannot be
installed from a `signed` repository in production; lab tests with
`--unsigned` repositories are possible but must never be promoted.
