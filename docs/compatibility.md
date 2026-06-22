# CapyBrowser compatibility and integration contract

CapyBrowser owns the **portable browser-core logic** (URL parsing,
HTML-to-text, static HTML/CSS parse, future display-list). CapyBrowser
modules must remain portable browser-core logic and must not depend
on CapyOS runtime internals.

## CapyOS reference version

- CapyOS core pinned for this contract: `0.8.0-alpha.265+20260611`
- Authoritative cross-repo matrix: [`CapyOS/docs/reference/integration/compatibility-matrix.md`](../../CapyOS/docs/reference/integration/compatibility-matrix.md)
- Canonical manifest format consumed by the in-tree adapter: [`CapyOS/docs/reference/integration/capypkg-publisher-manifest-format.md`](../../CapyOS/docs/reference/integration/capypkg-publisher-manifest-format.md)
- Manual deploy runbook: [`CapyOS/docs/operations/manual-module-deploy-runbook.md`](../../CapyOS/docs/operations/manual-module-deploy-runbook.md)
- Current cross-repo audit: [`CapyOS/docs/reference/integration/compatibility-audit-2026-06-11.md`](../../CapyOS/docs/reference/integration/compatibility-audit-2026-06-11.md)

## Authoritative CapyOS references

- `CapyOS/docs/reference/integration/modular-installation-architecture.md`
- `CapyOS/docs/reference/integration/browser-core-integration-contract.md`
- `CapyOS/docs/reference/integration/media-codec-integration-contract.md`
- `CapyOS/docs/reference/integration/external-core-repositories.md`

## Owned ABI

CapyBrowser owns the `capy-browser-core` ABI. The Etapa 6 text subset is
published in `v0.6.0` as a package handoff for CapyOS Slice 6.4; graphical
runtime integration remains gated by Etapa 7.

This ABI covers:

- URL parsing and normalization;
- HTML-to-text output (CapyBrowse Text for Etapa 6);
- static HTML/CSS parse contracts (Etapa 7);
- future display-list format (versioned, additive, deterministic);
- deterministic parse/layout errors;
- internal limits on memory, time and input size;
- download orchestration (planned, additive, Etapa 7) — URL resolution,
  metadata parse, deterministic filename derivation and streaming to a
  host-provided download sink (see "Download surface" below);
- private/anonymous session controls (planned, additive, Etapa 7+) —
  ephemeral state and minimal request identity at the application layer
  (see "Private session surface" below).

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

## URL surface (Fase C1) — implemented (host-testable)

The first concrete `capy-browser-core` surface. Lives in `src/url/`
(`url_parse.{c,h}`, `url_normalize.c`, `origin.c`) and is exercised by the
golden fixtures under `tests/fixtures/url/` via `make test-url`. It is pure,
deterministic and allocation-free; it performs no network, filesystem, clock or
RNG access. **Status:** implemented and host-testable; pending external
`make validate` and cross-repo ratification before the contract is authoritative
(see "Cross-repo ratification" below). It does not yet imply a `0.1.0` release.

Entry points:

- `capy_url_parse(input, base, out, warnings)` — parse + RFC 3986 relative
  resolution against an optional absolute base + normalize. The result is always
  absolute; a relative reference without an absolute base is rejected.
- `capy_url_serialize(url, buf, cap)` — deterministic recomposition.
- `capy_url_origin(url, out)` / `capy_url_origin_equal(a, b)` — origin tuple
  `(scheme, host, effective port)` and comparison.

Deterministic normalization rules:

- scheme and host are lower-cased; path/query/fragment case is preserved;
- an explicit default port is dropped (`https`/`wss` 443, `http`/`ws` 80,
  `ftp` 21); the effective port is still exposed for the origin;
- `.`/`..` path segments are resolved (RFC 3986 5.2.4); `..` at root is clamped,
  never escapes;
- percent-encoding is normalized: `%xx` of unreserved octets is decoded, other
  triplets keep upper-cased hex; bytes `>= 0x80` are percent-encoded;
- an authority with an empty path normalizes to `/`.
- Normalization is **idempotent**: re-parsing a serialized URL reproduces it.

Deliberate decisions (documented, additive):

- HTTPS-first is enforced by the host adapter, not here: non-HTTPS schemes still
  parse; the adapter rejects the fetch.
