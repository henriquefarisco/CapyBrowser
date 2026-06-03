#ifndef CAPY_HOST_ADAPTER_H
#define CAPY_HOST_ADAPTER_H

/*
 * Host adapter injection surface for capy-browser-core.
 *
 * CapyBrowser is decoupled from CapyOS: anything outside pure browser-core
 * logic (network, filesystem, codecs, cache, cookies) is supplied by the host
 * integration as injected callbacks. This header declares the callbacks the
 * core consumes; CapyOS implements them (e.g. routing image decode to the
 * capy-codec-image ABI owned by CapyCodecs).
 *
 * Fase C3 introduces the image-decode part of the adapter. Network/cache/cookie
 * callbacks are added additively in later phases; existing fields never change
 * meaning.
 */

#include <stddef.h>
#include <stdint.h>

/*
 * A decoded image surface. Pixels are RGBA8888, row-major, length
 * width*height, and are OWNED BY THE HOST (allocated by the codec via the host
 * adapter). CapyBrowser never frees them directly; it calls release_image.
 */
struct capy_rgba_image {
  uint32_t width;
  uint32_t height;
  const uint32_t *pixels;
};

/*
 * Decode encoded image bytes (PNG/JPEG/BMP/WebP/...) into RGBA via the
 * capy-codec-image ABI. Returns 0 on success (fills *out), negative on failure.
 * Must be deterministic for a given host codec and must not block on network.
 */
typedef int (*capy_host_decode_image_fn)(const uint8_t *data, size_t len,
                                         struct capy_rgba_image *out,
                                         void *user_data);

/* Release pixels previously returned by a successful decode_image call. */
typedef void (*capy_host_release_image_fn)(struct capy_rgba_image *image,
                                           void *user_data);

struct capy_host_adapter {
  /*
   * Image codec hooks (capy-codec-image), injected by the host. decode_image
   * may be NULL when image rendering is disabled; the core then renders
   * placeholders. release_image may be NULL only if decode_image is too.
   */
  capy_host_decode_image_fn decode_image;
  capy_host_release_image_fn release_image;
  void *codec_user_data;

  /*
   * Image decode budget. 0 selects the built-in default
   * (CAPY_IMAGE_DEFAULT_MAX_DIM). Images exceeding the budget are rejected
   * fail-closed and rendered as placeholders.
   */
  uint32_t max_image_width;
  uint32_t max_image_height;

  /* Future, additive: network fetch, cache/cookie, resource-limit hooks. */
};

#endif /* CAPY_HOST_ADAPTER_H */
