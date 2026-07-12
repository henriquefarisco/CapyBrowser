/*
 * Host-side test for download preparation (Fase M4, part a).
 *
 * Assertion-based (the inputs are header/argument values, not files): exercises
 * HTTPS-first, URL validation, Content-Disposition filename / filename*,
 * basename + traversal sanitization, the size budget and the fallback name.
 * Pure; no network, filesystem, clock or RNG.
 */

#include "capy_test.h"

#include "download.h"

#include <string.h>

static void test_accept_content_disposition(struct capy_test_ctx *ctx) {
  struct capy_download d;
  CAPY_TEST_CHECK(ctx, capy_download_prepare(
                           "https://e.com/a/x", NULL,
                           "attachment; filename=\"report.pdf\"", -1, 0,
                           &d) == CAPY_DOWNLOAD_OK);
  CAPY_TEST_CHECK(ctx, d.verdict == CAPY_DOWNLOAD_ACCEPT);
  CAPY_TEST_CHECK(ctx, strcmp(d.filename, "report.pdf") == 0);
  CAPY_TEST_CHECK(ctx, strcmp(d.url, "https://e.com/a/x") == 0);
}

static void test_reject_scheme(struct capy_test_ctx *ctx) {
  struct capy_download d;
  CAPY_TEST_CHECK(ctx, capy_download_prepare("http://e.com/x", NULL, NULL, -1, 0,
                                             &d) == CAPY_DOWNLOAD_OK);
  CAPY_TEST_CHECK(ctx, d.verdict == CAPY_DOWNLOAD_REJECT_SCHEME);
}

static void test_reject_url(struct capy_test_ctx *ctx) {
  struct capy_download d;
  /* a raw space is rejected by the URL core */
  CAPY_TEST_CHECK(ctx, capy_download_prepare("https://e.com/a b", NULL, NULL, -1,
                                             0, &d) == CAPY_DOWNLOAD_OK);
  CAPY_TEST_CHECK(ctx, d.verdict == CAPY_DOWNLOAD_REJECT_URL);
}

static void test_reject_too_large(struct capy_test_ctx *ctx) {
  struct capy_download d;
  CAPY_TEST_CHECK(ctx, capy_download_prepare("https://e.com/x", NULL, NULL, 2000,
                                             1000, &d) == CAPY_DOWNLOAD_OK);
  CAPY_TEST_CHECK(ctx, d.verdict == CAPY_DOWNLOAD_REJECT_TOO_LARGE);
}

static void test_traversal_basename(struct capy_test_ctx *ctx) {
  struct capy_download d;
  /* a path-traversal name is reduced to its basename, not rejected */
  CAPY_TEST_CHECK(ctx, capy_download_prepare(
                           "https://e.com/x", NULL,
                           "attachment; filename=\"../etc/passwd\"", -1, 0,
                           &d) == CAPY_DOWNLOAD_OK);
  CAPY_TEST_CHECK(ctx, d.verdict == CAPY_DOWNLOAD_ACCEPT);
  CAPY_TEST_CHECK(ctx, strcmp(d.filename, "passwd") == 0);
}

static void test_reject_dotdot_filename(struct capy_test_ctx *ctx) {
  struct capy_download d;
  CAPY_TEST_CHECK(ctx, capy_download_prepare("https://e.com/x", NULL,
                                             "filename=\"..\"", -1, 0,
                                             &d) == CAPY_DOWNLOAD_OK);
  CAPY_TEST_CHECK(ctx, d.verdict == CAPY_DOWNLOAD_REJECT_FILENAME);
}

static void test_url_derived(struct capy_test_ctx *ctx) {
  struct capy_download d;
  CAPY_TEST_CHECK(ctx, capy_download_prepare("https://e.com/a/file.zip", NULL,
                                             NULL, -1, 0, &d) ==
                           CAPY_DOWNLOAD_OK);
  CAPY_TEST_CHECK(ctx, d.verdict == CAPY_DOWNLOAD_ACCEPT);
  CAPY_TEST_CHECK(ctx, strcmp(d.filename, "file.zip") == 0);
}