- userinfo (`user@host`) is rejected (anti-phishing) as `CAPY_URL_ERR_HOST`.
- IDNA/punycode is out of scope for C1; non-ASCII host bytes are percent-encoded.
- Control bytes (`< 0x20` or `0x7F`) and raw spaces are rejected up front.

Error codes (negative `enum capy_url_status`): `NULL`, `EMPTY`, `TOO_LONG`,
`CONTROL`, `SPACE`, `PERCENT`, `SCHEME`, `HOST`, `PORT`, `BASE`, `OVERFLOW`.

Warning codes (`enum capy_url_warning`, emitted in a fixed canonical order so the
sequence is deterministic): `PERCENT_CASE_NORMALIZED`,
`PERCENT_UNRESERVED_DECODED`, `NON_ASCII_PCT_ENCODED`, `DEFAULT_PORT_DROPPED`,
`DOT_SEGMENTS_RESOLVED`.

Limits: input `<= CAPY_URL_MAX_LEN` (2048, aligned with CapyOS
`HTTP_MAX_URL`); per-component caps (scheme 32, host 256, path/query/fragment
2048) fail closed with `CAPY_URL_ERR_OVERFLOW`.

## HTML-to-text surface ("CapyBrowse Text", Fase C2) — implemented (host-testable)

The first user-facing surface. Lives in `src/text/` (`html_entities.{c,h}`,
`html_tokenizer.{c,h}`, `text_emit.c` + the public `html_text.h`) and is
exercised by golden fixtures under `tests/fixtures/html-to-text/` via
`make test-text`. It depends on the Fase C1 URL core to resolve link targets and
on the `capy-codec-image` ABI for nothing yet (images arrive in Fase C3). Pure,
deterministic, allocation-free; no network, filesystem, clock or RNG.
**Status:** implemented and host-testable; pending external `make validate`,
cross-repo ratification and the `0.2.0` release cut (target Etapa 6).

Entry point:

- `capy_html_to_text(html, html_len, base_url, text_buf, text_cap, out)` —
  renders the body into `text_buf` and fills `out` with the title, the resolved
  link targets, the warning set and a truncation flag.

Output (the "CapyBrowse Text" view):

- **title** — first `<title>`, entity-decoded and whitespace-normalized
  (`out->title` / `out->has_title`);
- **body** — normalized block text in `text_buf`; inline links appear as `[n]`
  markers;
- **links** — `out->links[1..n]` hold the resolved, normalized absolute URLs
  (resolved through `capy_url_parse` against `base_url`);
- **warnings** — `out->warnings` (deterministic, one of each in enum order);
- **truncation** — `out->truncated` plus the matching warning.

Deterministic rules:

- tolerant: malformed HTML never aborts; recovery is reproducible and flagged
  (`UNCLOSED_TAG`, `UNCLOSED_COMMENT`);
- no scripting: `<script>`/`<style>` content is dropped (JS stays blocked until
  Etapa 12);
- clean output: the body/title contain no control bytes other than the block
  separator `\n` (CRLF, tabs and other control bytes are normalized/dropped);
- whitespace runs collapse to a single space and blocks are trimmed; common
  named entities (including the complete HTML4 Latin-1 set, e.g. the accented
  letters `&Ccedil;`/`&Atilde;`/`&Otilde;`/`&Acirc;` and the symbol/punctuation
  block `&iexcl;`/`&iquest;`/`&micro;`/`&sup2;`), decimal and hex references are
  decoded to UTF-8; `&nbsp;` collapses to a space;
- preformatted text (`<pre>`) preserves its spaces, tabs and newlines verbatim
  (CRLF is normalized to LF; a single leading newline right after `<pre>` is
  dropped per the HTML rule); entities still decode inside `<pre>`;
- list items (`<li>`) render with a leading marker: `- ` inside `<ul>` and a
  1-based `N. ` inside `<ol>`, indented two spaces per nesting level (bounded
  depth; a stray `<li>` with no open list falls back to a bullet);
