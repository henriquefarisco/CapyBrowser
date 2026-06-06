/*
 * Host-side test for the CSS cascade (Fase M2, part 2).
 *
 * Fixture-driven: each case pairs <stem>.html and <stem>.css; the resulting
 * per-element computed style is dumped to a deterministic indented text and
 * compared against the golden <stem>.out. Fixtures live unde
 * tests/fixtures/cascade/ (override dir via argv[1]).
 *
 * Dump format (elements only, two spaces per depth level):
 *   <tag> { prop: value; prop: value }
 * with set properties in enum order; an element with no computed style is
 * `<tag> { }`.
 */

#include "capy_test.h"

#include "cascade.h"
#include "css_parse.h"
#include "dom.h"

#include <stdio.h>
#include <string.h>

#define IO_MAX 65536u

static const char *const g_cases[] = {
    "inherit",
    "specificity",
};

static char g_html[IO_MAX];
static char g_css[IO_MAX];
static char g_out[IO_MAX];
static char g_dump[IO_MAX];
static struct capy_dom_doc g_dom;
static struct capy_css_stylesheet g_sheet;
static struct capy_css_cascade g_casc;

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

static void dump_node(size_t idx, size_t depth) {
  const struct capy_dom_node *node = &g_dom.nodes[idx];
  size_t child;
  size_t child_depth = depth;

  if (node->type == CAPY_DOM_ELEMENT) {
    const struct capy_css_computed *cs = &g_casc.styles[idx];
    size_t d;
    size_t p;
    int first = 1;
    for (d = 0; d < depth; d++) {
      dput("  ");
    }
    dput(node->name);
    dput(" {");
    for (p = 0; p < CAPY_CSS_PROP_COUNT; p++) {
      if (cs->set[p]) {
        dput(first ? " " : "; ");
        first = 0;
        dput(capy_css_prop_name((enum capy_css_prop)p));
        dput(": ");
        dput_bytes(g_sheet.strings + cs->value_off[p], cs->value_len[p]);
      }
    }
    dput(" }");
    dput("\n");
    child_depth = depth + 1;
  }
  child = node->first_child;
  while (child != CAPY_DOM_NONE) {
    dump_node(child, child_depth);
    child = g_dom.nodes[child].next_sibling;
  }
}

static void dump_tree(void) {
  size_t child;
  g_len = 0;
  child = g_dom.nodes[g_dom.root].first_child;
  while (child != CAPY_DOM_NONE) {
    dump_node(child, 0);
    child = g_dom.nodes[child].next_sibling;
  }
  g_dump[g_len] = '\0';
}

static void run_case(struct capy_test_ctx *ctx, const char *dir,
                     const char *stem) {
  char path[1024];

  make_path(path, sizeof(path), dir, stem, ".html");
  if (read_file(path, g_html, sizeof(g_html)) < 0) {
    check(ctx, stem, "missing .html fixture", 0);
    return;
  }
  strip_eol(g_html);

  make_path(path, sizeof(path), dir, stem, ".css");
  if (read_file(path, g_css, sizeof(g_css)) < 0) {
    check(ctx, stem, "missing .css fixture", 0);
    return;
  }
  strip_eol(g_css);

  if (capy_html_parse(g_html, strlen(g_html), &g_dom) != CAPY_DOM_OK) {
    check(ctx, stem, "capy_html_parse returned error", 0);
    return;
  }
  if (capy_css_parse(g_css, strlen(g_css), &g_sheet) != CAPY_CSS_OK) {
    check(ctx, stem, "capy_css_parse returned error", 0);
    return;
  }
  if (capy_css_cascade(&g_dom, &g_sheet, &g_casc) != CAPY_CSS_CASCADE_OK) {
    check(ctx, stem, "capy_css_cascade returned error", 0);
    return;
  }

  dump_tree();
  strip_eol(g_dump);

  make_path(path, sizeof(path), dir, stem, ".out");
  if (read_file(path, g_out, sizeof(g_out)) < 0) {
    g_out[0] = '\0';
  } else {
    strip_eol(g_out);
  }
  {
    int ok = (strcmp(g_dump, g_out) == 0);
    check(ctx, stem, "computed style dump matches .out", ok);
    if (!ok) {
      fprintf(stderr, "    --- got ---\n%s\n    --- want ---\n%s\n", g_dump,
              g_out);
    }
  }
}

static void test_null_guards(struct capy_test_ctx *ctx) {
  CAPY_TEST_CHECK(ctx, capy_css_cascade(NULL, &g_sheet, &g_casc) ==
                           CAPY_CSS_CASCADE_ERR_NULL);
  CAPY_TEST_CHECK(ctx, capy_css_cascade(&g_dom, NULL, &g_casc) ==
                           CAPY_CSS_CASCADE_ERR_NULL);
  CAPY_TEST_CHECK(ctx, capy_css_cascade(&g_dom, &g_sheet, NULL) ==
                           CAPY_CSS_CASCADE_ERR_NULL);
}

int main(int argc, char **argv) {
  struct capy_test_ctx ctx;
  const char *dir = (argc > 1) ? argv[1] : "tests/fixtures/cascade";
  size_t i;

  ctx.checks = 0;
  ctx.failures = 0;

  for (i = 0; i < sizeof(g_cases) / sizeof(g_cases[0]); i++) {
    run_case(&ctx, dir, g_cases[i]);
  }
  test_null_guards(&ctx);

  return capy_test_report(&ctx, "cascade");
}
