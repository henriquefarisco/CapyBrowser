/* Golden tests for the production HTML -> display-list page pipeline. */

#include "capy_test.h"
#include "page_render.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IO_MAX 65536u
#define LINES_MAX 32u

static const char *const g_cases[] = {"pipeline", "hidden-content"};

static char g_html[IO_MAX];
static char g_css[IO_MAX];
static char g_aux[IO_MAX];
static char g_base[IO_MAX];
static char g_out[IO_MAX];
static char g_dump[IO_MAX];
static struct capy_page g_page;
static size_t g_len;

static long read_file(const char *path, char *buf, size_t cap) {
  FILE *f = fopen(path, "rb");
  size_t n;
  if (f == NULL) {
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

static size_t split_lines(char *buf, char *lines[], size_t maxlines) {
  size_t n = 0;
  char *p = buf;
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
  return n;
}

static void dput(const char *s) {
  while (*s != '\0' && g_len + 1 < sizeof(g_dump)) {
    g_dump[g_len++] = *s++;
  }
}

static void dput_bytes(const char *s, size_t n) {
  size_t i;
  for (i = 0; i < n && g_len + 1 < sizeof(g_dump); i++) {
    g_dump[g_len++] = s[i];
  }
}

static void dump_page(void) {
  char line[192];
  size_t i;
  g_len = 0;
  snprintf(line, sizeof(line),
           "page stage=%s boxes=%zu nodes=%zu content=%ldx%ld truncated=%d "
           "script=%d\n",
           capy_page_stage_name(g_page.completed_stage), g_page.layout.box_count,
           g_page.display_list.node_count, g_page.display_list.content_width,
           g_page.display_list.content_height, g_page.truncated,
           g_page.script_present);
  dput(line);
  snprintf(line, sizeof(line), "displaylist v%d\n", g_page.display_list.version);
  dput(line);
  for (i = 0; i < g_page.display_list.node_count; i++) {
    const struct capy_dl_node *n = &g_page.display_list.nodes[i];
    snprintf(line, sizeof(line), "%s @%ld,%ld %ldx%ld",
             capy_dl_node_kind_name(n->kind), n->x, n->y, n->width, n->height);
    dput(line);
    if (n->kind == CAPY_DL_TEXT) {
      dput(" \"");
      dput_bytes(g_page.display_list.strings + n->text_off, n->text_len);
      dput("\"");
      if (n->color_len > 0) {
        dput(" color=");
        dput_bytes(g_page.display_list.strings + n->color_off, n->color_len);
      }
    } else if (n->kind == CAPY_DL_RECT) {
      dput(" color=");
      dput_bytes(g_page.display_list.strings + n->color_off, n->color_len);
    } else if (n->kind == CAPY_DL_IMAGE) {
      if (n->label_len > 0) {
        dput(" alt=\"");
        dput_bytes(g_page.display_list.strings + n->label_off, n->label_len);
        dput("\"");
      }
      if (n->url_len > 0) {
        dput(" src=");
        dput_bytes(g_page.display_list.strings + n->url_off, n->url_len);
      }
    } else if (n->kind == CAPY_DL_LINK) {
      dput(" url=");
      dput_bytes(g_page.display_list.strings + n->url_off, n->url_len);
    }
    dput("\n");
  }
  g_dump[g_len] = '\0';
}

static void run_case(struct capy_test_ctx *ctx, const char *dir,
                     const char *stem) {
  char path[1024];
  long html_len;
  long css_len;
  long width = 80;
  const char *base = NULL;
  size_t i;

  make_path(path, sizeof(path), dir, stem, ".html");
  html_len = read_file(path, g_html, sizeof(g_html));
  if (html_len < 0) {
    check(ctx, stem, "missing .html fixture", 0);
    return;
  }
  make_path(path, sizeof(path), dir, stem, ".css");
  css_len = read_file(path, g_css, sizeof(g_css));
  if (css_len < 0) {
    check(ctx, stem, "missing .css fixture", 0);
    return;
  }
  make_path(path, sizeof(path), dir, stem, ".width");
  if (read_file(path, g_aux, sizeof(g_aux)) >= 0) {
    long parsed = strtol(g_aux, NULL, 10);
    if (parsed > 0) {
      width = parsed;
    }
  }
  make_path(path, sizeof(path), dir, stem, ".base");
  if (read_file(path, g_base, sizeof(g_base)) >= 0) {
    strip_eol(g_base);
    base = g_base;
  }

  check(ctx, stem, "capy_page_render returns OK",
        capy_page_render(g_html, (size_t)html_len, g_css, (size_t)css_len, base,
                         width, &g_page) == CAPY_PAGE_OK);
  check(ctx, stem, "pipeline reaches display-list",
        g_page.completed_stage == CAPY_PAGE_STAGE_DISPLAY_LIST);

  dump_page();
  strip_eol(g_dump);
  make_path(path, sizeof(path), dir, stem, ".out");
  if (read_file(path, g_out, sizeof(g_out)) < 0) {
    check(ctx, stem, "missing .out fixture", 0);
  } else {
    int ok;
    strip_eol(g_out);
    ok = strcmp(g_dump, g_out) == 0;
    check(ctx, stem, "page dump matches .out", ok);
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
          g_page.warnings.count == want);
    for (i = 0; i < g_page.warnings.count && i < want; i++) {
      check(ctx, stem, "warning name matches .warn",
            strcmp(capy_page_warning_name(g_page.warnings.codes[i]), lines[i]) ==
                0);
    }
  } else {
    check(ctx, stem, "no warnings expected", g_page.warnings.count == 0);
  }
}

static void test_optional_css_and_guards(struct capy_test_ctx *ctx) {
  static const char html[] = "<p>x</p>";
  CAPY_TEST_CHECK(ctx,
                  capy_page_render(html, sizeof(html) - 1, NULL, 0, NULL, 0,
                                   &g_page) == CAPY_PAGE_OK);
  CAPY_TEST_CHECK(ctx, g_page.layout.viewport_width == 1);
  CAPY_TEST_CHECK(ctx,
                  g_page.completed_stage == CAPY_PAGE_STAGE_DISPLAY_LIST);
  CAPY_TEST_CHECK(ctx, capy_page_render(NULL, 0, NULL, 0, NULL, 20, &g_page) ==
                           CAPY_PAGE_ERR_NULL);
  CAPY_TEST_CHECK(ctx, capy_page_render(html, sizeof(html) - 1, NULL, 1, NULL,
                                        20, &g_page) == CAPY_PAGE_ERR_NULL);
  CAPY_TEST_CHECK(ctx,
                  capy_page_render(html, sizeof(html) - 1, NULL, 0, NULL, 20,
                                   NULL) == CAPY_PAGE_ERR_NULL);
  CAPY_TEST_CHECK(ctx,
                  strcmp(capy_page_status_name(CAPY_PAGE_OK), "OK") == 0);
  CAPY_TEST_CHECK(
      ctx, strcmp(capy_page_stage_name(CAPY_PAGE_STAGE_DISPLAY_LIST),
                  "DISPLAY_LIST") == 0);
  CAPY_TEST_CHECK(ctx,
                  strcmp(capy_page_warning_name(CAPY_PAGE_WARN_SCRIPT_BLOCKED),
                         "SCRIPT_BLOCKED") == 0);
}

int main(int argc, char **argv) {
  struct capy_test_ctx ctx;
  const char *dir = (argc > 1) ? argv[1] : "tests/fixtures/page";
  size_t i;
  ctx.checks = 0;
  ctx.failures = 0;
  for (i = 0; i < sizeof(g_cases) / sizeof(g_cases[0]); i++) {
    run_case(&ctx, dir, g_cases[i]);
  }
  test_optional_css_and_guards(&ctx);
  return capy_test_report(&ctx, "page");
}