- numeric character references in the C1 range (`&#128;`–`&#159;` /
  `&#x80;`–`&#x9F;`) follow the WHATWG numeric-reference remap to the intended
  Windows-1252 punctuation (e.g. `&#151;` → em dash U+2014, `&#147;`/`&#148;` →
  curly double quotes U+201C/U+201D, `&#128;` → euro sign U+20AC); the five
  values with no Windows-1252 assignment (`&#129;`, `&#141;`, `&#143;`,
  `&#144;`, `&#157;`) are dropped as invalid C1 controls with `ENTITY_INVALID`,
  so the body and title never carry C1 control characters (named entities are
  unaffected);
- numeric character references that are NULL (`&#0;`), a surrogate half
  (`&#xD800;`–`&#xDFFF;`) or out of range (above `U+10FFFF`) follow the WHATWG
  numeric-reference end state and resolve to U+FFFD REPLACEMENT CHARACTER —
  emitted in the body/title and flagged `ENTITY_INVALID` (a parse error)
  rather than silently dropped;
- links resolve via C1; an `href` that cannot resolve (e.g. relative with no
  base) is dropped from the numbered list with `LINK_UNRESOLVED`.

Limits (alpha): HTML input `<= CAPY_TEXT_MAX_INPUT` (256 KiB); up to
`CAPY_TEXT_MAX_LINKS` (64) numbered links; title `<= CAPY_TEXT_TITLE_MAX` (256).
Over-budget input/output/links/title set `out->truncated` and a warning. A
single-pass O(n) tokenizer plus the input cap bound parse time; an injected
parse-time clock budget is additive future work.

Warnings (`enum capy_text_warning`, fixed canonical order): `INPUT_TRUNCATED`,
`OUTPUT_TRUNCATED`, `TITLE_TRUNCATED`, `LINK_BUDGET`, `LINK_UNRESOLVED`,
`ENTITY_INVALID`, `UNCLOSED_TAG`, `UNCLOSED_COMMENT`.

Deferred to later phases (documented, additive): full attribute model and a
DOM tree (Fase M1), CSS (Fase M2), image placeholders via `capy-codec-image`
(Fase C3), IDNA/punycode, and UTF-8 input validation/replacement.

## Image adapter surface (Fase C3) — implemented (host-testable)

CapyBrowser never decodes images. It requests a decode through an injected host
adapter callback that routes to the `capy-codec-image` ABI (owned by
CapyCodecs), and turns any failure into a deterministic, non-fatal placeholder
so the page still renders. There is no browser-local codec; the deprecated
`src/codecs/` BMP snapshot is superseded by this adapter and stays out of the
decode path.

Files: `src/adapter/host_adapter.h` (the injection surface) and
`src/codec/image_adapter.{c,h}` (orchestration), exercised by
`tests/test_image_adapter.c` with a deterministic stub codec via
`make test-image`. **Status:** implemented and host-testable; pending external
`make validate`, cross-repo ratification and consumption by the static
display-list (Fase M3 / Etapa 7). Render itself only happens at Etapa 7.

Injection surface (`struct capy_host_adapter`, supplied by CapyOS):

- `decode_image(data, len, out, user) -> int` — decode encoded bytes to
  host-owned RGBA8888 via `capy-codec-image`; 0 on success, negative on failure;
- `release_image(image, user)` — free pixels from a successful decode;
- `codec_user_data`, plus `max_image_width` / `max_image_height` (0 selects the
  built-in default `CAPY_IMAGE_DEFAULT_MAX_DIM` = 4096).
- `download_open` / `download_append` / `download_close` + `download_user_data`
  (Fase M4d) - the streaming download sink the host writes to; NULL when
  downloads are unsupported (the core then refuses a download fail-closed);
- `ephemeral_session` / `block_third_party` (Fase M4b) - session privacy flags
  the host honours; default 0 = a normal session (zero-init preserves behavior);
- Network/cache/cookie hooks are additive future fields; existing fields never
  change meaning.

Orchestration (`capy_image_request` / `capy_image_release`):

- fail-closed and non-fatal: a missing decoder, empty input, codec error, bad
  dimensions or an over-budget image all yield `CAPY_IMAGE_PLACEHOLDER` with a
  deterministic `enum capy_image_reason` (`NO_DECODER`, `EMPTY_INPUT`,
  `DECODE_FAILED`, `BAD_DIMENSIONS`, `TOO_LARGE`) — never a page failure;
- ownership: pixels stay host-owned; the core releases them via `release_image`
  on rejection and on `capy_image_release` (which is idempotent);