static void test_url_fallback(struct capy_test_ctx *ctx) {
  struct capy_download d;
  CAPY_TEST_CHECK(ctx, capy_download_prepare("https://e.com/", NULL, NULL, -1, 0,
                                             &d) == CAPY_DOWNLOAD_OK);
  CAPY_TEST_CHECK(ctx, d.verdict == CAPY_DOWNLOAD_ACCEPT);
  CAPY_TEST_CHECK(ctx, strcmp(d.filename, "download") == 0);
}

static void test_filename_star_pct(struct capy_test_ctx *ctx) {
  struct capy_download d;
  CAPY_TEST_CHECK(ctx, capy_download_prepare(
                           "https://e.com/x", NULL,
                           "attachment; filename*=UTF-8''a%20b.txt", -1, 0,
                           &d) == CAPY_DOWNLOAD_OK);
  CAPY_TEST_CHECK(ctx, d.verdict == CAPY_DOWNLOAD_ACCEPT);
  CAPY_TEST_CHECK(ctx, strcmp(d.filename, "a b.txt") == 0);
}

static void test_ignore_lookalike_filename_params(struct capy_test_ctx *ctx) {
  struct capy_download d;
  CAPY_TEST_CHECK(ctx, capy_download_prepare(
                           "https://e.com/safe.bin", NULL,
                           "attachment; notfilename=attacker.exe; "
                           "xfilename*=UTF-8''attacker2.exe",
                           -1, 0, &d) == CAPY_DOWNLOAD_OK);
  CAPY_TEST_CHECK(ctx, d.verdict == CAPY_DOWNLOAD_ACCEPT);
  CAPY_TEST_CHECK(ctx, strcmp(d.filename, "safe.bin") == 0);

  CAPY_TEST_CHECK(ctx, capy_download_prepare(
                           "https://e.com/safe.bin", NULL,
                           "attachment; note=\"x; filename=attacker.exe\"",
                           -1, 0, &d) == CAPY_DOWNLOAD_OK);
  CAPY_TEST_CHECK(ctx, d.verdict == CAPY_DOWNLOAD_ACCEPT);
  CAPY_TEST_CHECK(ctx, strcmp(d.filename, "safe.bin") == 0);
}

static void test_relative_resolution(struct capy_test_ctx *ctx) {
  struct capy_download d;
  CAPY_TEST_CHECK(ctx, capy_download_prepare("file.zip", "https://e.com/a/",
                                             NULL, -1, 0, &d) ==
                           CAPY_DOWNLOAD_OK);
  CAPY_TEST_CHECK(ctx, d.verdict == CAPY_DOWNLOAD_ACCEPT);
  CAPY_TEST_CHECK(ctx, strcmp(d.url, "https://e.com/a/file.zip") == 0);
  CAPY_TEST_CHECK(ctx, strcmp(d.filename, "file.zip") == 0);
}

static void test_null_guards(struct capy_test_ctx *ctx) {
  struct capy_download d;
  CAPY_TEST_CHECK(ctx, capy_download_prepare(NULL, NULL, NULL, -1, 0, &d) ==
                           CAPY_DOWNLOAD_ERR_NULL);
  CAPY_TEST_CHECK(ctx, capy_download_prepare("https://e.com/x", NULL, NULL, -1,
                                             0, NULL) ==
                           CAPY_DOWNLOAD_ERR_NULL);
}

static void test_verdict_names(struct capy_test_ctx *ctx) {
  CAPY_TEST_CHECK(ctx, strcmp(capy_download_verdict_name(CAPY_DOWNLOAD_ACCEPT),
                              "ACCEPT") == 0);
  CAPY_TEST_CHECK(ctx,
                  strcmp(capy_download_verdict_name(CAPY_DOWNLOAD_REJECT_SCHEME),
                         "REJECT_SCHEME") == 0);
}

int main(void) {
  struct capy_test_ctx ctx;
  ctx.checks = 0;
  ctx.failures = 0;
  test_accept_content_disposition(&ctx);
  test_reject_scheme(&ctx);
  test_reject_url(&ctx);
  test_reject_too_large(&ctx);
  test_traversal_basename(&ctx);
  test_reject_dotdot_filename(&ctx);
  test_url_derived(&ctx);
  test_url_fallback(&ctx);
  test_filename_star_pct(&ctx);
  test_ignore_lookalike_filename_params(&ctx);
  test_relative_resolution(&ctx);
  test_null_guards(&ctx);
  test_verdict_names(&ctx);
  return capy_test_report(&ctx, "download");
}
