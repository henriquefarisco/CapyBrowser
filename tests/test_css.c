/*
 * Host-side test for the tolerant CSS parser (Fase M2, part 1).
 *
 * Fixture-driven: each case's stylesheet is dumped to a deterministic text and
 * compared against the golden .out; warnings against .warn. Fixtures live unde
 * tests/fixtures/css/ (override dir via argv[1]).
 *
 * Dump format, one rule per line:
 *   <selector> { prop: value; prop: value }
 * where <selector> is `*`, `tag`, `.class` or `#id`; an empty block is `{ }`.
 */

#include "capy_test.h"
#include "css_parse.h"

#include <stdio.h>
#include <string.h>

#define IO_MAX 65536u
#define LINES_MAX 32u

static const char *const g_cases[] = {
    "recovery",
    "selectors",
    "skips",
};

static char g_in[IO_MAX];
static char g_out[IO_MAX];
static char g_aux[IO_MAX];
static char g_dump[IO_MAX];
static struct capy_css_stylesheet g_sheet;

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

static size_t g_len;

static void dput(const char *s) {
  while (*s != '\0' && g_len + 1 < sizeof(g_dump)) {
    g_dump[g_len++] = *s++;
  }
}

static void dput_bytes(const char *s, size_t n) {
  size_t i;
  for (i = 0; i < n; i++) {
    if (g_len + 1 < sizeof(g_dump)) {
      g_dump[g_len++] = s[i];
    }
  }
}

static void dump_sheet(const struct capy_css_stylesheet *sheet) {
  size_t i;
  g_len = 0;
  for (i = 0; i < sheet->rule_count; i++) {
    const struct capy_css_rule *r = &sheet->rules[i];
    size_t j;
    switch (r->selector.kind) {
      case CAPY_CSS_SEL_UNIVERSAL:
        dput("*");
        break;
      case CAPY_CSS_SEL_CLASS:
        dput(".");
        dput_bytes(sheet->strings + r->selector.name_off,
                   r->selector.name_len);
        break;
      case CAPY_CSS_SEL_ID:
        dput("#");
        dput_bytes(sheet->strings + r->selector.name_off,
                   r->selector.name_len);
        break;
      case CAPY_CSS_SEL_TYPE:
      default:
        dput_bytes(sheet->strings + r->selector.name_off,
                   r->selector.name_len);
        break;
    }
    dput(" {");
    for (j = 0; j < r->decl_count; j++) {
      const struct capy_css_decl *d = &sheet->decls[r->decl_start + j];
      dput(j == 0 ? " " : "; ");
      dput_bytes(sheet->strings + d->prop_off, d->prop_len);
      dput(": ");
      dput_bytes(sheet->strings + d->value_off, d->value_len);
    }
    dput(" }");
    dput("\n");
  }
  g_dump[g_len] = '\0';
}

static void run_case(struct capy_test_ctx *ctx, const char *dir,
                     const char *stem) {
  char path[1024];
  size_t i;

  make_path(path, sizeof(path), dir, stem, ".in");
  if (read_file(path, g_in, sizeof(g_in)) < 0) {
    check(ctx, stem, "missing .in fixture", 0);
    return;
  }
  strip_eol(g_in);

  if (capy_css_parse(g_in, strlen(g_in), &g_sheet) != CAPY_CSS_OK) {
    check(ctx, stem, "capy_css_parse returned error", 0);
    return;
  }

  dump_sheet(&g_sheet);
  strip_eol(g_dump);

  make_path(path, sizeof(path), dir, stem, ".out");
  if (read_file(path, g_out, sizeof(g_out)) < 0) {
    g_out[0] = '\0';
  } else {
    strip_eol(g_out);
  }
  {
    int ok = (strcmp(g_dump, g_out) == 0);
    check(ctx, stem, "dump matches .out", ok);
    if (!ok) {
      fprintf(stderr, "    --- got ---\n%s\n    --- want ---\n%s\n", g_dump,
              g_out);
    }
  }

  make_path(path, sizeof(path), dir, stem, ".warn");
  if (read_file(path, g_aux, sizeof(g_aux)) >= 0) {
    char *lines[LINES_MAX];
    size_t want;
    strip_eol(g_aux);
    want = split_lines(g_aux, lines, LINES_MAX);
    check(ctx, stem, "warning count matches .warn",
          g_sheet.warnings.count == want);
    for (i = 0; i < g_sheet.warnings.count && i < want; i++) {
      const char *got = capy_css_warning_name(g_sheet.warnings.codes[i]);
      int ok = (strcmp(got, lines[i]) == 0);
      check(ctx, stem, "warning name matches .warn", ok);
      if (!ok) {
        fprintf(stderr, "    warn[%zu] got=%s want=%s\n", i, got, lines[i]);
      }
    }
  } else {
    check(ctx, stem, "no warnings expected", g_sheet.warnings.count == 0);
  }
}

static void test_null_guards(struct capy_test_ctx *ctx) {
  CAPY_TEST_CHECK(ctx, capy_css_parse(NULL, 0, &g_sheet) == CAPY_CSS_ERR_NULL);
  CAPY_TEST_CHECK(ctx, capy_css_parse("a{}", 3, NULL) == CAPY_CSS_ERR_NULL);
}

int main(int argc, char **argv) {
  struct capy_test_ctx ctx;
  const char *dir = (argc > 1) ? argv[1] : "tests/fixtures/css";
  size_t i;

  ctx.checks = 0;
  ctx.failures = 0;

  for (i = 0; i < sizeof(g_cases) / sizeof(g_cases[0]); i++) {
    run_case(&ctx, dir, g_cases[i]);
  }
  test_null_guards(&ctx);

  return capy_test_report(&ctx, "css");
}