- decoupled: `host_adapter.h` only declares callback types and an RGBA struct;
  it includes no CapyOS or CapyCodecs headers.

## DOM-like parse surface (Fase M1) — implemented (host-testable)

A tolerant HTML → element/text tree, built by reusing the Fase C2 tokenizer. It
is the structural substrate for CSS (Fase M2) and static layout / display-list
(Fase M3); it is **not** a full HTML5 tree builder (no implied-tag insertion,
no adoption-agency re-parenting). Lives in `src/html/` (`dom.{c,h}`,
`html_parse.c`, internal `dom_internal.h`), exercised by `tests/test_html.c`
with golden tree dumps under `tests/fixtures/dom/` via `make test-html`.
**Status:** implemented and host-testable; pending external `make validate` and
the `0.3.0` release cut (prep Etapa 7).

Entry point and shape:

- `capy_html_parse(html, html_len, doc)` — builds the tree into a caller-provided
  `struct capy_dom_doc` (allocation-free arena: node pool + attribute pool +
  string arena). Accessors: `capy_dom_node_at`, `capy_dom_string`,
  `capy_dom_find_attr`.
- Nodes are `CAPY_DOM_DOCUMENT` (synthetic root), `CAPY_DOM_ELEMENT`
  (lower-cased tag + attributes) or `CAPY_DOM_TEXT`. Tree links are pool indices
  (`CAPY_DOM_NONE` sentinel).
- Attribute names are lower-cased; attribute values and text are entity-decoded
  to UTF-8 (except `<script>`/`<style>` text, kept raw); the DOM **preserves
  whitespace** (collapsing is a layout concern); non-whitespace control bytes
  are dropped.

Tolerant, deterministic rules:

- void elements (`area base br col embed hr img input link meta param source
  track wbr`) never get children; an end tag closes the nearest matching open
  element (and anything still open inside it); a stray end tag warns and is
  ignored;
- nesting depth is bounded by `CAPY_DOM_MAX_DEPTH`; node/attr/string arenas are
  bounded — overflow sets `doc->truncated` and a budget warning, never a crash;
- same input → same tree, same warning sequence (recorded once each in
  first-occurrence order).

Warnings (`enum capy_dom_warning`): `INPUT_TRUNCATED`, `NODE_BUDGET`,
`ATTR_BUDGET`, `STRING_BUDGET`, `DEPTH_LIMIT`, `STRAY_END_TAG`, `UNCLOSED_TAG`,
`UNCLOSED_COMMENT`.

Limits (alpha, configurable): nodes `CAPY_DOM_MAX_NODES` (1024), attributes
`CAPY_DOM_MAX_ATTRS` (1024), string arena `CAPY_DOM_STRING_ARENA` (64 KiB),
depth `CAPY_DOM_MAX_DEPTH` (128), HTML input 256 KiB. The tokenizer now exposes
a general attribute list (`struct capy_html_attr`) additively; the C2 `href`
fast-path is unchanged.

## CSS parse surface (Fase M2, part 1) - implemented (host-testable)

A tolerant CSS parser producing a deterministic stylesheet of rules - the input
to the cascade (Fase M2, part 2). Lives in `src/css/` (`css_parse.{c,h}`),
exercised by golden fixtures under `tests/fixtures/css/` via `make test-css`.
Pure, allocation-free (caller-provided `struct capy_css_stylesheet` arena),
deterministic and fail-closed; no network, filesystem, clock or RNG. **Status:**
implemented and host-testable; cascade onto the DOM and value semantics are the
next M2 step.

Entry point:

- `capy_css_parse(css, css_len, out)` - parse into rules; always resets `*out`;
  returns `CAPY_CSS_OK` or `CAPY_CSS_ERR_NULL`.

Supported selector subset (one simple selector per comma-separated entry):

- universal `*`, type `tag` (lower-cased), class `.name`, id `#name` (class/id
  names keep case to match HTML class/id values);
- comma lists are expanded so each rule carries exactly one selector, in source
  order (used later for cascade tie-breaking).

Declarations are stored generically as `property: value` pairs (property
lower-cased; value raw and end-trimmed); the cascade interprets the known
property subset. The parser attaches no value semantics yet.

Deterministic tolerant rules:

