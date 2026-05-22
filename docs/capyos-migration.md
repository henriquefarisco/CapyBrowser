# CapyOS migration notes

CapyBrowser is the external home for browser-core work that must remain decoupled from CapyOS internals.

## Current migration status

| Area | CapyOS source | Status in this repo |
|---|---|---|
| Historical browser/chrome | Removed from active `src/apps`; only legacy comments remain | No source to migrate; rebuild as decoupled core |
| BMP decode | `CapyOS/src/gui/core/bmp_loader.c`, `CapyOS/include/gui/bmp_loader.h` | Moved to canonical `CapyCodecs/src/image/` |
| PNG decode | `CapyOS/src/gui/core/png_loader.c`, `CapyOS/include/gui/png_loader.h` | Extracted under `CapyCodecs` with injected allocator and inflater |
| JPEG decode | `CapyOS/src/gui/core/jpeg_loader.c`, `CapyOS/include/gui/jpeg_loader.h` | Extracted under `CapyCodecs` with injected allocator |
| HTML-to-text | Not present in current active `src/apps` | New implementation required |
| HTML/CSS display list | Not present in current active `src/apps` | New implementation required |

## Ownership boundary

CapyBrowser owns:

- URL parsing and normalization;
- HTML-to-text;
- HTML/CSS static parsing;
- display-list generation;
- host-side fixtures and golden tests.

CapyOS owns:

- DNS/TCP/HTTP/TLS;
- certificate policy;
- window/input/render backend;
- cache/cookie storage;
- sandbox and per-page resource limits;
- app lifecycle and user-facing errors.

## Codec ownership

`CapyCodecs/src/image/bmp_decode.c` is now the canonical decoupled rewrite of
the existing CapyOS BMP loader. Any older browser-local codec snapshot is
non-canonical and should not be used for new integration work.

## Integration rule

CapyOS must consume this repository through:

- `CapyOS/docs/reference/integration/browser-core-integration-contract.md`
- `CapyOS/docs/reference/integration/media-codec-integration-contract.md`

The in-tree CapyOS GUI loaders remain legacy runtime code until a CapyOS adapter is added during the appropriate stage.

## Next migration slices

1. Add codec golden/corrupt fixtures in CapyCodecs.
2. Add HTML-to-text core with golden fixtures in CapyBrowser.
3. Add display-list schema for static pages in CapyBrowser.
4. Add CapyBrowser adapter to consume CapyCodecs outputs.
5. Add CapyOS adapters only when Etapa 6 or Etapa 7 is active.
