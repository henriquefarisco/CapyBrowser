# CapyBrowser compatibility and integration contract

CapyBrowser owns the **portable browser-core logic** (URL parsing,
HTML-to-text, static HTML/CSS parse, future display-list). CapyBrowser
modules must remain portable browser-core logic and must not depend
on CapyOS runtime internals.

## CapyOS reference version

- CapyOS core pinned for this contract: `0.8.0-alpha.262+20260602`
- Authoritative cross-repo matrix: [`CapyOS/docs/reference/integration/compatibility-matrix.md`](../../CapyOS/docs/reference/integration/compatibility-matrix.md)
- Canonical manifest format consumed by the in-tree adapter: [`CapyOS/docs/reference/integration/capypkg-publisher-manifest-format.md`](../../CapyOS/docs/reference/integration/capypkg-publisher-manifest-format.md)
- Manual deploy runbook: [`CapyOS/docs/operations/manual-module-deploy-runbook.md`](../../CapyOS/docs/operations/manual-module-deploy-runbook.md)
- Current cross-repo audit: [`CapyOS/docs/reference/integration/compatibility-audit-2026-06-02.md`](../../CapyOS/docs/reference/integration/compatibility-audit-2026-06-02.md)

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
  named, decimal and hex entities are decoded to UTF-8; `&nbsp;` collapses to a
  space;
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
| Maximum download size (planned) | configurable per integration stage; enforced fail-closed | CapyBrowser (enforce) + CapyOS (storage quota) |
| Maximum derived filename length (planned) | bounded (alpha target: 255 bytes) | CapyBrowser |
| Capy package payload | ≤ 1 MiB during alpha streaming-buffer window | CapyOS adapter |

## Download surface (planned, additive, Etapa 7)

File download is a planned additive surface. It is not active until the
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

## Private session surface (anonymous mode) (planned, additive, Etapa 7+)

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
  `org.capyos.browser.core` (Etapa 7); the `Makefile` emits each name via
  `make package STAGE=text` / `STAGE=core` (default `core`);
- `install_root` must live under `/var/capypkg` or `/opt/`;
- the Ed25519 signature must cover the canonical descriptor
  `name=N|version=V|payload_sha256=H|payload_url=U\n`;
- `depends` must declare `org.capyos.codecs.image-basic` (or the
  active `capy-codec-image` package name) when image rendering is
  enabled.

Until CapyAgent publishes its Ed25519 signer, CapyBrowser cannot be
installed from a `signed` repository in production; lab tests with
`--unsigned` repositories are possible but must never be promoted.
