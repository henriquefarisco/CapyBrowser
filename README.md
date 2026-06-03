# CapyBrowser

Version: 0.3.0

CapyBrowser owns browser-side portable components that can be validated outside the CapyOS kernel tree.

## Validation

```sh
make validate
```

The release gate compiles with strict C warnings, runs codec contract tests, checks release metadata and verifies hardened compile flags.

## Packaging

`make package` builds the dry-run `.bin` + `.manifest` consumed by the CapyOS adapter. `STAGE` selects the canonical package name by integration stage (see `docs/compatibility.md`):

```sh
make package STAGE=text   # Etapa 6: org.capyos.browser.text
make package STAGE=core   # Etapa 7: org.capyos.browser.core (default)
```