- block comments and whitespace are skipped; an unclosed comment or block warns;
- at-rules (`@media`, ...) are skipped whole with `AT_RULE_SKIPPED`;
- unsupported selectors (compound, descendant, combinator, attribute, pseudo)
  are dropped with `SELECTOR_SKIPPED`; malformed declarations with
  `DECL_SKIPPED`; recovery is reproducible byte-for-byte.

Limits (alpha, configurable): input `<= CAPY_CSS_MAX_INPUT` (256 KiB); rules
`CAPY_CSS_MAX_RULES` (256); declarations `CAPY_CSS_MAX_DECLS` (1024); string
arena `CAPY_CSS_STRING_ARENA` (32 KiB). Over-budget input/rules/declarations/
strings set `out->truncated` and the matching budget warning.

Warnings (`enum capy_css_warning`, fixed canonical order): `INPUT_TRUNCATED`,
`RULE_BUDGET`, `DECL_BUDGET`, `STRING_BUDGET`, `AT_RULE_SKIPPED`,
`SELECTOR_SKIPPED`, `DECL_SKIPPED`, `UNCLOSED_BLOCK`, `UNCLOSED_COMMENT`.

## CSS cascade surface (Fase M2, part 2) - implemented (host-testable)

The cascade matches the parsed stylesheet onto the Fase M1 DOM and computes, fo
every node, a computed style over a documented property subset. Lives in
`src/css/` (`cascade.{c,h}`), exercised by golden fixtures unde
`tests/fixtures/cascade/` via `make test-cascade`. Pure, allocation-free
(caller-provided `struct capy_css_cascade` arena sized to the DOM node pool),
deterministic; no network, filesystem, clock or RNG.

Entry point:

- `capy_css_cascade(dom, sheet, out)` - fills one computed style per DOM node;
  always resets `*out`; returns `CAPY_CSS_CASCADE_OK` o
  `CAPY_CSS_CASCADE_ERR_NULL`. Computed values are ranges into `sheet->strings`.

Matching and specificity:

- a simple selector matches by tag (type), a `class` token (space-separated,
  case-sensitive), `id` (case-sensitive) or `*` (any element);
- specificity is id (100) > class (10) > type (1) > universal (0); per property
  the winning declaration is the highest specificity, ties broken by late
  source order (rules are applied in document order).

Known property subset (additive `enum capy_css_prop`): `display`, `color`,
`background-color`, `font-weight`, `font-style`, `text-align`,
`text-decoration`. Inherited properties (`color`, `font-weight`, `font-style`,
`text-align`) fall back to the parent's computed value when unset; the others
use the initial (unset) state. Unknown properties are ignored.

Determinism: same `(DOM, stylesheet)` produces the same computed style for every
node, inheritance included. Additive: new properties append before
`CAPY_CSS_PROP_COUNT`; existing indices never change.

## Static layout surface (Fase M3, part a) - implemented (host-testable)

Static block layout consuming the M1 DOM and M2 computed styles, producing a box
tree with geometry - the input to the display-list emitter (Fase M3, part b).
Lives in `src/layout/` (`layout.{c,h}`), exercised by golden fixtures unde
`tests/fixtures/layout/` via `make test-layout`. Pure, allocation-free
(caller-provided `struct capy_layout_tree` arena), deterministic, fail-closed;
no network, filesystem, clock or RNG.

Entry point:

- `capy_layout(dom, sheet, casc, viewport_width, out)` - lays the styled
  document into a box tree; always resets `*out`; returns `CAPY_LAYOUT_OK` o
  `CAPY_LAYOUT_ERR_NULL`.

Model (a deliberately simple first layout):

- vertical block flow: every rendered element is a block box that stacks its
  children and spans the parent content width;
- `display: none` (from the cascade) removes an element and its subtree;
- text nodes collapse ASCII whitespace and greedy-wrap to the content width,
  each becoming a text box whose height is its line count;
- no inline flow, margins/padding/borders, floats or positioning yet (additive);
- geometry is in abstract cells (1 column wide, 1 line tall) - a deterministic,
  font-independent monospace approximation; real pixel metrics arrive with a
  font backend.

Limits (alpha, configurable): boxes `CAPY_LAYOUT_MAX_BOXES` (2048); nesting
`CAPY_LAYOUT_MAX_DEPTH` (128). Over-budget sets `out->truncated` and a warning
(`enum capy_layout_warning`: `BOX_BUDGET`, `DEPTH_LIMIT`).

