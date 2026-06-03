/*
 * Host-side test for the tolerant HTML -> DOM tree (Fase M1).
 *
 * Fixture-driven: each case is dumped to a deterministic indented tree text and
 * compared against the golden .out; warnings against .warn. Plus a few in-memory
 * accessor assertions. Fixtures live under tests/fixtures/dom/ (override dir via
 * argv[1]).
 *
 * Dump format: two spaces per depth level; elements as `tag name="value" ...`
 * (attributes in document order; value-less attributes as bare names); text as
 * a quoted, escaped string (\\ \" \n \t \r).
 */

#include "capy_test.h"
#include "dom.h"

#include <stdio.h>
#include <string.h>

#define IO_MAX 65536u
#define LINES_MAX 32u

static const char *const g_cases[] = {
    "attributes", "basic",        "entities-attr", "misnested",
    "script-raw", "stray-end",    "void-elements",
};

static char g_in[IO_MAX];
static char g_out[IO_MAX];
static char g_aux[IO_MAX];
static char g_dump[IO_MAX];
static struct capy_dom_doc g_doc;

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

static void dputc_raw(char c) {
  if (g_len + 1 < sizeof(g_dump)) {
    g_dump[g_len++] = c;
  }
}

static void dput_bytes(const char *s, size_t n) {
  size_t i;
  for (i = 0; i < n; i++) {
    dputc_raw(s[i]);
  }
}

static void dput_escaped(const char *s, size_t n) {
  size_t i;
  for (i = 0; i < n; i++) {
    char c = s[i];
    switch (c) {
      case '\\':
        dput("\\\\");
        break;
      case '"':
        dput("\\\"");
        break;
      case '\n':
        dput("\\n");
        break;
      case '\t':
        dput("\\t");
        break;
      case '\r':
        dput("\\r");
        break;
      default:
        dputc_raw(c);
        break;
    }
  }
}

static void dump_node(const struct capy_dom_doc *doc, size_t idx, size_t depth) {
  const struct capy_dom_node *n = &doc->nodes[idx];
  size_t d;
  size_t child;
  for (d = 0; d < depth; d++) {
    dput("  ");
  }
  if (n->type == CAPY_DOM_TEXT) {
    dputc_raw('"');
    dput_escaped(doc->strings + n->text_off, n->text_len);
    dputc_raw('"');
  } else {
    size_t a;
    dput(n->name);
    for (a = 0; a < n->attr_count; a++) {
      const struct capy_dom_attr *at = &doc->attrs[n->attr_start + a];
      dputc_raw(' ');
      dput_bytes(doc->strings + at->name_off, at->name_len);
      if (at->has_value) {
        dput("=\"");
        dput_escaped(doc->strings + at->value_off, at->value_len);
        dputc_raw('"');
      }
    }
  }
  dputc_raw('\n');
  child = n->first_child;
  while (child != CAPY_DOM_NONE) {
    dump_node(doc, child, depth + 1);
    child = doc->nodes[child].next_sibling;
  }
}

static void dump_tree(const struct capy_dom_doc *doc) {
  size_t child;
  g_len = 0;
  child = doc->nodes[doc->root].first_child;
  while (child != CAPY_DOM_NONE) {
    dump_node(doc, child, 0);
    child = doc->nodes[child].next_sibling;
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

  if (capy_html_parse(g_in, strlen(g_in), &g_doc) != CAPY_DOM_OK) {
    check(ctx, stem, "capy_html_parse returned error", 0);
    return;
  }

  dump_tree(&g_doc);
  strip_eol(g_dump);

  make_path(path, sizeof(path), dir, stem, ".out");
  if (read_file(path, g_out, sizeof(g_out)) < 0) {
    g_out[0] = '\0';
  } else {
    strip_eol(g_out);
  }
  {
    int ok = (strcmp(g_dump, g_out) == 0);
    check(ctx, stem, "tree dump matches .out", ok);
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
    check(ctx, stem, "warning count matches .warn", g_doc.warnings.count == want);
    for (i = 0; i < g_doc.warnings.count && i < want; i++) {
      const char *got = capy_dom_warning_name(g_doc.warnings.codes[i]);
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

static void test_accessors(struct capy_test_ctx *ctx) {
  const struct capy_dom_node *root;
  const struct capy_dom_node *a;
  const struct capy_dom_node *text;
  size_t voff = 0;
  size_t vlen = 0;
  static const char html[] = "<a href=\"https://x/\">hi</a>";

  CAPY_TEST_CHECK(ctx,
                  capy_html_parse(html, sizeof(html) - 1, &g_doc) == CAPY_DOM_OK);
  root = capy_dom_node_at(&g_doc, g_doc.root);
  CAPY_TEST_CHECK(ctx, root != NULL && root->type == CAPY_DOM_DOCUMENT);
  CAPY_TEST_CHECK(ctx, root != NULL && root->first_child != CAPY_DOM_NONE);
  a = capy_dom_node_at(&g_doc, root->first_child);
  CAPY_TEST_CHECK(ctx, a != NULL && a->type == CAPY_DOM_ELEMENT);
  CAPY_TEST_CHECK(ctx, a != NULL && strcmp(a->name, "a") == 0);
  CAPY_TEST_CHECK(ctx, a != NULL && capy_dom_find_attr(&g_doc, a, "href", &voff,
                                                       &vlen) == 1);
  CAPY_TEST_CHECK_EQ_UINT(ctx, vlen, 10u); /* "https://x/" */
  CAPY_TEST_CHECK(ctx, memcmp(g_doc.strings + voff, "https://x/", 10) == 0);
  text = (a != NULL) ? capy_dom_node_at(&g_doc, a->first_child) : NULL;
  CAPY_TEST_CHECK(ctx, text != NULL && text->type == CAPY_DOM_TEXT);
  CAPY_TEST_CHECK(ctx, text != NULL && text->text_len == 2u &&
                           memcmp(g_doc.strings + text->text_off, "hi", 2) == 0);
}

static void test_null_guards(struct capy_test_ctx *ctx) {
  CAPY_TEST_CHECK(ctx, capy_html_parse(NULL, 0, &g_doc) == CAPY_DOM_ERR_NULL);
  CAPY_TEST_CHECK(ctx, capy_html_parse("<p>x</p>", 8, NULL) == CAPY_DOM_ERR_NULL);
}

int main(int argc, char **argv) {
  struct capy_test_ctx ctx;
  const char *dir = (argc > 1) ? argv[1] : "tests/fixtures/dom";
  size_t i;

  ctx.checks = 0;
  ctx.failures = 0;

  for (i = 0; i < sizeof(g_cases) / sizeof(g_cases[0]); i++) {
    run_case(&ctx, dir, g_cases[i]);
  }
  test_accessors(&ctx);
  test_null_guards(&ctx);

  return capy_test_report(&ctx, "dom");
}
