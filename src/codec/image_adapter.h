#ifndef CAPY_IMAGE_ADAPTER_H
#define CAPY_IMAGE_ADAPTER_H

/*
 * capy-browser-core image request orchestration (Fase C3).
 *
 * CapyBrowser does NOT decode images: it requests a decode through the host
 * adapter (which routes to the capy-codec-image ABI) and turns any failure into
 * a deterministic, non-fatal placeholder so the page still renders. There is no
 * browser-local codec here.
 */

#include <stddef.h>
#include <stdint.h>

#include "host_adapter.h"

/* Built-in image dimension budget when the adapter leaves it at 0. */
#define CAPY_IMAGE_DEFAULT_MAX_DIM 4096u

/* Return codes for capy_image_request. */
enum capy_image_status {
  CAPY_IMAGE_OK = 0,          /* decoded; result->image is valid */
  CAPY_IMAGE_PLACEHOLDER = 1, /* not an error: render a placeholder */
  CAPY_IMAGE_ERR_NULL = -1    /* NULL adapter or result */
};

/* Deterministic reason a placeholder was produced. */
enum capy_image_reason {
  CAPY_IMAGE_REASON_OK = 0,
  CAPY_IMAGE_REASON_NO_DECODER,     /* adapter has no decode_image hook */
  CAPY_IMAGE_REASON_EMPTY_INPUT,    /* no encoded bytes */
  CAPY_IMAGE_REASON_DECODE_FAILED,  /* codec returned an error */
  CAPY_IMAGE_REASON_BAD_DIMENSIONS, /* codec claimed success but dims/pixels bad */
  CAPY_IMAGE_REASON_TOO_LARGE       /* decoded image exceeds the budget */
};

struct capy_image_result {
  int placeholder;              /* 1 => render a placeholder, image is unset */
  enum capy_image_reason reason;
  uint32_t width;               /* decoded width (0 when placeholder) */
  uint32_t height;              /* decoded height (0 when placeholder) */
  struct capy_rgba_image image; /* host-owned pixels; valid iff !placeholder */
};

/*
 * Request a decode through the adapter. Never fails the page: on any problem it
 * returns CAPY_IMAGE_PLACEHOLDER with a deterministic result->reason. Returns
 * CAPY_IMAGE_OK when decoded, or CAPY_IMAGE_ERR_NULL on a NULL argument.
 */
int capy_image_request(const struct capy_host_adapter *adapter,
                       const uint8_t *data, size_t len,
                       struct capy_image_result *result);

/* Release a decoded image obtained from a successful request (idempotent). */
void capy_image_release(const struct capy_host_adapter *adapter,
                        struct capy_image_result *result);

/* Stable name for a reason code (logs/tests); "UNKNOWN" if unrecognized. */
const char *capy_image_reason_name(enum capy_image_reason r);

#endif /* CAPY_IMAGE_ADAPTER_H */
