# CapyBrowser release readiness

Snapshot: 2026-07-11. Target: `0.6.7`, pinned to CapyOS
`0.10.0-alpha.1+20260903`.

## Completion

| Scope | Completion | Meaning |
|---|---:|---|
| Local `0.6.7` release implementation | 100% | Source, tests, HTTPS-linked build, both packages and deterministic offline gate pass. |
| Publish/install readiness | 80% | Code is ready; GitHub Release assets, aggregate modules index and VM install smoke remain external. |
| Static graphical browser core | 82% | Production page pipeline exists; runtime resource/form/download wiring and broader HTML/CSS remain. |
| Modern dynamic-web browser | 30% | Static safe browsing works; JavaScript, dynamic DOM/events/fetch and full layout are later stages. |

These percentages intentionally measure different outcomes. Passing the local
release gate does not claim parity with Chromium/Firefox-class dynamic sites.

## Implemented and gated

- One production API composes HTML parse, CSS parse/cascade, layout and
  display-list generation.
- Metadata/raw-text/inert elements are excluded from layout; scripts are never
  executed and produce a visible `SCRIPT_BLOCKED` policy warning.
- The libcurl host rejects non-2xx final status, aborts fixed-buffer overflow,
  and uses the effective post-redirect URL for relative links, history and
  Referer.
- `make release-check` runs the complete deterministic suite, links the HTTPS
  host, builds text/core packages twice and verifies canonical manifests,
  hashes, paths and byte-for-byte reproducibility.
- CI and CodeQL compile the real browser surfaces, including the libcurl host.

## P0 before publishing/installing `0.6.7`

1. Commit the reviewed tree and create the immutable `v0.6.7` tag from that
   clean commit. Do not move/reuse `v0.6.6`: it already points to the pre-fix
   source revision.
2. Create the GitHub Release `v0.6.7` and upload the exact four artifacts
   produced under `build/release-check/pass1/capypkg/`.
3. Generate and publish one aggregate modules index containing both current
   CapyBrowser descriptors plus compatible current dependencies. The current
   CapyOS default index still points to the legacy browser-core `0.0.5`.
4. Run `make release-check-remote MODULES_INDEX_URL=<versioned-https-url>`; it
   must verify tag, Release, bytes, hashes, sizes and both index entries.
5. Only after step 4, update the CapyOS `modules_index` pin/URL and run its
   version/release gates.
6. Run repeated full-profile installs in the supported VM with real egress,
   including clean install, retry after injected failure and reboot activation.

## P1 static-browser product work

- Wire `capy_page_render()` into the CapyOS desktop page lifecycle.
- Add the resource loader for linked CSS/images with origin, budget and cache
  policy; re-run layout when a supported resource becomes available.
- Connect form controls and GET/POST submission to the host adapter.
- Replace buffered downloads with quota-aware, atomic streaming and cleanup.
- Enforce private-session cookies/cache/storage and transport behavior.
- Expand HTML tree recovery, CSS selectors/box model/inline flow, UTF-8 error
  handling and IDNA.
- Add sanitizer/fuzzer jobs and a deterministic corpus of representative sites.

## P2 dynamic modern web

JavaScript execution, mutable DOM, events, timers, Fetch/XHR, reflow and a
sandboxed process model remain the long-term Stage 12+ track. They must not be
silently enabled inside the `0.6.x` static-engine contract.

## Release commands

```sh
make release-check
make release-check-remote \
  MODULES_INDEX_URL=https://example.invalid/versioned/modules-index.txt
```

The second command is deliberately remote and fail-closed. It is expected to
fail until the Release and aggregate index have actually been published.
