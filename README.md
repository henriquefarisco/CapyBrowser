# CapyBrowser

Version: 0.6.6

CapyBrowser owns browser-side portable components that can be validated outside the CapyOS kernel tree.

## Reference host app (CapyBrowse Text)

`host/` holds a decoupled reference front-end that turns the pure browser-core (URL + HTML-to-text) into a usable command-line text browser. It lives outside `src/` and supplies the side effects the core must not perform itself (HTTPS fetch, file read), so the decoupling contract holds and no JavaScript runs.

Offline build (render a local file or stdin; no external dependencies):

```sh
make capybrowse
build/capybrowse --file page.html
cat page.html | build/capybrowse -
```

HTTPS build (opt-in; requires libcurl; HTTPS-first, non-HTTPS refused):

```sh
make capybrowse-net
build/capybrowse https://example.com
build/capybrowse https://example.com -i        # links + 'b' back, 'q' quit
build/capybrowse https://example.com --page 20 # paginate long pages
build/capybrowse https://example.com --private # ephemeral: minimal UA, no Refere
build/capybrowse https://example.com/file.pdf  # non-HTML is saved as a download
```

## Validation

```sh
make validate
```

The release gate compiles with strict C warnings, runs codec contract tests, checks release metadata and verifies hardened compile flags.

## CapyOS handoff

`v0.6.0` is the Etapa 6 publication handoff for the text package:
`make package STAGE=text` emits `org.capyos.browser.text` without image codec
dependencies. The graphical `STAGE=core` package remains Etapa 7-gated and
keeps the `org.capyos.codecs.image-basic` dependency.

## Packaging

`make package` builds the dry-run `.bin` + `.manifest` consumed by the CapyOS adapter. `STAGE` selects the canonical package name by integration stage (see `docs/compatibility.md`):

```sh
make package STAGE=text   # Etapa 6: org.capyos.browser.text
make package STAGE=core   # Etapa 7: org.capyos.browser.core (default)
```
