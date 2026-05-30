#ifndef CAPY_IMAGE_CODEC_H
#define CAPY_IMAGE_CODEC_H

#include <stddef.h>
#include <stdint.h>

#define CAPY_IMAGE_MAX_WIDTH 4096u
#define CAPY_IMAGE_MAX_HEIGHT 4096u

typedef void *(*capy_image_alloc_fn)(size_t size, void *user_data);
typedef void (*capy_image_free_fn)(void *ptr, void *user_data);

struct capy_image_allocator {
  capy_image_alloc_fn alloc;
  capy_image_free_fn free;
  void *user_data;
};

struct capy_image_rgba32 {
  uint32_t width;
  uint32_t height;
  uint32_t *pixels;
  struct capy_image_allocator allocator;
};

int capy_bmp_decode_memory(const uint8_t *data, size_t size,
                           const struct capy_image_allocator *allocator,
                           struct capy_image_rgba32 *out);
void capy_image_rgba32_free(struct capy_image_rgba32 *image);

#endif
