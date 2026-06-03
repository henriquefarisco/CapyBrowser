/*
 * Host-side test for the capy-browser-core URL surface (Fase C1).
 *
 * Fixture-driven: each case under tests/fixtures/url/ pairs an input with its
 * deterministic normalized output (and optional base + warning sequence). The
 * directory may be overridden via argv[1] so the test does not depend on the
 * current working directory.
 *
 * Convention (see tests/fixtures/README.md):
 *   <stem>.in    input bytes (trailing newline stripped)        [required]
 *   <stem>.base  absolute base URL, one line                    [optional]
 *   <stem>.out   expected normalized URL, OR "ERR:<NAME>"        [required]
 *   <stem>.warn  expected warning names, one per line           [optional]
 */

#include "capy_test.h"
#include "url_parse.h"

#include <stdio.h>
#include <string.h>

#define IO_MAX 8192u
#define WARN_LINES_MAX 16u

/* Every fixture stem, listed in a fixed (sorted) order for determinism. */
static const char *const g_cases[] = {
    "abs-combo-warnings",     "abs-default-port-https",
    "abs-dot-segments",       "abs-dot-segments-clamp",
    "abs-nondefault-port",    "abs-percent-case",
    "abs-percent-unreserved", "abs-simple",
    "abs-uppercase",          "err-base-missing",
    "err-control-tab",        "err-empty-host",
    "err-percent-bad",        "err-port-nondigit",
    "err-port-range",         "err-space",
    "nonascii-path",          "rel-absolute-path",
    "rel-dot-parent",         "rel-query-only",
};

static long read_file(const char *path, char *buf, size_t cap) {
  FILE *f = fopen(path, "rb");
  size_t n;
  if (!f) {
    return -1;
  }
  n = fread(buf, 1, cap - 1, f);
  fclose(f);
  buf[n] = '\0';
  return (long)n;
}

static void strip_eol(char *buf) {
  size_t n = strlen(buf);
  while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
    buf[--n] = '\0';
  }
}

static void make_path(char *dst, size_t cap, const char *dir, const char *stem,
                      const char *ext) {
  snprintf(dst, cap, "%s/%s%s", dir, stem, ext);
}

static size_t split_lines(char *buf, char *lines[], size_t maxlines) {
  size_t n = 0;
  char *p = buf;
  size_t i;
  if (*p == '\0') {
    return 0;
  }
  lines[n++] = p;
  while (*p != '\0') {
    if (*p == '\n' && n < maxlines) {
      *p = '\0';
      lines[n++] = p + 1;
    } else if (*p == '\n') {
      *p = '\0';
    }
    p++;
  }
  for (i = 0; i < n; i++) {
    size_t l = strlen(lines[i]);
    if (l > 0 && lines[i][l - 1] == '\r') {
      lines[i][l - 1] = '\0';
    }
  }
  return n;
}

static int err_code_from_name(const char *name) {
  if (strcmp(name, "NULL") == 0) {
    return CAPY_URL_ERR_NULL;
  }
  if (strcmp(name, "EMPTY") == 0) {
    return CAPY_URL_ERR_EMPTY;
  }
  if (strcmp(name, "TOO_LONG") == 0) {
    return CAPY_URL_ERR_TOO_LONG;
  }
  if (strcmp(name, "CONTROL") == 0) {
    return CAPY_URL_ERR_CONTROL;
  }
  if (strcmp(name, "SPACE") == 0) {
    return CAPY_URL_ERR_SPACE;
  }
  if (strcmp(name, "PERCENT") == 0) {
    return CAPY_URL_ERR_PERCENT;
  }
  if (strcmp(name, "SCHEME") == 0) {
    return CAPY_URL_ERR_SCHEME;
  }
  if (strcmp(name, "HOST") == 0) {
    return CAPY_URL_ERR_HOST;
  }
  if (strcmp(name, "PORT") == 0) {
    return CAPY_URL_ERR_PORT;
  }
  if (strcmp(name, "BASE") == 0) {
    return CAPY_URL_ERR_BASE;
  }
  if (strcmp(name, "OVERFLOW") == 0) {
    return CAPY_URL_ERR_OVERFLOW;
  }
  return 0;
}

static void check(struct capy_test_ctx *ctx, const char *stem, const char *what,
                  int ok) {
  ctx->checks++;
  if (!ok) {
    ctx->failures++;
    fprintf(stderr, "[FAIL] %s: %s\n", stem, what);
  }
}

