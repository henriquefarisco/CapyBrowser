# CapyBrowser test fixtures

Golden fixtures for the `capy-browser-core` surfaces. Each fixture pairs an
input with its expected, deterministic output. A surface passes only when, for
identical `(input bytes, base URL, declared limits)`, it produces byte-for-byte
the same output **and** the same warning sequence.

## Convention

- `*.in`    — input bytes (HTML, URL list, etc.). A trailing newline, if any,
  is stripped by the runner before the bytes reach the surface.
- `*.base`  — optional base URL for resolution (one line).
- `*.out`   — expected output (normalized URL / text / display-list dump), OR a
  fail-closed verdict written as `ERR:<NAME>` (see below).
- `*.warn`  — expected warning sequence, one code name per line (may be empty
  or absent when no warning is expected).
- A case "name" is the shared filename stem, e.g. `relative-link.in` +
  `relative-link.out` + `relative-link.warn`.

### Fail-closed verdicts

A rejection case encodes the expected error in `*.out` as a single line
`ERR:<NAME>`, where `<NAME>` is the surface error code without its
`CAPY_<SURFACE>_ERR_` prefix (e.g. `ERR:CONTROL`, `ERR:HOST`, `ERR:BASE` for the
URL surface). The runner asserts the surface returns exactly that negative code
and writes nothing further.

Rules for every fixture:

- Deterministic: re-running the surface must reproduce `*.out` and `*.warn`
  exactly.
- Surface-specific rejection cases live next to their surface (e.g. `url/`),
  while `malformed/` holds cross-surface and truncated-payload cases. Either
  way, a rejection shows a deterministic verdict and never a process abort.
- No wall-clock, randomness or network: tests inject the deterministic clock
  and seeded PRNG from `../harness/capy_determinism.h`.

## Directories (one per surface)

- `url/`          — URL parse + normalization + origin (Fase C1). **Live:**
  consumed by `tests/test_url.c` via `make test-url`.
- `html-to-text/` — `CapyBrowse Text`: title, blocks, numbered links,
  warnings, truncation (Fase C2). Scaffolding.
- `display-list/` — static display-list nodes: text runs, rectangles, image
  placeholders, link bounds, form controls, scroll extent, accessibility
  labels (Fase M3). **Live:** consumed by `tests/test_displaylist.c` via
  `make test-displaylist`.
- `page/`         — production HTML + CSS -> display-list pipeline, including
  inert-element filtering and page-policy warnings. **Live:** consumed by
  `tests/test_page.c` via `make test-page`.
- `malformed/`    — tolerant recovery + fail-closed rejection across surfaces.
  Scaffolding.

Scaffolding directories receive real fixtures together with the surface that
consumes them (see `../../docs/roadmap.md`).
