# Deprecated codec snapshot

This directory contains an early migration snapshot created before the dedicated `CapyCodecs` repository existed.

Canonical codec ownership is now:

- `CapyCodecs/src/image/capy_image.h`
- `CapyCodecs/src/image/bmp_decode.c`

Do not build new integrations against this browser-local codec snapshot. Browser integrations should consume `CapyCodecs` through the media-codec integration contract.

The Fase C3 image adapter (`src/adapter/host_adapter.h` + `src/codec/image_adapter.{c,h}`) supersedes this snapshot: image decode is requested through the injected host adapter callback (routed to the `capy-codec-image` ABI), never decoded here. This directory stays out of the decode path and will be removed once the legacy `tests/test_browser_codecs.c` is retired.
