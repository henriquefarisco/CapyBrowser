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
 * Fase C3 introduces the image-decode part of the adapter; Fase M4d adds the
 * download sink + privacy flags. Network/cache/cookie callbacks are added
 * additively in later phases; existing fields never change meaning.
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

/*
 * Streaming download sink (Fase M4d). The host owns the filesystem; the
 * browser-core download surface (Fase M4a) has already validated the URL
 * (HTTPS-first) and produced a sanitized, path-component-free filename. The
 * host opens a destination, receives the bytes in order, then finalizes.
 *
 * Protocol: download_open returns an opaque non-NULL handle on success, or NULL
 * on failure. download_append is then called zero or more times with
 * consecutive chunks; it returns 0 to continue or negative to abort the
 * transfer. download_close finalizes (ok != 0) or discards (ok == 0) the
 * destination and releases the handle. total_size_hint is the declared length,
 * or 0 if unknown. Implementations must not block on network and must be
 * deterministic with respect to the bytes delivered.
 */
typedef void *(*capy_host_download_open_fn)(const char *filename,
                                            uint64_t total_size_hint,
                                            void *user_data);
typedef int (*capy_host_download_append_fn)(void *handle, const uint8_t *data,
                                            size_t len, void *user_data);
typedef void (*capy_host_download_close_fn)(void *handle, int ok,
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

  /*
   * Download sink (Fase M4d), injected by the host. All three are NULL when
   * downloads are unsupported; the core then refuses a download fail-closed.
   * download_open/append/close form one transfer (see the typedefs above).
   */
  capy_host_download_open_fn download_open;
  capy_host_download_append_fn download_append;
  capy_host_download_close_fn download_close;
  void *download_user_data;

  /*
   * Privacy flags (Fase M4b) the host must honour. Default 0 = a normal,
   * non-ephemeral session with third-party loads allowed, so a zero-initialized
   * adapter preserves the prior behavior. The application's session mode sets
   * these; the host's cache/cookie/network hooks (added additively later) must
   * respect them.
   */
  int ephemeral_session; /* 1 = do not persist cookies/cache */
  int block_third_party; /* 1 = no automatic third-party loads */

  /* Future, additive: network fetch, cache/cookie hooks. */
};

#endif /* CAPY_HOST_ADAPTER_H */
