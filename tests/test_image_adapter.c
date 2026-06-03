/*
 * Host-side test for the capy-browser-core image adapter (Fase C3).
 *
 * Uses deterministic stub decode callbacks (no real codec, no CapyOS): the
 * adapter must turn every failure mode into a non-fatal placeholder with a
 * deterministic reason, and must release host-owned pixels on rejection.
 */

#include "capy_test.h"
#include "image_adapter.h"

#include <string.h>

static uint32_t g_pixels[4] = {0u, 0u, 0u, 0u};
static int g_released;

static int dec_ok(const uint8_t *data, size_t len, struct capy_rgba_image *out,
                  void *user) {
  (void)data;
  (void)len;
  (void)user;
  out->width = 2u;
  out->height = 2u;
  out->pixels = g_pixels;
  return 0;
}

static int dec_fail(const uint8_t *data, size_t len, struct capy_rgba_image *out,
                    void *user) {
  (void)data;
  (void)len;
  (void)out;
  (void)user;
  return -1;
}

static int dec_huge(const uint8_t *data, size_t len, struct capy_rgba_image *out,
                    void *user) {
  (void)data;
  (void)len;
  (void)user;
  out->width = 100000u;
  out->height = 1u;
  out->pixels = g_pixels;
  return 0;
}

static int dec_zero(const uint8_t *data, size_t len, struct capy_rgba_image *out,
                    void *user) {
  (void)data;
  (void)len;
  (void)user;
  out->width = 0u;
  out->height = 0u;
  out->pixels = 0;
  return 0;
}

static void rel(struct capy_rgba_image *image, void *user) {
  (void)image;
  (void)user;
  g_released++;
}

static const uint8_t g_data[4] = {1u, 2u, 3u, 4u};

static void test_decode_success(struct capy_test_ctx *ctx) {
  struct capy_host_adapter a;
  struct capy_image_result r;
  int rc;
  memset(&a, 0, sizeof(a));
  a.decode_image = dec_ok;
  a.release_image = rel;
  g_released = 0;
  rc = capy_image_request(&a, g_data, sizeof(g_data), &r);
  CAPY_TEST_CHECK(ctx, rc == CAPY_IMAGE_OK);
  CAPY_TEST_CHECK(ctx, r.placeholder == 0);
  CAPY_TEST_CHECK_EQ_UINT(ctx, r.width, 2u);
  CAPY_TEST_CHECK_EQ_UINT(ctx, r.height, 2u);
  CAPY_TEST_CHECK(ctx, r.image.pixels == g_pixels);
  capy_image_release(&a, &r);
  CAPY_TEST_CHECK_EQ_UINT(ctx, (unsigned)g_released, 1u);
  CAPY_TEST_CHECK(ctx, r.placeholder == 1);
  CAPY_TEST_CHECK(ctx, r.image.pixels == 0);
  /* release is idempotent: a second call frees nothing more */
  capy_image_release(&a, &r);
  CAPY_TEST_CHECK_EQ_UINT(ctx, (unsigned)g_released, 1u);
}

static void test_no_decoder(struct capy_test_ctx *ctx) {
  struct capy_host_adapter a;
  struct capy_image_result r;
  memset(&a, 0, sizeof(a));
  CAPY_TEST_CHECK(ctx, capy_image_request(&a, g_data, sizeof(g_data), &r) ==
                           CAPY_IMAGE_PLACEHOLDER);
  CAPY_TEST_CHECK(ctx, r.placeholder == 1);
  CAPY_TEST_CHECK(ctx, r.reason == CAPY_IMAGE_REASON_NO_DECODER);
}

static void test_empty_input(struct capy_test_ctx *ctx) {
  struct capy_host_adapter a;
  struct capy_image_result r;
  memset(&a, 0, sizeof(a));
  a.decode_image = dec_ok;
  CAPY_TEST_CHECK(ctx,
                  capy_image_request(&a, 0, 0, &r) == CAPY_IMAGE_PLACEHOLDER);
  CAPY_TEST_CHECK(ctx, r.reason == CAPY_IMAGE_REASON_EMPTY_INPUT);
}

