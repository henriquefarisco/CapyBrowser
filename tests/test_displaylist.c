/*
 * Host-side test for the display-list emitter (Fase M3, part b).
 *
 * Fixture-driven: each case pairs <stem>.html and <stem>.css with an optional
 * <stem>.width (viewport columns, default 80) and <stem>.base (base URL fo
 * link resolution); the emitted display list is dumped to a deterministic text
 * and compared against the golden <stem>.out. Fixtures live unde
 * tests/fixtures/display-list/ (override dir via argv[1]).
 *
 * Dump format:
 *   displaylist v<version> <cw>x<ch>
 *   <kind> @<x>,<y> <w>x<h> [payload]
 * where payload is `"text" [color=...]`, `color=...`, `[alt="..."]` o
 * `url=...` per kind.
 */

#include "capy_test.h"

#include "cascade.h"
#include "css_parse.h"
#include "display_list.h"
#include "dom.h"
#include "layout.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IO_MAX 65536u

static const char *const g_cases[] = {
    "page",
    "plain",
};

static char g_html[IO_MAX];
static char g_css[IO_MAX];
static char g_aux[IO_MAX];
static char g_base[IO_MAX];
static char g_out[IO_MAX];
static char g_dump[IO_MAX];
static struct capy_dom_doc g_dom;
static struct capy_css_stylesheet g_sheet;
static struct capy_css_cascade g_casc;
static struct capy_layout_tree g_lay;
static struct capy_dl g_dl;

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

static void dump_dl(void) {
  char ln[160];
  size_t i;
  g_len = 0;
  snprintf(ln, sizeof(ln), "displaylist v%d %ldx%ld\n", g_dl.version,
           g_dl.content_width, g_dl.content_height);
  dput(ln);
  for (i = 0; i < g_dl.node_count; i++) {
    const struct capy_dl_node *n = &g_dl.nodes[i];
    snprintf(ln, sizeof(ln), "%s @%ld,%ld %ldx%ld",
             capy_dl_node_kind_name(n->kind), n->x, n->y, n->width, n->height);
    dput(ln);
    switch (n->kind) {
      case CAPY_DL_TEXT:
        dput(" \"");
        dput_bytes(g_dl.strings + n->text_off, n->text_len);
        dput("\"");
        if (n->color_len > 0) {
          dput(" color=");
          dput_bytes(g_dl.strings + n->color_off, n->color_len);
        }
        break;
      case CAPY_DL_RECT:
        dput(" color=");
        dput_bytes(g_dl.strings + n->color_off, n->color_len);
        break;
      case CAPY_DL_IMAGE:
        if (n->label_len > 0) {
          dput(" alt=\"");
          dput_bytes(g_dl.strings + n->label_off, n->label_len);
          dput("\"");
        }
        break;
      case CAPY_DL_LINK:
        dput(" url=");
        dput_bytes(g_dl.strings + n->url_off, n->url_len);
        break;
    }
    dput("\n");
  }
  g_dump[g_len] = '\0';
}

static void run_case(struct capy_test_ctx *ctx, const char *dir,
                     const char *stem) {
  char path[1024];
  long width = 80;
  const char *base_ptr = NULL;

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

  make_path(path, sizeof(path), dir, stem, ".base");
  if (read_file(path, g_base, sizeof(g_base)) >= 0) {
    strip_eol(g_base);
    base_ptr = g_base;
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
  if (capy_displaylist(&g_dom, &g_sheet, &g_casc, &g_lay, base_ptr, &g_dl) !=
      CAPY_DL_OK) {
    check(ctx, stem, "capy_displaylist returned error", 0);
    return;
  }

  dump_dl();
  strip_eol(g_dump);

  make_path(path, sizeof(path), dir, stem, ".out");
  if (read_file(path, g_out, sizeof(g_out)) < 0) {
    g_out[0] = '\0';
  } else {
    strip_eol(g_out);
  }
  {
    int ok = (strcmp(g_dump, g_out) == 0);
    check(ctx, stem, "display list dump matches .out", ok);
    if (!ok) {
      fprintf(stderr, "    --- got ---\n%s\n    --- want ---\n%s\n", g_dump,
              g_out);
    }
  }
}

static void test_null_guards(struct capy_test_ctx *ctx) {
  CAPY_TEST_CHECK(ctx, capy_displaylist(NULL, &g_sheet, &g_casc, &g_lay, NULL,
                                        &g_dl) == CAPY_DL_ERR_NULL);
  CAPY_TEST_CHECK(ctx, capy_displaylist(&g_dom, &g_sheet, &g_casc, &g_lay, NULL,
                                        NULL) == CAPY_DL_ERR_NULL);
}

int main(int argc, char **argv) {
  struct capy_test_ctx ctx;
  const char *dir = (argc > 1) ? argv[1] : "tests/fixtures/display-list";
  size_t i;

  ctx.checks = 0;
  ctx.failures = 0;

  for (i = 0; i < sizeof(g_cases) / sizeof(g_cases[0]); i++) {
    run_case(&ctx, dir, g_cases[i]);
  }
  test_null_guards(&ctx);

  return capy_test_report(&ctx, "display-list");
}