Determinism: same `(DOM, stylesheet, computed styles, viewport width)` produces
the same box tree (kinds, geometry, ordering). Additive: new box kinds, box
fields and warnings append; existing ones never change meaning.

## Display-list surface (Fase M3, part b) - implemented (host-testable)

The display-list emitter walks the M3a box tree (with M2 computed styles and the
M1 DOM) and produces a flat, ordered, **versioned** list of draw nodes plus the
scroll extent - the compositor-independent data the CapyOS render backend will
consume. Lives in `src/displaylist/` (`display_list.{c,h}`), exercised by golden
fixtures under `tests/fixtures/display-list/` via `make test-displaylist`. Pure,
allocation-free (caller-provided `struct capy_dl` arena), deterministic,
fail-closed; no network, filesystem, clock, RNG or image decode (placeholders).

Entry point:

- `capy_displaylist(dom, sheet, casc, layout, base_url, out)` - emits the list;
  always resets `*out`; returns `CAPY_DL_OK` or `CAPY_DL_ERR_NULL`. `base_url`
  (may be NULL) resolves link hrefs through Fase C1.

Versioning: `out->version` is `CAPY_DL_VERSION` (currently 1). The schema is
**additive** - new node kinds, node fields and warnings append; existing ones
never change meaning or numbering.

Node kinds (`enum capy_dl_node_kind`); per-element emission order is background
first, then image/link markers, then the element's children in document order:

- `RECT` - a background fill for an element with a computed `background-color`
  (geometry = the box; payload = the color value);
- `IMAGE` - a placeholder for an `<img>` element (geometry = the box; payload =
  the `alt` text as an accessibility label, when present). No decode happens
  here; pixels are a later `capy-codec-image` concern;
- `LINK` - a link bound for an `<a>` element whose `href` resolves to an
  absolute URL through Fase C1 (geometry = the box; payload = the resolved URL).
  An href that cannot resolve is dropped (no node);
- `TEXT` - a text run for a text box (geometry = the box; payload = the
  whitespace-collapsed run text plus the computed text color when set).

Scroll extent: `out->content_width` / `out->content_height` carry the laid-out
size. Form controls and per-line text-run splitting are deferred (additive).

Limits (alpha, configurable): nodes `CAPY_DL_MAX_NODES` (4096); string arena
`CAPY_DL_STRING_ARENA` (64 KiB). Over-budget sets `out->truncated` and a warning
(`enum capy_dl_warning`: `NODE_BUDGET`, `STRING_BUDGET`).

Determinism: same `(DOM, stylesheet, computed styles, box tree, base URL)`
produces the same node sequence, geometry and strings.

## Reference host front-end (outside the `capy-browser-core` ABI)

`host/` contains a reference command-line front-end ("CapyBrowse Text") that
consumes the pure core (Fase C1 URL + Fase C2 HTML-to-text) and supplies the
side effects the core must not perform: a local-file/stdin reader (always built)
and an opt-in HTTPS fetch backend (libcurl, `-DCAPY_HOST_HAVE_CURL`). It lives
outside `src/`, so the decoupling discipline is intact, and it enforces
HTTPS-first at the adapter (a non-HTTPS fetch is refused; the core only parses
the URL; followed redirects are restricted to HTTPS). No JavaScript is executed.
It applies the Fase M4b session request identity to each HTTPS fetch (the minimal
static User-Agent and the computed Referer); `--private` selects the ephemeral
mode (no Referer sent). A response whose Content-Type is not `text/*` is treated
as a download: the Fase M4a core derives a sanitized filename (from
Content-Disposition or the URL path) and the bytes are written to it, fail-closed
on a truncated transfer or a rejected verdict.

It is **reference tooling, not part of the ABI**: it adds no error code, warning
or display-list node, so it requires no cross-repo ratification. When the CapyOS
Etapa 6 adapter lands it supersedes this front-end for the desktop; the text
mode remains the documented fallback. Build via `make capybrowse` (offline) o
`make capybrowse-net` (HTTPS).

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
| Download rejected (planned) | download validator rejects (non-HTTPS, dangerous redirect, invalid/empty/traversal filename) | UI displays download blocked |
| Download exceeds size limit (planned) | download stream exceeds declared budget | UI displays "download too large" |

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
| Maximum download size | declared-length check fail-closed (M4a); stream enforcement planned | CapyBrowser (enforce) + CapyOS (storage quota) |
| Maximum derived filename length | bounded to 255 bytes (`CAPY_DOWNLOAD_FILENAME_MAX`, M4a) | CapyBrowser |
| Capy package payload | ≤ 1 MiB during alpha streaming-buffer window | CapyOS adapter |

