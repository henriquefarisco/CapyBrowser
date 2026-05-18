# CapyBrowser compatibility and integration contract

CapyBrowser modules must remain portable browser-core logic and must not depend on CapyOS runtime internals.

Authoritative CapyOS references:

- `CapyOS/docs/reference/integration/modular-installation-architecture.md`
- `CapyOS/docs/reference/integration/browser-core-integration-contract.md`
- `CapyOS/docs/reference/integration/media-codec-integration-contract.md`

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
