#include "capy_image_codec.h"

static int failures;

#define EXPECT(expr)      \
  do {                    \
    if (!(expr)) {        \
      ++failures;         \
      return;             \
    }                     \
  } while (0)

struct test_heap {
  unsigned char storage[128];
  unsigned used;
  unsigned frees;
};

static void *test_alloc(size_t size, void *user_data) {
  struct test_heap *heap = (struct test_heap *)user_data;
  void *ptr;
  if (!heap || size > sizeof(heap->storage) - heap->used) {
    return 0;
  }
  ptr = heap->storage + heap->used;
  heap->used += (unsigned)size;
  return ptr;
}

static void test_free(void *ptr, void *user_data) {
  struct test_heap *heap = (struct test_heap *)user_data;
  if (heap && ptr) {
    ++heap->frees;
  }
}

static void test_decode_1x1_bmp(void) {
  static const unsigned char bmp[] = {
      0x42u, 0x4Du, 0x3Au, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
      0x00u, 0x00u, 0x36u, 0x00u, 0x00u, 0x00u, 0x28u, 0x00u,
      0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u,
      0x00u, 0x00u, 0x01u, 0x00u, 0x18u, 0x00u, 0x00u, 0x00u,
      0x00u, 0x00u, 0x04u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
      0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
      0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x33u, 0x22u,
      0x11u, 0x00u};
  struct test_heap heap = {{0}, 0u, 0u};
  struct capy_image_allocator allocator = {test_alloc, test_free, &heap};
  struct capy_image_rgba32 image;
  EXPECT(capy_bmp_decode_memory(bmp, sizeof(bmp), &allocator, &image) == 0);
  EXPECT(image.width == 1u);
  EXPECT(image.height == 1u);
  EXPECT(image.pixels[0] == 0xFF112233u);
  capy_image_rgba32_free(&image);
  EXPECT(heap.frees == 1u);
}

static void test_invalid_input_resets_output(void) {
  struct test_heap heap = {{0}, 0u, 0u};
  struct capy_image_allocator allocator = {test_alloc, test_free, &heap};
  struct capy_image_rgba32 image;
  image.width = 99u;
  image.height = 88u;
  image.pixels = (uint32_t *)1;
  image.allocator = allocator;
  EXPECT(capy_bmp_decode_memory(0, 0u, &allocator, &image) != 0);
  EXPECT(image.width == 0u);
  EXPECT(image.height == 0u);
  EXPECT(image.pixels == 0);
}

int main(void) {
  test_decode_1x1_bmp();
  test_invalid_input_resets_output();
  return failures == 0 ? 0 : 1;
}