## Download surface (decision core implemented; streaming additive, Etapa 7)

The **decision core (Fase M4, part a) is implemented and host-testable**:
`src/download/` (`download.{c,h}`) provides `capy_download_prepare(url, base_url,
content_disposition, content_length, max_size, out)` - a pure, deterministic,
fail-closed function that validates the URL (HTTPS-first via Fase C1), derives
and sanitizes the filename and enforces the size budget, returning a verdict
(`ACCEPT`, `REJECT_URL`, `REJECT_SCHEME`, `REJECT_TOO_LARGE`,
`REJECT_FILENAME`). Exercised by `tests/test_download.c` via `make test-download`.
The byte streaming to a host download sink (the `download_open` /
`download_append` / `download_close` callbacks + `download_user_data`) is now
declared on `struct capy_host_adapter` (Fase M4d); the host implementation and
core wiring land at Etapa 7.

The streaming/runtime side is not active until the
Etapa 7 runtime opens and the CapyOS-side adapter exists. Responsibility is
split so CapyBrowser stays decoupled from network and filesystem.

CapyBrowser (browser-core) responsibilities:

- resolve and validate the download URL (HTTPS-first; reject non-HTTPS and
  dangerous/cross-scheme redirects);
- parse response metadata (MIME type, `Content-Disposition`);
- derive a deterministic, sanitized filename: strip path separators and
  `..`, reject control bytes and empty names, bound length;
- enforce the maximum download size fail-closed;
- stream bytes to a host-provided download sink via injected callbacks
  (`download_open` / `download_append` / `download_close`).

CapyOS (host) responsibilities:

- perform the actual filesystem write and the "save as" UX (with CapyUI);
- apply sandbox, storage quota and filename collision resolution;
- own the network transport that feeds the stream.

Rules:

- CapyBrowser never opens a socket and never writes a file directly.
- Deterministic: same `(URL, response headers, declared limits)` → same
  derived filename and same accept/reject verdict.
- Fail-closed: non-HTTPS, dangerous redirect, invalid filename or
  over-budget transfer reject with a documented error; nothing is written.
- New error codes and callbacks are additive; no existing element changes
  semantics.

## Private session surface (anonymous mode) (application core implemented; transport additive, Etapa 7+)

The **application-layer core (Fase M4, part b) is implemented and host-testable**:
`src/session/` (`session.{c,h}`) provides `capy_request_identity(mode,
current_url, target_url, out)` - a pure, deterministic function returning the
ephemeral-storage flag, the no-automatic-third-party flag, a minimal static
User-Agent ("CapyBrowse", no version leak) and the Referer under a
strict-origin-when-cross-origin policy (zeroed entirely in private mode; neve
sent on an HTTPS->non-HTTPS downgrade). Exercised by `tests/test_session.c` via
`make test-session`. Transport-level anonymity below remains a CapyOS concern.

Anonymity spans two layers. CapyBrowser owns only the application layer;
transport-level anonymity (proxy, onion routing, private DNS) is a CapyOS
responsibility.

CapyBrowser (browser-core) responsibilities in a private session:

- ephemeral state: no persistent cookies or cache (signalled via adapter
  flags); session state is discarded at session end;
- minimal, static `User-Agent` with no version leak, defined by the host
  adapter contract (the base system HTTP identity stays a CapyOS concern);
- `Referer` minimized or zeroed; request headers minimal and deterministic;
- no automatic third-party or external resource loading; no JavaScript
  (already guaranteed before Etapa 12, which removes most fingerprinting
  surface);
- no telemetry of any kind.

CapyOS (host) responsibilities:

- transport anonymity (proxy / onion routing), DNS policy, certificate
  policy, storage isolation and sandbox;
- the base system HTTP identity and the cookie/cache storage backends that
  the ephemeral flags toggle.

Rules:

