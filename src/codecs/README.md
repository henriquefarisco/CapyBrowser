# Deprecated codec snapshot

This directory contains an early migration snapshot created before the dedicated `CapyCodecs` repository existed.

Canonical codec ownership is now:

- `CapyCodecs/src/image/capy_image.h`
- `CapyCodecs/src/image/bmp_decode.c`

Do not build new integrations against this browser-local codec snapshot. Browser integrations should consume `CapyCodecs` through the media-codec integration contract.
