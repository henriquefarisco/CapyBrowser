/*
 * Host-side test for the reference front-end's URL preparation
 * (capy_host_prepare_url): HTTPS-first enforcement, scheme defaulting and
 * deterministic fail-closed rejection. Pure; no network, no filesystem.
 */

#include "capy_test.h"

#include "capy_host.h"
#include "url_parse.h"

#include <string.h>

static void test_assumes_https(struct capy_test_ctx *ctx) {
  char out[CAPY_URL_MAX_LEN + 1];
  CAPY_TEST_CHECK(ctx, capy_host_prepare_url("example.com", out, sizeof(out)) ==
                           CAPY_HOST_OK);
  CAPY_TEST_CHECK(ctx, strcmp(out, "https://example.com/") == 0);
}

static void test_normalizes(struct capy_test_ctx *ctx) {
  char out[CAPY_URL_MAX_LEN + 1];
  CAPY_TEST_CHECK(ctx, capy_host_prepare_url("HTTPS://Example.COM/Path", out,
                                             sizeof(out)) == CAPY_HOST_OK);
  CAPY_TEST_CHECK(ctx, strcmp(out, "https://example.com/Path") == 0);
}

static void test_rejects_http(struct capy_test_ctx *ctx) {
  char out[CAPY_URL_MAX_LEN + 1];
  CAPY_TEST_CHECK(ctx, capy_host_prepare_url("http://example.com/", out,
                                             sizeof(out)) ==
                           CAPY_HOST_ERR_SCHEME);
}

static void test_rejects_ftp(struct capy_test_ctx *ctx) {
  char out[CAPY_URL_MAX_LEN + 1];
  /* Non-https is refused regardless of whether C1 parses the scheme. */
  CAPY_TEST_CHECK(ctx, capy_host_prepare_url("ftp://example.com/", out,
                                             sizeof(out)) != CAPY_HOST_OK);
}

static void test_rejects_space(struct capy_test_ctx *ctx) {
  char out[CAPY_URL_MAX_LEN + 1];
  CAPY_TEST_CHECK(ctx, capy_host_prepare_url("https://exa mple.com/", out,
                                             sizeof(out)) ==
                           CAPY_HOST_ERR_ARGS);
}

static void test_rejects_empty_host(struct capy_test_ctx *ctx) {
  char out[CAPY_URL_MAX_LEN + 1];
  /* "/path" gets https:// prepended -> empty host -> fail closed. */
  CAPY_TEST_CHECK(ctx, capy_host_prepare_url("/path", out, sizeof(out)) ==
                           CAPY_HOST_ERR_ARGS);
}

static void test_null_and_small_cap(struct capy_test_ctx *ctx) {
  char out[CAPY_URL_MAX_LEN + 1];
  char tiny[4];
  CAPY_TEST_CHECK(ctx, capy_host_prepare_url(NULL, out, sizeof(out)) ==
                           CAPY_HOST_ERR_ARGS);
  CAPY_TEST_CHECK(ctx, capy_host_prepare_url("https://example.com/", tiny,
                                             sizeof(tiny)) ==
                           CAPY_HOST_ERR_ARGS);
}

static void test_status_names(struct capy_test_ctx *ctx) {
  CAPY_TEST_CHECK(ctx, strcmp(capy_host_status_name(CAPY_HOST_OK), "OK") == 0);
  CAPY_TEST_CHECK(
      ctx, strcmp(capy_host_status_name(CAPY_HOST_ERR_SCHEME), "SCHEME") == 0);
  CAPY_TEST_CHECK(
      ctx, strcmp(capy_host_status_name(CAPY_HOST_ERR_DISABLED), "DISABLED") ==
               0);
}

int main(void) {
  struct capy_test_ctx ctx;
  ctx.checks = 0;
  ctx.failures = 0;
  test_assumes_https(&ctx);
  test_normalizes(&ctx);
  test_rejects_http(&ctx);
  test_rejects_ftp(&ctx);
  test_rejects_space(&ctx);
  test_rejects_empty_host(&ctx);
  test_null_and_small_cap(&ctx);
  test_status_names(&ctx);
  return capy_test_report(&ctx, "host");
}
