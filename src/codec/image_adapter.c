#include "image_adapter.h"

#include <string.h>

static void result_reset_placeholder(struct capy_image_result *result,
                                     enum capy_image_reason reason) {
  memset(result, 0, sizeof(*result));
  result->placeholder = 1;
  result->reason = reason;
}

int capy_image_request(const struct capy_host_adapter *adapter,
                       const uint8_t *data, size_t len,
                       struct capy_image_result *result) {
  struct capy_rgba_image img;
  uint32_t max_w;
  uint32_t max_h;

  if (!adapter || !result) {
    return CAPY_IMAGE_ERR_NULL;
  }

  if (!adapter->decode_image) {
    result_reset_placeholder(result, CAPY_IMAGE_REASON_NO_DECODER);
    return CAPY_IMAGE_PLACEHOLDER;
  }
  if (!data || len == 0) {
    result_reset_placeholder(result, CAPY_IMAGE_REASON_EMPTY_INPUT);
    return CAPY_IMAGE_PLACEHOLDER;
  }

  memset(&img, 0, sizeof(img));
  if (adapter->decode_image(data, len, &img, adapter->codec_user_data) != 0) {
    result_reset_placeholder(result, CAPY_IMAGE_REASON_DECODE_FAILED);
    return CAPY_IMAGE_PLACEHOLDER;
  }

  if (img.width == 0 || img.height == 0 || img.pixels == NULL) {
    if (adapter->release_image) {
      adapter->release_image(&img, adapter->codec_user_data);
    }
    result_reset_placeholder(result, CAPY_IMAGE_REASON_BAD_DIMENSIONS);
    return CAPY_IMAGE_PLACEHOLDER;
  }

  max_w = adapter->max_image_width ? adapter->max_image_width
                                   : CAPY_IMAGE_DEFAULT_MAX_DIM;
  max_h = adapter->max_image_height ? adapter->max_image_height
                                    : CAPY_IMAGE_DEFAULT_MAX_DIM;
  if (img.width > max_w || img.height > max_h) {
    if (adapter->release_image) {
      adapter->release_image(&img, adapter->codec_user_data);
    }
    result_reset_placeholder(result, CAPY_IMAGE_REASON_TOO_LARGE);
    return CAPY_IMAGE_PLACEHOLDER;
  }

  result->placeholder = 0;
  result->reason = CAPY_IMAGE_REASON_OK;
  result->width = img.width;
  result->height = img.height;
  result->image = img;
  return CAPY_IMAGE_OK;
}

void capy_image_release(const struct capy_host_adapter *adapter,
                        struct capy_image_result *result) {
  if (!adapter || !result) {
    return;
  }
  if (!result->placeholder && result->image.pixels != NULL &&
      adapter->release_image) {
    adapter->release_image(&result->image, adapter->codec_user_data);
  }
  result->placeholder = 1;
  result->reason = CAPY_IMAGE_REASON_OK;
  result->width = 0;
  result->height = 0;
  result->image.width = 0;
  result->image.height = 0;
  result->image.pixels = NULL;
}

const char *capy_image_reason_name(enum capy_image_reason r) {
  switch (r) {
    case CAPY_IMAGE_REASON_OK:
      return "OK";
    case CAPY_IMAGE_REASON_NO_DECODER:
      return "NO_DECODER";
    case CAPY_IMAGE_REASON_EMPTY_INPUT:
      return "EMPTY_INPUT";
    case CAPY_IMAGE_REASON_DECODE_FAILED:
      return "DECODE_FAILED";
    case CAPY_IMAGE_REASON_BAD_DIMENSIONS:
      return "BAD_DIMENSIONS";
    case CAPY_IMAGE_REASON_TOO_LARGE:
      return "TOO_LARGE";
  }
  return "UNKNOWN";
}
