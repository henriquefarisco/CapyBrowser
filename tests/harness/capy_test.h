#ifndef CAPY_TEST_H
#define CAPY_TEST_H

/*
 * Minimal, dependency-free host test harness for CapyBrowser.
 *
 * Header-only so every surface (URL parser, HTML-to-text, display-list,
 * malformed-input recovery) can include it without extra build wiring.
 * Deterministic by construction: no time, no randomness, no global state.
 */

#include <stddef.h>
#include <stdio.h>

struct capy_test_ctx {
  unsigned checks;
  unsigned failures;
};

#define CAPY_TEST_CHECK(ctx, expr)                                      \
  do {                                                                  \
    (ctx)->checks++;                                                    \
    if (!(expr)) {                                                      \
      (ctx)->failures++;                                                \
      fprintf(stderr, "[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #expr); \
    }                                                                   \
  } while (0)

#define CAPY_TEST_CHECK_EQ_UINT(ctx, got, want)                       \
  do {                                                                 \
    unsigned long capy_got_ = (unsigned long)(got);                   \
    unsigned long capy_want_ = (unsigned long)(want);                 \
    (ctx)->checks++;                                                   \
    if (capy_got_ != capy_want_) {                                    \
      (ctx)->failures++;                                               \
      fprintf(stderr, "[FAIL] %s:%d: %s = %lu, want %lu\n", __FILE__, \
              __LINE__, #got, capy_got_, capy_want_);                  \
    }                                                                  \
  } while (0)

/*
 * Golden helper: compare produced bytes against expected bytes and report the
 * first divergence. Records exactly one check on ctx; returns 1 on match,
 * 0 otherwise.
 */
static inline int capy_test_bytes_eq(struct capy_test_ctx *ctx,
                                     const unsigned char *got, size_t got_len,
                                     const unsigned char *want,
                                     size_t want_len) {
  size_t i;
  ctx->checks++;
  if (got_len != want_len) {
    ctx->failures++;
    fprintf(stderr, "[FAIL] byte length = %zu, want %zu\n", got_len, want_len);
    return 0;
  }
  for (i = 0; i < got_len; i++) {
    if (got[i] != want[i]) {
      ctx->failures++;
      fprintf(stderr, "[FAIL] byte %zu = 0x%02x, want 0x%02x\n", i,
              (unsigned)got[i], (unsigned)want[i]);
      return 0;
    }
  }
  return 1;
}

static inline int capy_test_report(const struct capy_test_ctx *ctx,
                                   const char *suite) {
  fprintf(stderr, "[%s] %u checks, %u failures\n", suite, ctx->checks,
          ctx->failures);
  return ctx->failures == 0 ? 0 : 1;
}

#endif /* CAPY_TEST_H */
