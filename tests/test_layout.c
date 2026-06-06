/*
 * Host-side test for the static block layout (Fase M3, part a).
 *
 * Fixture-driven: each case pairs <stem>.html and <stem>.css and an optional
 * <stem>.width (viewport columns, default 80); the resulting box tree is dumped
 * to a deterministic indented text and compared against the golden <stem>.out.
 * Fixtures live under tests/fixtures/layout/ (override dir via argv[1]).
 *
 * Dump format (two spaces per depth level):
 *   <label> @<x>,<y> <w>x<h>
 * where <label> is `#root` (viewport), an element tag, or `#text`.
 */

#include "capy_test.h"

#include "cascade.h"
#include "css_parse.h"
#include "dom.h"
#include "layout.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IO_MAX 65536u

static const char *const g_cases[] = {
    "flow",
    "wrap",
};

static char g_html[IO_MAX];
static char g_css[IO_MAX];
static char g_aux[IO_MAX];
static char g_out[IO_MAX];
static char g_dump[IO_MAX];
static struct capy_dom_doc g_dom;
static struct capy_css_stylesheet g_sheet;
static struct capy_css_cascade g_casc;
static struct capy_layout_tree g_lay;

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

static void dump_box(size_t idx, size_t depth) {
  const struct capy_layout_box *b = &g_lay.boxes[idx];
  const char *label;
  char ln[160];
  size_t d;
  size_t child;

  if (b->kind == CAPY_LAYOUT_TEXT) {
    label = "#text";
  } else {
    const struct capy_dom_node *n = &g_dom.nodes[b->dom_node];
    label = (n->type == CAPY_DOM_DOCUMENT) ? "#root" : n->name;
  }
  for (d = 0; d < depth; d++) {
    dput("  ");
  }
  snprintf(ln, sizeof(ln), "%s @%ld,%ld %ldx%ld\n", label, b->x, b->y, b->width,
           b->height);
  dput(ln);
  child = b->first_child;
  while (child != CAPY_LAYOUT_NONE) {
    dump_box(child, depth + 1);
    child = g_lay.boxes[child].next_sibling;
  }
}

static void dump_tree(void) {
  g_len = 0;
  if (g_lay.root != CAPY_LAYOUT_NONE) {
    dump_box(g_lay.root, 0);
  }
  g_dump[g_len] = '\0';
}

static void run_case(struct capy_test_ctx *ctx, const char *dir,
                     const char *stem) {
  char path[1024];
  long width = 80;

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

  make_path(path, sizeof(path), dir, stem, ".width");
  if (read_file(path, g_aux, sizeof(g_aux)) >= 0) {
    long w = strtol(g_aux, NULL, 10);
    if (w >= 1) {
      width = w;
    }
  }

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
  if (capy_layout(&g_dom, &g_sheet, &g_casc, width, &g_lay) != CAPY_LAYOUT_OK) {
    check(ctx, stem, "capy_layout returned error", 0);
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
    check(ctx, stem, "box tree dump matches .out", ok);
    if (!ok) {
      fprintf(stderr, "    --- got ---\n%s\n    --- want ---\n%s\n", g_dump,
              g_out);
    }
  }
}

static void test_null_guards(struct capy_test_ctx *ctx) {
  CAPY_TEST_CHECK(ctx, capy_layout(NULL, &g_sheet, &g_casc, 80, &g_lay) ==
                           CAPY_LAYOUT_ERR_NULL);
  CAPY_TEST_CHECK(ctx, capy_layout(&g_dom, &g_sheet, &g_casc, 80, NULL) ==
                           CAPY_LAYOUT_ERR_NULL);
}

int main(int argc, char **argv) {
  struct capy_test_ctx ctx;
  const char *dir = (argc > 1) ? argv[1] : "tests/fixtures/layout";
  size_t i;

  ctx.checks = 0;
  ctx.failures = 0;

  for (i = 0; i < sizeof(g_cases) / sizeof(g_cases[0]); i++) {
    run_case(&ctx, dir, g_cases[i]);
  }
  test_null_guards(&ctx);

  return capy_test_report(&ctx, "layout");
}
