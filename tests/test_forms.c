/*
 * Host-side test for static form submission (Fase M4, part c).
 *
 * Assertion-based: exercises application/x-www-form-urlencoded encoding (space
 * -> '+', reserved -> %XX), GET (query on the action, replacing any existing
 * query), POST (body + content type), relative-action resolution, HTTPS-first
 * and URL validation. Pure; no network, filesystem, clock or RNG.
 */

#include "capy_test.h"

#include "forms.h"

#include <string.h>

static void test_get_single(struct capy_test_ctx *ctx) {
  struct capy_form_field f[1];
  struct capy_form_request r;
  f[0].name = "q";
  f[0].value = "hello world";
  CAPY_TEST_CHECK(ctx, capy_form_submit(CAPY_FORM_GET, "https://e.com/s", NULL,
                                        f, 1, &r) == CAPY_FORM_OK);
  CAPY_TEST_CHECK(ctx, r.method == CAPY_FORM_GET);
  CAPY_TEST_CHECK(ctx, strcmp(r.url, "https://e.com/s?q=hello+world") == 0);
  CAPY_TEST_CHECK(ctx, r.body_len == 0);
  CAPY_TEST_CHECK(ctx, r.content_type[0] == '\0');
}

static void test_get_special(struct capy_test_ctx *ctx) {
  struct capy_form_field f[2];
  struct capy_form_request r;
  f[0].name = "a";
  f[0].value = "x&y";
  f[1].name = "b";
  f[1].value = "c=d";
  CAPY_TEST_CHECK(ctx, capy_form_submit(CAPY_FORM_GET, "https://e.com/s", NULL,
                                        f, 2, &r) == CAPY_FORM_OK);
  CAPY_TEST_CHECK(ctx, strcmp(r.url, "https://e.com/s?a=x%26y&b=c%3Dd") == 0);
}

static void test_post(struct capy_test_ctx *ctx) {
  struct capy_form_field f[2];
  struct capy_form_request r;
  f[0].name = "user";
  f[0].value = "bob";
  f[1].name = "pw";
  f[1].value = "p@ss";
  CAPY_TEST_CHECK(ctx, capy_form_submit(CAPY_FORM_POST, "https://e.com/s", NULL,
                                        f, 2, &r) == CAPY_FORM_OK);
  CAPY_TEST_CHECK(ctx, r.method == CAPY_FORM_POST);
  CAPY_TEST_CHECK(ctx, strcmp(r.url, "https://e.com/s") == 0);
  CAPY_TEST_CHECK(ctx, strcmp(r.body, "user=bob&pw=p%40ss") == 0);
  CAPY_TEST_CHECK(ctx, r.body_len == strlen("user=bob&pw=p%40ss"));
  CAPY_TEST_CHECK(ctx, strcmp(r.content_type,
                              "application/x-www-form-urlencoded") == 0);
}

static void test_get_replaces_query(struct capy_test_ctx *ctx) {
  struct capy_form_field f[1];
  struct capy_form_request r;
  f[0].name = "q";
  f[0].value = "z";
  CAPY_TEST_CHECK(ctx, capy_form_submit(CAPY_FORM_GET, "https://e.com/s?old=1",
                                        NULL, f, 1, &r) == CAPY_FORM_OK);
  CAPY_TEST_CHECK(ctx, strcmp(r.url, "https://e.com/s?q=z") == 0);
}

static void test_relative_action(struct capy_test_ctx *ctx) {
  struct capy_form_field f[1];
  struct capy_form_request r;
  f[0].name = "q";
  f[0].value = "1";
  CAPY_TEST_CHECK(ctx, capy_form_submit(CAPY_FORM_GET, "submit",
                                        "https://e.com/a/", f, 1, &r) ==
                           CAPY_FORM_OK);
  CAPY_TEST_CHECK(ctx, strcmp(r.url, "https://e.com/a/submit?q=1") == 0);
}

static void test_reject_scheme(struct capy_test_ctx *ctx) {
  struct capy_form_field f[1];
  struct capy_form_request r;
  f[0].name = "q";
  f[0].value = "1";
  CAPY_TEST_CHECK(ctx, capy_form_submit(CAPY_FORM_GET, "http://e.com/s", NULL, f,
                                        1, &r) == CAPY_FORM_ERR_SCHEME);
}

static void test_reject_url(struct capy_test_ctx *ctx) {
  struct capy_form_field f[1];
  struct capy_form_request r;
  f[0].name = "q";
  f[0].value = "1";
  CAPY_TEST_CHECK(ctx, capy_form_submit(CAPY_FORM_GET, "https://e.com/a b", NULL,
                                        f, 1, &r) == CAPY_FORM_ERR_URL);
}

static void test_reject_method_and_reset(struct capy_test_ctx *ctx) {
  struct capy_form_request r;
  strcpy(r.url, "https://stale.example/");
  strcpy(r.body, "secret=stale");
  r.body_len = strlen(r.body);
  r.content_type = "stale/type";
  CAPY_TEST_CHECK(ctx,
                  capy_form_submit((enum capy_form_method)99,
                                   "https://e.com/s", NULL, NULL, 0, &r) ==
                      CAPY_FORM_ERR_METHOD);
  CAPY_TEST_CHECK(ctx, r.url[0] == '\0');
  CAPY_TEST_CHECK(ctx, r.body[0] == '\0');
  CAPY_TEST_CHECK(ctx, r.body_len == 0);
  CAPY_TEST_CHECK(ctx, r.content_type[0] == '\0');
  CAPY_TEST_CHECK(ctx,
                  strcmp(capy_form_method_name((enum capy_form_method)99),
                         "UNKNOWN") == 0);

  strcpy(r.url, "https://stale.example/");
  strcpy(r.body, "secret=stale");
  r.body_len = strlen(r.body);
  r.content_type = "stale/type";
  CAPY_TEST_CHECK(ctx, capy_form_submit(CAPY_FORM_POST, NULL, NULL, NULL, 0,
                                        &r) == CAPY_FORM_ERR_NULL);
  CAPY_TEST_CHECK(ctx, r.url[0] == '\0');
  CAPY_TEST_CHECK(ctx, r.body[0] == '\0');
  CAPY_TEST_CHECK(ctx, r.body_len == 0);
  CAPY_TEST_CHECK(ctx, r.content_type[0] == '\0');
}

static void test_null_and_names(struct capy_test_ctx *ctx) {
  struct capy_form_request r;
  CAPY_TEST_CHECK(ctx, capy_form_submit(CAPY_FORM_GET, NULL, NULL, NULL, 0, &r) ==
                           CAPY_FORM_ERR_NULL);
  CAPY_TEST_CHECK(ctx, capy_form_submit(CAPY_FORM_GET, "https://e.com/", NULL,
                                        NULL, 0, NULL) == CAPY_FORM_ERR_NULL);
  CAPY_TEST_CHECK(ctx, strcmp(capy_form_method_name(CAPY_FORM_POST), "POST") ==
                           0);
}

int main(void) {
  struct capy_test_ctx ctx;
  ctx.checks = 0;
  ctx.failures = 0;
  test_get_single(&ctx);
  test_get_special(&ctx);
  test_post(&ctx);
  test_get_replaces_query(&ctx);
  test_relative_action(&ctx);
  test_reject_scheme(&ctx);
  test_reject_url(&ctx);
  test_reject_method_and_reset(&ctx);
  test_null_and_names(&ctx);
  return capy_test_report(&ctx, "forms");
}
