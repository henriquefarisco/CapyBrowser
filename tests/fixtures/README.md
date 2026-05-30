# CapyBrowser test fixtures

Golden fixtures for the `capy-browser-core` surfaces. Each fixture pairs an
input with its expected, deterministic output. A surface passes only when, for
identical `(input bytes, base URL, declared limits)`, it produces byte-for-byte
the same output **and** the same warning sequence.

## Convention

- `*.in`    — input bytes (HTML, URL list, etc.).
- `*.base`  — optional base URL for resolution (one line).
- `*.out`   — expected output (normalized URL / text / display-list dump).
- `*.warn`  — expected warning sequence, one code per line (may be empty).
- A case "name" is the shared filename stem, e.g. `relative-link.in` +
  `relative-link.out` + `relative-link.warn`.

Rules for every fixture:

- Deterministic: re-running the surface must reproduce `*.out` and `*.warn`
  exactly.
- Fail-closed cases live under `malformed/` and must show recovery warnings
  and a non-crashing verdict — never a process abort.
- No wall-clock, randomness or network: tests inject the deterministic clock
  and seeded PRNG from `../harness/capy_determinism.h`.

## Directories (one per surface)

- `url/`          — URL parse + normalization + origin (Fase C1).
- `html-to-text/` — `CapyBrowse Text`: title, blocks, numbered links,
  warnings, truncation (Fase C2).
- `display-list/` — static display-list nodes: text runs, rectangles, image
  placeholders, link bounds, form controls, scroll extent, accessibility
  labels (Fase M3).
- `malformed/`    — tolerant recovery + fail-closed rejection across surfaces.

These directories are scaffolding: real fixtures land together with the
surface that consumes them (see `../../docs/roadmap.md`).
