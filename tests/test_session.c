/*
 * Host-side test for the private/anonymous session base (Fase M4, part b).
 *
 * Assertion-based: exercises ephemeral/third-party flags, the minimal static
 * User-Agent and the Referer policy (private = none; normal same-origin = full
 * URL minus fragment; cross-origin = origin only; HTTPS->non-HTTPS = none).
 * Pure; no network, filesystem, clock or RNG.
 */

#include "capy_test.h"

#include "session.h"

#include <string.h>

static void test_private(struct capy_test_ctx *ctx) {
  struct capy_request_identity id;
  CAPY_TEST_CHECK(ctx, capy_request_identity(CAPY_SESSION_PRIVATE,
                                             "https://e.com/a", "https://e.com/b",
                                             &id) == CAPY_SESSION_OK);
  CAPY_TEST_CHECK(ctx, id.ephemeral == 1);
  CAPY_TEST_CHECK(ctx, id.allow_third_party == 0);
  CAPY_TEST_CHECK(ctx, id.send_referer == 0);
  CAPY_TEST_CHECK(ctx, id.referer[0] == '\0');
  CAPY_TEST_CHECK(ctx, strcmp(id.user_agent, "CapyBrowse") == 0);
}

static void test_normal_same_origin(struct capy_test_ctx *ctx) {
  struct capy_request_identity id;
  CAPY_TEST_CHECK(ctx, capy_request_identity(CAPY_SESSION_NORMAL,
                                             "https://e.com/a?x=1#frag",
                                             "https://e.com/b", &id) ==
                           CAPY_SESSION_OK);
  CAPY_TEST_CHECK(ctx, id.ephemeral == 0);
  CAPY_TEST_CHECK(ctx, id.allow_third_party == 1);
  CAPY_TEST_CHECK(ctx, id.send_referer == 1);
  CAPY_TEST_CHECK(ctx, strcmp(id.referer, "https://e.com/a?x=1") == 0);
}

static void test_normal_cross_origin(struct capy_test_ctx *ctx) {
  struct capy_request_identity id;
  CAPY_TEST_CHECK(ctx, capy_request_identity(CAPY_SESSION_NORMAL,
                                             "https://e.com/a/page?q=1",
                                             "https://other.com/x", &id) ==
                           CAPY_SESSION_OK);
  CAPY_TEST_CHECK(ctx, id.send_referer == 1);
  CAPY_TEST_CHECK(ctx, strcmp(id.referer, "https://e.com/") == 0);
}

static void test_normal_no_current(struct capy_test_ctx *ctx) {
  struct capy_request_identity id;
  CAPY_TEST_CHECK(ctx, capy_request_identity(CAPY_SESSION_NORMAL, NULL,
                                             "https://e.com/x", &id) ==
                           CAPY_SESSION_OK);
  CAPY_TEST_CHECK(ctx, id.send_referer == 0);
  CAPY_TEST_CHECK(ctx, id.referer[0] == '\0');
}

static void test_normal_downgrade(struct capy_test_ctx *ctx) {
  struct capy_request_identity id;
  CAPY_TEST_CHECK(ctx, capy_request_identity(CAPY_SESSION_NORMAL,
                                             "https://e.com/a", "http://e.com/b",
                                             &id) == CAPY_SESSION_OK);
  CAPY_TEST_CHECK(ctx, id.send_referer == 0);
  CAPY_TEST_CHECK(ctx, id.referer[0] == '\0');
}

static void test_ua_and_names(struct capy_test_ctx *ctx) {
  struct capy_request_identity id;
  CAPY_TEST_CHECK(ctx, capy_request_identity(CAPY_SESSION_NORMAL, NULL, NULL,
                                             &id) == CAPY_SESSION_OK);
  CAPY_TEST_CHECK(ctx, strcmp(id.user_agent, "CapyBrowse") == 0);
  CAPY_TEST_CHECK(ctx, strcmp(capy_session_mode_name(CAPY_SESSION_PRIVATE),
                              "private") == 0);
  CAPY_TEST_CHECK(ctx, strcmp(capy_session_mode_name(CAPY_SESSION_NORMAL),
                              "normal") == 0);
}

static void test_null_guard(struct capy_test_ctx *ctx) {
  CAPY_TEST_CHECK(ctx, capy_request_identity(CAPY_SESSION_NORMAL, "https://a/",
                                             "https://b/", NULL) ==
                           CAPY_SESSION_ERR_NULL);
}

int main(void) {
  struct capy_test_ctx ctx;
  ctx.checks = 0;
  ctx.failures = 0;
  test_private(&ctx);
  test_normal_same_origin(&ctx);
  test_normal_cross_origin(&ctx);
  test_normal_no_current(&ctx);
  test_normal_downgrade(&ctx);
  test_ua_and_names(&ctx);
  test_null_guard(&ctx);
  return capy_test_report(&ctx, "session");
}
