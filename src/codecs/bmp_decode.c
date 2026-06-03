#include "capy_image_codec.h"

struct bmp_file_header {
  uint16_t type;
  uint32_t size;
  uint16_t reserved1;
  uint16_t reserved2;
  uint32_t offset;
} __attribute__((packed));

struct bmp_info_header {
  uint32_t size;
  int32_t width;
  int32_t height;
  uint16_t planes;
  uint16_t bpp;
  uint32_t compression;
  uint32_t image_size;
  int32_t x_ppm;
  int32_t y_ppm;
  uint32_t colors_used;
  uint32_t colors_important;
} __attribute__((packed));

static void capy_image_rgba32_reset(struct capy_image_rgba32 *image) {
  if (!image) {
    return;
  }
  image->width = 0;
  image->height = 0;
  image->pixels = 0;
  image->allocator.alloc = 0;
  image->allocator.free = 0;
  image->allocator.user_data = 0;
}

void capy_image_rgba32_free(struct capy_image_rgba32 *image) {
  if (!image) {
    return;
  }
  if (image->pixels && image->allocator.free) {
    image->allocator.free(image->pixels, image->allocator.user_data);
  }
  capy_image_rgba32_reset(image);
}

int capy_bmp_decode_memory(const uint8_t *data, size_t size,
                           const struct capy_image_allocator *allocator,
                           struct capy_image_rgba32 *out) {
  const struct bmp_file_header *fh;
  const struct bmp_info_header *ih;
  int32_t width;
  int32_t height;
  int bottom_up;
  uint32_t bpp;
  uint32_t row_size;
  uint32_t pixel_offset;

  if (!out) {
    return -1;
  }
  capy_image_rgba32_reset(out);
  if (!data || !allocator || !allocator->alloc || size < 54u) {
    return -1;
  }

  fh = (const struct bmp_file_header *)data;
  if (fh->type != 0x4D42u) {
    return -1;
  }

  ih = (const struct bmp_info_header *)(data + 14u);
  if (ih->bpp != 24u && ih->bpp != 32u) {
    return -1;
  }
  if (ih->compression != 0u) {
    return -1;
  }

  width = ih->width;
  height = ih->height;
  bottom_up = height > 0 ? 1 : 0;
  if (height < 0) {
    height = -height;
  }
  if (width <= 0 || height <= 0 || (uint32_t)width > CAPY_IMAGE_MAX_WIDTH ||
      (uint32_t)height > CAPY_IMAGE_MAX_HEIGHT) {
    return -1;
  }

  bpp = ih->bpp;
  row_size = ((bpp * (uint32_t)width + 31u) / 32u) * 4u;
  pixel_offset = fh->offset;
  if (pixel_offset >= size) {
    return -1;
  }

  out->pixels = (uint32_t *)allocator->alloc(
      (size_t)((uint32_t)width * (uint32_t)height * 4u),
      allocator->user_data);
  if (!out->pixels) {
    capy_image_rgba32_reset(out);
    return -1;
  }
  out->allocator = *allocator;

  for (int32_t y = 0; y < height; ++y) {
    int32_t src_y = bottom_up ? (height - 1 - y) : y;
    size_t row_offset = (size_t)pixel_offset + (size_t)((uint32_t)src_y) *
                                                (size_t)row_size;
    const uint8_t *row;
    if (row_offset > size || row_size > size - row_offset) {
      capy_image_rgba32_free(out);
      return -1;
    }
    row = data + row_offset;
    for (int32_t x = 0; x < width; ++x) {
      uint32_t pixel;
      if (bpp == 32u) {
        uint32_t off = (uint32_t)x * 4u;
        pixel = 0xFF000000u | ((uint32_t)row[off + 2u] << 16) |
                ((uint32_t)row[off + 1u] << 8) | (uint32_t)row[off];
      } else {
        uint32_t off = (uint32_t)x * 3u;
        pixel = 0xFF000000u | ((uint32_t)row[off + 2u] << 16) |
                ((uint32_t)row[off + 1u] << 8) | (uint32_t)row[off];
      }
      out->pixels[y * (uint32_t)width + x] = pixel;
    }
  }
  out->width = (uint32_t)width;
  out->height = (uint32_t)height;

  return 0;
}
