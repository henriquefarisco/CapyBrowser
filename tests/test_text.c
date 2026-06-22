/*
 * Host-side test for the CapyBrowse Text (HTML-to-text) surface (Fase C2).
 *
 * Fixture-driven (see tests/fixtures/README.md). Per case <stem>:
 *   <stem>.in     HTML bytes                                   [required]
 *   <stem>.base   absolute base URL for link resolution        [optional]
 *   <stem>.out    expected normalized body (inline "[n]")      [optional: ""]
 *   <stem>.title  expected document title                      [optional: none]
 *   <stem>.links  expected resolved link URLs, one per line    [optional: none]
 *   <stem>.warn   expected warning names, one per line         [optional: none]
 *
 * The fixtures directory may be overridden via argv[1].
 */

#include "capy_test.h"
#include "html_text.h"

#include <stdio.h>
#include <string.h>

#define IO_MAX 65536u
#define LINES_MAX 80u

static const char *const g_cases[] = {
    "basic",
    "br-linebreak",
    "comment-skip",
    "entities",
    "entities-latin1",
    "entities-latin1-symbols",
    "link-unresolved",
    "links",
    "list",
    "list-ordered-nested",
    "malformed-unclosed-tag",
    "numeric-entity-c1-unmapped",
    "numeric-entity-invalid",
    "numeric-entity-replacement",
    "numeric-entity-win1252",
    "only-script",
    "pre-preformatted",
    "title-only",
    "unclosed-comment",
    "whitespace-collapse",
};

static char g_in[IO_MAX];
static char g_out[IO_MAX];
static char g_aux[IO_MAX];
static char g_base[IO_MAX];
static char g_body[IO_MAX];
static struct capy_text_doc g_doc;

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
    if (*p == '\n') {
      *p = '\0';
      if (n < maxlines) {
        lines[n++] = p + 1;
      }
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
  char path[1024];
  const char *base_ptr = NULL;
  size_t html_len;
  size_t i;

  make_path(path, sizeof(path), dir, stem, ".in");
  if (read_file(path, g_in, sizeof(g_in)) < 0) {
    check(ctx, stem, "missing .in fixture", 0);
    return;
  }
  strip_eol(g_in);
  html_len = strlen(g_in);

  make_path(path, sizeof(path), dir, stem, ".base");
  if (read_file(path, g_base, sizeof(g_base)) >= 0) {
    strip_eol(g_base);
    base_ptr = g_base;
  }

  if (capy_html_to_text((const uint8_t *)g_in, html_len, base_ptr, g_body,
                        sizeof(g_body), &g_doc) != CAPY_TEXT_OK) {
    check(ctx, stem, "capy_html_to_text returned error", 0);
    return;
  }

  /* body */
  make_path(path, sizeof(path), dir, stem, ".out");
  if (read_file(path, g_out, sizeof(g_out)) < 0) {
    g_out[0] = '\0';
  } else {
    strip_eol(g_out);
  }
  {
    int ok = (strcmp(g_body, g_out) == 0);
    check(ctx, stem, "body matches .out", ok);
    if (!ok) {
      fprintf(stderr, "    got=[%s]\n   want=[%s]\n", g_body, g_out);
    }
  }

  /* title */
  make_path(path, sizeof(path), dir, stem, ".title");
  if (read_file(path, g_out, sizeof(g_out)) >= 0) {
    strip_eol(g_out);
    check(ctx, stem, "has_title expected", g_doc.has_title == 1);
    check(ctx, stem, "title matches .title", strcmp(g_doc.title, g_out) == 0);
    if (strcmp(g_doc.title, g_out) != 0) {
      fprintf(stderr, "    title got=[%s] want=[%s]\n", g_doc.title, g_out);
    }
  } else {
    check(ctx, stem, "no title expected", g_doc.has_title == 0);
  }

  /* links */
  make_path(path, sizeof(path), dir, stem, ".links");
  if (read_file(path, g_aux, sizeof(g_aux)) >= 0) {
    char *lines[LINES_MAX];
    size_t want;
    strip_eol(g_aux);
    want = split_lines(g_aux, lines, LINES_MAX);
    check(ctx, stem, "link_count matches .links", g_doc.link_count == want);
    for (i = 0; i < g_doc.link_count && i < want; i++) {
      int ok = (strcmp(g_doc.links[i].url, lines[i]) == 0);
      check(ctx, stem, "link url matches .links", ok);
      if (!ok) {
        fprintf(stderr, "    link[%zu] got=[%s] want=[%s]\n", i,
                g_doc.links[i].url, lines[i]);
      }
    }
  } else {
    check(ctx, stem, "no links expected", g_doc.link_count == 0);
  }

  /* warnings */
  make_path(path, sizeof(path), dir, stem, ".warn");
  if (read_file(path, g_aux, sizeof(g_aux)) >= 0) {
    char *lines[LINES_MAX];
    size_t want;
    strip_eol(g_aux);
    want = split_lines(g_aux, lines, LINES_MAX);
    check(ctx, stem, "warning count matches .warn", g_doc.warnings.count == want);
    for (i = 0; i < g_doc.warnings.count && i < want; i++) {
      const char *got = capy_text_warning_name(g_doc.warnings.codes[i]);
      int ok = (strcmp(got, lines[i]) == 0);
      check(ctx, stem, "warning name matches .warn", ok);
      if (!ok) {
        fprintf(stderr, "    warn[%zu] got=%s want=%s\n", i, got, lines[i]);
      }
    }
  } else {
    check(ctx, stem, "no warnings expected", g_doc.warnings.count == 0);
  }
}

static void test_null_guards(struct capy_test_ctx *ctx) {
  char buf[8];
  CAPY_TEST_CHECK(ctx, capy_html_to_text(NULL, 0, NULL, buf, sizeof(buf),
                                         &g_doc) == CAPY_TEXT_ERR_NULL);
  CAPY_TEST_CHECK(ctx, capy_html_to_text((const uint8_t *)"<p>x</p>", 8, NULL,
                                         buf, sizeof(buf), NULL) ==
                           CAPY_TEXT_ERR_NULL);
}

int main(int argc, char **argv) {
  struct capy_test_ctx ctx;
  const char *dir = (argc > 1) ? argv[1] : "tests/fixtures/html-to-text";
  size_t i;

  ctx.checks = 0;
  ctx.failures = 0;

  for (i = 0; i < sizeof(g_cases) / sizeof(g_cases[0]); i++) {
    run_case(&ctx, dir, g_cases[i]);
  }
  test_null_guards(&ctx);

  return capy_test_report(&ctx, "html-to-text");
}
