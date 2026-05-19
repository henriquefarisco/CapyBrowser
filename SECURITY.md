# Security Policy

CapyBrowser 0.0.2 is an early service release. Report security issues privately to the repository owner before opening public issues.

## Release gate

- `make validate` must pass before release tags.
- Codec inputs must fail closed and reset output buffers on invalid input.
- Build gates use strict C warnings and hardened compile flags.
