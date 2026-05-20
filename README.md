# CapyBrowser

Version: 0.0.3

CapyBrowser owns browser-side portable components that can be validated outside the CapyOS kernel tree.

## Validation

```sh
make validate
```

The release gate compiles with strict C warnings, runs codec contract tests, checks release metadata and verifies hardened compile flags.