static void run_case(struct capy_test_ctx *ctx, const char *dir,
                     const char *stem) {
  char in_buf[IO_MAX];
  char base_buf[IO_MAX];
  char out_buf[IO_MAX];
  char warn_buf[IO_MAX];
  char ser[IO_MAX];
  char path[1024];
  struct capy_url url;
  struct capy_url_warnings warns;
  const char *base_ptr = NULL;
  long n;
  int rc;

  make_path(path, sizeof(path), dir, stem, ".in");
  if (read_file(path, in_buf, sizeof(in_buf)) < 0) {
    check(ctx, stem, "missing .in fixture", 0);
    return;
  }
  strip_eol(in_buf);

  make_path(path, sizeof(path), dir, stem, ".out");
  if (read_file(path, out_buf, sizeof(out_buf)) < 0) {
    check(ctx, stem, "missing .out fixture", 0);
    return;
  }
  strip_eol(out_buf);

  make_path(path, sizeof(path), dir, stem, ".base");
  n = read_file(path, base_buf, sizeof(base_buf));
  if (n >= 0) {
    strip_eol(base_buf);
    base_ptr = base_buf;
  }

  rc = capy_url_parse(in_buf, base_ptr, &url, &warns);

  if (strncmp(out_buf, "ERR:", 4) == 0) {
    int want = err_code_from_name(out_buf + 4);
    int ok = (rc == want);
    check(ctx, stem, "expected rejection code", ok);
    if (!ok) {
      fprintf(stderr, "    got rc=%d want=%d (%s)\n", rc, want, out_buf);
    }
    return;
  }

  check(ctx, stem, "expected CAPY_URL_OK", rc == CAPY_URL_OK);
  if (rc != CAPY_URL_OK) {
    fprintf(stderr, "    got rc=%d, expected success for [%s]\n", rc, in_buf);
    return;
  }

  {
    int sl = capy_url_serialize(&url, ser, sizeof(ser));
    int ok = (sl >= 0) && (strcmp(ser, out_buf) == 0);
    check(ctx, stem, "serialized output matches .out", ok);
    if (!ok) {
      fprintf(stderr, "    got=[%s] want=[%s]\n", sl >= 0 ? ser : "<overflow>",
              out_buf);
    }
  }

  /* warning sequence */
  {
    char *lines[WARN_LINES_MAX];
    size_t want_n = 0;
    size_t i;
    make_path(path, sizeof(path), dir, stem, ".warn");
    if (read_file(path, warn_buf, sizeof(warn_buf)) >= 0) {
      strip_eol(warn_buf);
      want_n = split_lines(warn_buf, lines, WARN_LINES_MAX);
    }
    check(ctx, stem, "warning count matches .warn", warns.count == want_n);
    for (i = 0; i < warns.count && i < want_n; i++) {
      const char *got = capy_url_warning_name(warns.codes[i]);
      int ok = (strcmp(got, lines[i]) == 0);
      check(ctx, stem, "warning name matches .warn", ok);
      if (!ok) {
        fprintf(stderr, "    warn[%zu] got=%s want=%s\n", i, got, lines[i]);
      }
    }
  }

  /* idempotency: re-parsing the serialized form reproduces it exactly */
  {
    struct capy_url again;
    char ser2[IO_MAX];
    int rc2 = capy_url_parse(ser, NULL, &again, NULL);
    int ok = (rc2 == CAPY_URL_OK) &&
             (capy_url_serialize(&again, ser2, sizeof(ser2)) >= 0) &&
             (strcmp(ser, ser2) == 0);
    check(ctx, stem, "normalization is idempotent", ok);
    if (!ok) {
      fprintf(stderr, "    once=[%s] twice=[%s] rc2=%d\n", ser, ser2, rc2);
    }
  }
}

static void test_origin(struct capy_test_ctx *ctx) {
  struct capy_url a;
  struct capy_url b;
  struct capy_url_origin oa;
  struct capy_url_origin ob;

  /* default port and explicit default port share the same origin */
  CAPY_TEST_CHECK(ctx, capy_url_parse("https://example.com/x", NULL, &a,
                                      NULL) == CAPY_URL_OK);
  CAPY_TEST_CHECK(ctx, capy_url_parse("https://example.com:443/y", NULL, &b,
                                      NULL) == CAPY_URL_OK);
  CAPY_TEST_CHECK(ctx, capy_url_origin(&a, &oa) == CAPY_URL_OK);
  CAPY_TEST_CHECK(ctx, capy_url_origin(&b, &ob) == CAPY_URL_OK);
  CAPY_TEST_CHECK_EQ_UINT(ctx, oa.port, 443u);
  CAPY_TEST_CHECK(ctx, capy_url_origin_equal(&oa, &ob) == 1);

  /* different scheme -> different origin */
  CAPY_TEST_CHECK(ctx,
                  capy_url_parse("http://example.com/", NULL, &b, NULL) ==
                      CAPY_URL_OK);
  CAPY_TEST_CHECK(ctx, capy_url_origin(&b, &ob) == CAPY_URL_OK);
  CAPY_TEST_CHECK_EQ_UINT(ctx, ob.port, 80u);
  CAPY_TEST_CHECK(ctx, capy_url_origin_equal(&oa, &ob) == 0);

  /* different host -> different origin */
  CAPY_TEST_CHECK(ctx,
                  capy_url_parse("https://other.example/", NULL, &b, NULL) ==
                      CAPY_URL_OK);
  CAPY_TEST_CHECK(ctx, capy_url_origin(&b, &ob) == CAPY_URL_OK);
  CAPY_TEST_CHECK(ctx, capy_url_origin_equal(&oa, &ob) == 0);
}

static void test_null_guards(struct capy_test_ctx *ctx) {
  struct capy_url url;
  CAPY_TEST_CHECK(ctx, capy_url_parse(NULL, NULL, &url, NULL) ==
                           CAPY_URL_ERR_NULL);
  CAPY_TEST_CHECK(ctx, capy_url_parse("https://h/", NULL, NULL, NULL) ==
                           CAPY_URL_ERR_NULL);
}

int main(int argc, char **argv) {
  struct capy_test_ctx ctx;
  const char *dir = (argc > 1) ? argv[1] : "tests/fixtures/url";
  size_t i;

  ctx.checks = 0;
  ctx.failures = 0;

  for (i = 0; i < sizeof(g_cases) / sizeof(g_cases[0]); i++) {
    run_case(&ctx, dir, g_cases[i]);
  }
  test_origin(&ctx);
  test_null_guards(&ctx);

  return capy_test_report(&ctx, "url");
}
