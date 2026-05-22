# CapyBrowser compatibility and integration contract

CapyBrowser owns the **portable browser-core logic** (URL parsing,
HTML-to-text, static HTML/CSS parse, future display-list). CapyBrowser
modules must remain portable browser-core logic and must not depend
on CapyOS runtime internals.

## CapyOS reference version

- CapyOS core pinned for this contract: `0.8.0-alpha.244+20260520`
- Authoritative cross-repo matrix: [`CapyOS/docs/reference/integration/compatibility-matrix.md`](../../CapyOS/docs/reference/integration/compatibility-matrix.md)
- Canonical manifest format consumed by the in-tree adapter: [`CapyOS/docs/reference/integration/capypkg-publisher-manifest-format.md`](../../CapyOS/docs/reference/integration/capypkg-publisher-manifest-format.md)
- Manual deploy runbook: [`CapyOS/docs/operations/manual-module-deploy-runbook.md`](../../CapyOS/docs/operations/manual-module-deploy-runbook.md)
- Current cross-repo audit: [`CapyOS/docs/reference/integration/compatibility-audit-2026-05-20.md`](../../CapyOS/docs/reference/integration/compatibility-audit-2026-05-20.md)

## Authoritative CapyOS references

- `CapyOS/docs/reference/integration/modular-installation-architecture.md`
- `CapyOS/docs/reference/integration/browser-core-integration-contract.md`
- `CapyOS/docs/reference/integration/media-codec-integration-contract.md`
- `CapyOS/docs/reference/integration/external-core-repositories.md`

## Owned ABI

CapyBrowser owns the `capy-browser-core` ABI (v1 planned; not yet
runtime-active — runtime integration gated by Etapas 6-7).

This ABI covers:

- URL parsing and normalization;
- HTML-to-text output (CapyBrowse Text for Etapa 6);
- static HTML/CSS parse contracts (Etapa 7);
- future display-list format (versioned, additive, deterministic);
- deterministic parse/layout errors;
- internal limits on memory, time and input size.

CapyBrowser does **not** own:

- codec ABIs (belongs to `CapyCodecs` and `capy-codec-image`;
  CapyBrowser must declare the codec dependency in its
  `depends=` manifest line);
- DNS/TCP/TLS network access (CapyOS `net/`);
- filesystem (CapyOS `fs/`);
- compositor / window manager / input plumbing (CapyOS core +
  CapyUI desktop session);
- cache, cookies, sandbox policy (CapyOS adapters);
- user-facing app lifecycle (CapyOS / CapyUI).

## Compatibility rules

- Browser outputs must be deterministic for the same input and limits.
- Display-list changes must be versioned and additive until the
  integration stage allows migration.
- Browser logic must not call DNS/TCP/TLS, filesystem, compositor or
  input directly. It must consume host adapter callbacks injected by
  the integration adapter.
- Network, cache, cookies, sandbox and user-facing lifecycle belong
  to CapyOS adapters.
- Codec use must go through `capy-codec-image`, not browser-local
  snapshots.
- HTML parser must be tolerant (recovers from malformed input) but
  deterministic (same input → same output).
- The text mode of CapyBrowse must remain available as a fallback
  even after the graphical browser lands (Etapa 7 keeps text mode
  for diagnostics).

## Error model

| Code family | Trigger | Adapter behaviour |
|---|---|---|
| URL parse invalid | `capy_browser_url_parse` returns negative | UI displays "URL invalid" |
| HTML parse error | parser emits warning event; never aborts process | UI shows partial render + warning |
| HTTPS handshake failure | host adapter returns error code | UI displays clear TLS error; no auto-degrade to HTTP |
| Resource exceeds memory limit | parser/layout returns out-of-budget | UI displays "page too large" |
| Resource exceeds time limit | parser/layout returns out-of-budget | UI displays "page too slow" |
| Image decode failure | codec adapter returns negative | UI shows placeholder; page still renders |
| JavaScript present | parser blocks execution (Etapas 6-7 do not execute JS) | UI shows warning; page renders without JS |
| Dangerous redirect (cross-scheme, non-HTTPS) | host adapter rejects | UI displays redirect blocked |

All errors must be deterministic. CapyBrowser never crashes the
desktop, never executes JavaScript before Etapa 12, and never
auto-follows non-HTTPS redirects.

## Resource and performance limits

| Limit | Value | Owner |
|---|---|---|
| Maximum HTML input size | configurable per integration stage (alpha target: 256 KiB) | CapyBrowser |
| Maximum URL length | bounded by CapyOS `HTTP_MAX_URL = 2048` | CapyOS |
| Maximum parse time per page | configurable per integration stage (alpha target: 2 s) | CapyBrowser |
| Maximum layout depth | bounded by widget core constraints | CapyBrowser |
| Image decode budget | bounded by `capy-codec-image` limits | CapyCodecs |
| Capy package payload | ≤ 1 MiB during alpha streaming-buffer window | CapyOS adapter |

## Install/update boundary

CapyBrowser may be an optional Capy package when the integration
stage opens (Etapas 6-7). CapyOS owns:

- network transport and TLS policy;
- cache/cookie storage;
- window/input/render backend;
- sandbox and permissions;
- staging, activation and rollback;
- user-facing app lifecycle and launcher integration via CapyUI.

## Dependency rules

A browser component may declare dependencies on:

- `capy-browser-core` ABI;
- `capy-codec-image` when image rendering is enabled (typical canonical
  package name: `org.capyos.codecs.image-basic`);
- CapyOS UI/render ABIs only through documented adapters.

It must not depend on codec source files or GUI internals directly.

## Validation before CapyOS integration

Before CapyOS consumes a CapyBrowser release, externally validate:

- URL parse fixtures (host-side, golden);
- HTML-to-text golden fixtures;
- static display-list fixtures when available;
- malformed/truncated input rejection;
- dependency declaration for codecs (when image rendering is enabled);
- no direct CapyOS kernel/runtime includes;
- `make validate` and `make package` produce canonical assets when the
  runtime opens.

CapyBrowser integration is gated by Etapas 6-7. Text-mode core
lands first (Etapa 6) and is preserved as a fallback when graphical
browsing arrives (Etapa 7).

## Publishing as a Capy package (future, when Etapas 6-7 open)

When CapyBrowser is delivered as a remote module to the CapyOS
`services/capypkg` adapter, the publisher must follow
[`CapyOS/docs/reference/integration/capypkg-publisher-manifest-format.md`](../../CapyOS/docs/reference/integration/capypkg-publisher-manifest-format.md).
The key requirements that affect CapyBrowser are:

- `payload_url` must be HTTPS only;
- `payload_sha256` must be lowercase 64 hex of the published artifact;
- `payload_size` ≤ 1 MiB during the alpha streaming-buffer window;
- `name` must match the alphabet `[a-zA-Z0-9._-]`; suggested canonical
  names: `org.capyos.browser.text` (Etapa 6) and
  `org.capyos.browser.core` (Etapa 7);
- `install_root` must live under `/var/capypkg` or `/opt/`;
- the Ed25519 signature must cover the canonical descriptor
  `name=N|version=V|payload_sha256=H|payload_url=U\n`;
- `depends` must declare `org.capyos.codecs.image-basic` (or the
  active `capy-codec-image` package name) when image rendering is
  enabled.

Until CapyAgent publishes its Ed25519 signer, CapyBrowser cannot be
installed from a `signed` repository in production; lab tests with
`--unsigned` repositories are possible but must never be promoted.