static void test_decode_failed(struct capy_test_ctx *ctx) {
  struct capy_host_adapter a;
  struct capy_image_result r;
  memset(&a, 0, sizeof(a));
  a.decode_image = dec_fail;
  a.release_image = rel;
  g_released = 0;
  CAPY_TEST_CHECK(ctx, capy_image_request(&a, g_data, sizeof(g_data), &r) ==
                           CAPY_IMAGE_PLACEHOLDER);
  CAPY_TEST_CHECK(ctx, r.reason == CAPY_IMAGE_REASON_DECODE_FAILED);
  /* nothing was decoded, so nothing is released */
  CAPY_TEST_CHECK_EQ_UINT(ctx, (unsigned)g_released, 0u);
}

static void test_too_large_releases(struct capy_test_ctx *ctx) {
  struct capy_host_adapter a;
  struct capy_image_result r;
  memset(&a, 0, sizeof(a));
  a.decode_image = dec_huge;
  a.release_image = rel;
  g_released = 0;
  CAPY_TEST_CHECK(ctx, capy_image_request(&a, g_data, sizeof(g_data), &r) ==
                           CAPY_IMAGE_PLACEHOLDER);
  CAPY_TEST_CHECK(ctx, r.reason == CAPY_IMAGE_REASON_TOO_LARGE);
  CAPY_TEST_CHECK_EQ_UINT(ctx, (unsigned)g_released, 1u);
}

static void test_bad_dimensions_releases(struct capy_test_ctx *ctx) {
  struct capy_host_adapter a;
  struct capy_image_result r;
  memset(&a, 0, sizeof(a));
  a.decode_image = dec_zero;
  a.release_image = rel;
  g_released = 0;
  CAPY_TEST_CHECK(ctx, capy_image_request(&a, g_data, sizeof(g_data), &r) ==
                           CAPY_IMAGE_PLACEHOLDER);
  CAPY_TEST_CHECK(ctx, r.reason == CAPY_IMAGE_REASON_BAD_DIMENSIONS);
  CAPY_TEST_CHECK_EQ_UINT(ctx, (unsigned)g_released, 1u);
}

static void test_custom_budget(struct capy_test_ctx *ctx) {
  struct capy_host_adapter a;
  struct capy_image_result r;
  memset(&a, 0, sizeof(a));
  a.decode_image = dec_ok; /* returns a 2x2 image */
  a.release_image = rel;
  a.max_image_width = 1u; /* tighter than the image */
  a.max_image_height = 1u;
  g_released = 0;
  CAPY_TEST_CHECK(ctx, capy_image_request(&a, g_data, sizeof(g_data), &r) ==
                           CAPY_IMAGE_PLACEHOLDER);
  CAPY_TEST_CHECK(ctx, r.reason == CAPY_IMAGE_REASON_TOO_LARGE);
  CAPY_TEST_CHECK_EQ_UINT(ctx, (unsigned)g_released, 1u);
}

static void test_null_args(struct capy_test_ctx *ctx) {
  struct capy_host_adapter a;
  struct capy_image_result r;
  memset(&a, 0, sizeof(a));
  a.decode_image = dec_ok;
  CAPY_TEST_CHECK(ctx, capy_image_request(0, g_data, sizeof(g_data), &r) ==
                           CAPY_IMAGE_ERR_NULL);
  CAPY_TEST_CHECK(ctx, capy_image_request(&a, g_data, sizeof(g_data), 0) ==
                           CAPY_IMAGE_ERR_NULL);
  /* release tolerates NULL */
  capy_image_release(0, &r);
  capy_image_release(&a, 0);
}

int main(void) {
  struct capy_test_ctx ctx;
  ctx.checks = 0;
  ctx.failures = 0;
  test_decode_success(&ctx);
  test_no_decoder(&ctx);
  test_empty_input(&ctx);
  test_decode_failed(&ctx);
  test_too_large_releases(&ctx);
  test_bad_dimensions_releases(&ctx);
  test_custom_budget(&ctx);
  test_null_args(&ctx);
  return capy_test_report(&ctx, "image-adapter");
}