- Privacy controls affect requests and stored state only; they must not
  change parse/display determinism. A private session must produce the same
  HTML-to-text and display-list output as a normal session for identical
  inputs.
- The private-session flag and the controlled-`User-Agent` contract are
  additive host-adapter inputs; no existing surface changes semantics.

## Form submission surface (Fase M4, part c) - implemented (host-testable)

Static (non-scripted) form submission. Lives in `src/forms/` (`forms.{c,h}`):
`capy_form_submit(method, action, base_url, fields, field_count, out)` is a
pure, deterministic, fail-closed function that `application/x-www-form-
urlencoded`-encodes the name/value fields (space -> `+`; alphanumerics and
`*-._` kept literal; everything else `%XX` upper-hex) and builds the request:
for GET, the action URL with its query replaced by the encoded data; for POST,
the resolved action URL plus the encoded body and the urlencoded content type.
HTTPS-first (the resolved action must be https; else `CAPY_FORM_ERR_SCHEME`) and
fail-closed on overflow (`CAPY_FORM_ERR_OVERFLOW`). No I/O, no JavaScript; the
fetch is the host's job. Exercised by `tests/test_forms.c` via `make test-forms`.

## Cross-repo ratification of planned surfaces

The download and private-session surfaces above are documented here as the
CapyBrowser-owned target, but they are **not ratified cross-repo** until the
matching CapyOS-side updates land (Etapa 7). Before they become authoritative:

- update [`../../CapyOS/docs/reference/integration/compatibility-matrix.md`](../../CapyOS/docs/reference/integration/compatibility-matrix.md)
  (CapyBrowser row plus any new error/limit rows);
- update [`../../CapyOS/docs/reference/integration/browser-core-integration-contract.md`](../../CapyOS/docs/reference/integration/browser-core-integration-contract.md)
  (download + private-session adapter contract);
- open a fresh `compatibility-audit-<date>.md` on the CapyOS side.

These are additive surfaces (new callbacks, new error codes, new limits);
they must not remove or repurpose any existing ABI element.

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

## Etapa 6 publication handoff (`v0.6.0`)

`v0.6.0` is the explicit handoff release for CapyOS Etapa 6 / Slice 6.4. It
publishes the text-mode package identity and manifest contract that the CapyOS
adapter can consume without waiting for image codecs or the graphical browser:

- package: `org.capyos.browser.text`;
- command: `make package STAGE=text`;
- payload URL base: GitHub release `v0.6.0`;
- dependency line: `depends=` (empty by design);
- owned surfaces: URL parse/normalize/origin, HTML-to-text, link extraction and
  deterministic error/warning model;
- non-owned surfaces: DNS/TCP/TLS/HTTP fetch, filesystem, window/input/render,
  sandbox and lifecycle, all supplied by CapyOS adapters.

The graphical package remains `org.capyos.browser.core` (`STAGE=core`) and keeps
the `org.capyos.codecs.image-basic` dependency for Etapa 7.

## Publishing as a Capy package

When CapyBrowser is delivered as a remote module to the CapyOS
`services/capypkg` adapter, the publisher must follow
[`CapyOS/docs/reference/integration/capypkg-publisher-manifest-format.md`](../../CapyOS/docs/reference/integration/capypkg-publisher-manifest-format.md).
The key requirements that affect CapyBrowser are:

- `payload_url` must be HTTPS only;
- `payload_sha256` must be lowercase 64 hex of the published artifact;
- `payload_size` ≤ 1 MiB during the alpha streaming-buffer window;
- `name` must match the alphabet `[a-zA-Z0-9._-]`; suggested canonical
  names: `org.capyos.browser.text` (Etapa 6) and
  `org.capyos.browser.core` (Etapa 7); the `Makefile` emits each name via
  `make package STAGE=text` / `STAGE=core` (default `core`);
- `install_root` must live under `/var/capypkg` or `/opt/`;
- the Ed25519 signature must cover the canonical descriptor
  `name=N|version=V|payload_sha256=H|payload_url=U\n`;
- `depends` is empty for `STAGE=text`; it must declare
  `org.capyos.codecs.image-basic` (or the active `capy-codec-image` package
  name) when image rendering is enabled.

Until CapyAgent publishes its Ed25519 signer, CapyBrowser cannot be
installed from a `signed` repository in production; lab tests with
`--unsigned` repositories are possible but must never be promoted.
