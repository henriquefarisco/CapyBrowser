#include "capy_determinism.h"
#include "capy_test.h"

static void test_rng_is_deterministic(struct capy_test_ctx *ctx) {
  struct capy_rng a;
  struct capy_rng b;
  int i;
  capy_rng_seed(&a, 0x00C0FFEEu);
  capy_rng_seed(&b, 0x00C0FFEEu);
  for (i = 0; i < 16; i++) {
    CAPY_TEST_CHECK(ctx, capy_rng_next_u64(&a) == capy_rng_next_u64(&b));
  }
}

static void test_rng_seed_changes_sequence(struct capy_test_ctx *ctx) {
  struct capy_rng a;
  struct capy_rng b;
  capy_rng_seed(&a, 1u);
  capy_rng_seed(&b, 2u);
  CAPY_TEST_CHECK(ctx, capy_rng_next_u64(&a) != capy_rng_next_u64(&b));
}

static void test_clock_is_injected_and_monotonic(struct capy_test_ctx *ctx) {
  struct capy_clock clk;
  capy_clock_init(&clk, 0u);
  CAPY_TEST_CHECK_EQ_UINT(ctx, capy_clock_now_ns(&clk), 0u);
  capy_clock_advance_ns(&clk, 1500u);
  CAPY_TEST_CHECK_EQ_UINT(ctx, capy_clock_now_ns(&clk), 1500u);
  capy_clock_advance_ns(&clk, 500u);
  CAPY_TEST_CHECK_EQ_UINT(ctx, capy_clock_now_ns(&clk), 2000u);
}

static void test_golden_bytes_match(struct capy_test_ctx *ctx) {
  static const unsigned char got[] = {0x10u, 0x20u, 0x30u};
  static const unsigned char want[] = {0x10u, 0x20u, 0x30u};
  CAPY_TEST_CHECK(
      ctx,
      capy_test_bytes_eq(ctx, got, sizeof(got), want, sizeof(want)) == 1);
}

static void test_golden_detects_divergence(struct capy_test_ctx *ctx) {
  static const unsigned char got[] = {0x10u, 0x20u, 0x30u};
  static const unsigned char want[] = {0x10u, 0x20u, 0x99u};
  struct capy_test_ctx scratch;
  scratch.checks = 0u;
  scratch.failures = 0u;
  CAPY_TEST_CHECK(
      ctx,
      capy_test_bytes_eq(&scratch, got, sizeof(got), want, sizeof(want)) == 0);
  CAPY_TEST_CHECK_EQ_UINT(ctx, scratch.failures, 1u);
}

int main(void) {
  struct capy_test_ctx ctx;
  ctx.checks = 0u;
  ctx.failures = 0u;
  test_rng_is_deterministic(&ctx);
  test_rng_seed_changes_sequence(&ctx);
  test_clock_is_injected_and_monotonic(&ctx);
  test_golden_bytes_match(&ctx);
  test_golden_detects_divergence(&ctx);
  return capy_test_report(&ctx, "harness");
}
