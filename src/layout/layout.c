/*
 * capy-browser-core: static block layout (Fase M3, part a).
 *
 * A vertical block flow over the styled DOM. Block boxes stack their children
 * and span the parent content width; text nodes collapse whitespace and
 * greedy-wrap to that width. `display: none` drops a subtree. Pure,
 * deterministic, allocation-free, fail-closed.
 */

#include "layout.h"

#include <string.h>

static void layout_warn(struct capy_layout_tree *t,
                        enum capy_layout_warning code) {
  size_t i;
  for (i = 0; i < t->warnings.count; i++) {
    if (t->warnings.codes[i] == code) {
      return;
    }
  }
  if (t->warnings.count < CAPY_LAYOUT_WARN_MAX) {
    t->warnings.codes[t->warnings.count++] = code;
  }
}

static int layout_is_space(unsigned char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

/*
 * Count the lines `len` bytes of text occupy when greedy-wrapped to `width`
 * columns, after collapsing ASCII whitespace runs and trimming. Each byte is
 * one column (monospace approximation). Returns 0 for whitespace-only text.
 */
static long layout_text_lines(const char *s, size_t len, long width) {
  long lines = 0;
  long col = 0;
  size_t i = 0;
  if (width < 1) {
    width = 1;
  }
  while (i < len) {
    size_t start;
    long wlen;
    while (i < len && layout_is_space((unsigned char)s[i])) {
      i++;
    }
    if (i >= len) {
      break;
    }
    start = i;
    while (i < len && !layout_is_space((unsigned char)s[i])) {
      i++;
    }
    wlen = (long)(i - start);
    if (lines == 0) {
      lines = 1;
      col = wlen;
    } else if (col + 1 + wlen <= width) {
      col += 1 + wlen;
    } else {
      lines++;
      col = wlen;
    }
  }
  return lines;
}

static int layout_value_eq(const struct capy_css_stylesheet *s, size_t off,
                           size_t len, const char *lit) {
  size_t n = strlen(lit);
  return len == n && memcmp(s->strings + off, lit, n) == 0;
}

struct layout_ctx {
  const struct capy_dom_doc *dom;
  const struct capy_css_stylesheet *sheet;
  const struct capy_css_cascade *casc;
  struct capy_layout_tree *out;
};

static int layout_is_display_none(const struct layout_ctx *c, size_t node) {
  const struct capy_css_computed *cs = &c->casc->styles[node];
  if (!cs->set[CAPY_CSS_PROP_DISPLAY]) {
    return 0;
  }
  return layout_value_eq(c->sheet, cs->value_off[CAPY_CSS_PROP_DISPLAY],
                         cs->value_len[CAPY_CSS_PROP_DISPLAY], "none");
}

/* HTML elements that never produce a CSS box in the static engine. Keep the
 * rule here, at the layout boundary: the DOM remains complete and inspectable,
 * while every renderer consuming the box tree gets the same safe behavior.
 * `head` and `template` suppress their complete subtrees. The metadata/rawtext
 * entries are also listed individually so malformed markup that places them in
 * the body cannot leak their contents into the visible page. */
static int layout_is_non_rendered_element(const struct capy_dom_node *node) {
  static const char *const hidden[] = {
      "base", "datalist", "head", "link",   "meta",
      "script", "style",  "template", "title",
  };
  size_t i;
  for (i = 0; i < sizeof(hidden) / sizeof(hidden[0]); i++) {
    if (strcmp(node->name, hidden[i]) == 0) {
      return 1;
    }
  }
  return 0;
}

static size_t layout_new_box(struct capy_layout_tree *t,
                             enum capy_layout_box_kind kind, size_t dom_node) {
  struct capy_layout_box *b;
  if (t->box_count >= CAPY_LAYOUT_MAX_BOXES) {
    layout_warn(t, CAPY_LAYOUT_WARN_BOX_BUDGET);
    t->truncated = 1;
    return CAPY_LAYOUT_NONE;
  }
  b = &t->boxes[t->box_count];
  b->kind = kind;
  b->dom_node = dom_node;
  b->x = 0;
  b->y = 0;
  b->width = 0;
  b->height = 0;
  b->parent = CAPY_LAYOUT_NONE;
  b->first_child = CAPY_LAYOUT_NONE;
  b->last_child = CAPY_LAYOUT_NONE;
  b->next_sibling = CAPY_LAYOUT_NONE;
  return t->box_count++;
}

static void layout_append_child(struct capy_layout_tree *t, size_t parent,
                                size_t child) {
  struct capy_layout_box *p;
  if (parent >= t->box_count || child >= t->box_count) {
    return;
  }
  p = &t->boxes[parent];
  t->boxes[child].parent = parent;
  if (p->first_child == CAPY_LAYOUT_NONE) {
    p->first_child = child;
    p->last_child = child;
  } else {
    t->boxes[p->last_child].next_sibling = child;
    p->last_child = child;
  }
}

/* Lay out the children of `parent_dom` as a vertical block flow stacked from
 * (x, top) within `width`. Returns the total stacked height. */
static long layout_children(struct layout_ctx *c, size_t parent_dom,
                            size_t parent_box, long x, long top, long width,
                            size_t depth) {
  long cursor = top;
  size_t child;

  if (depth >= CAPY_LAYOUT_MAX_DEPTH) {
    layout_warn(c->out, CAPY_LAYOUT_WARN_DEPTH_LIMIT);
    c->out->truncated = 1;
    return 0;
  }

  child = c->dom->nodes[parent_dom].first_child;
  while (child != CAPY_DOM_NONE) {
    const struct capy_dom_node *cn = &c->dom->nodes[child];
    if (cn->type == CAPY_DOM_ELEMENT) {
      if (!layout_is_non_rendered_element(cn) &&
          !layout_is_display_none(c, child)) {
        size_t b = layout_new_box(c->out, CAPY_LAYOUT_BLOCK, child);
        if (b != CAPY_LAYOUT_NONE) {
          long h;
          c->out->boxes[b].x = x;
          c->out->boxes[b].y = cursor;
          c->out->boxes[b].width = width;
          layout_append_child(c->out, parent_box, b);
          h = layout_children(c, child, b, x, cursor, width, depth + 1);
          c->out->boxes[b].height = h;
          cursor += h;
        }
      }
    } else if (cn->type == CAPY_DOM_TEXT) {
      long lines =
          layout_text_lines(c->dom->strings + cn->text_off, cn->text_len, width);
      if (lines > 0) {
        size_t b = layout_new_box(c->out, CAPY_LAYOUT_TEXT, child);
        if (b != CAPY_LAYOUT_NONE) {
          c->out->boxes[b].x = x;
          c->out->boxes[b].y = cursor;
          c->out->boxes[b].width = width;
          c->out->boxes[b].height = lines;
          layout_append_child(c->out, parent_box, b);
          cursor += lines;
        }
      }
    }
    child = c->dom->nodes[child].next_sibling;
  }
  return cursor - top;
}

int capy_layout(const struct capy_dom_doc *dom,
                const struct capy_css_stylesheet *sheet,
                const struct capy_css_cascade *casc, long viewport_width,
                struct capy_layout_tree *out) {
  size_t root;

  if (!dom || !sheet || !casc || !out) {
    return CAPY_LAYOUT_ERR_NULL;
  }
  out->box_count = 0;
  out->root = CAPY_LAYOUT_NONE;
  out->content_height = 0;
  out->truncated = 0;
  out->warnings.count = 0;
  if (viewport_width < 1) {
    viewport_width = 1;
  }
  out->viewport_width = viewport_width;

  root = layout_new_box(out, CAPY_LAYOUT_BLOCK, dom->root);
  out->root = root;
  if (root != CAPY_LAYOUT_NONE) {
    struct layout_ctx c;
    long h;
    out->boxes[root].x = 0;
    out->boxes[root].y = 0;
    out->boxes[root].width = viewport_width;
    c.dom = dom;
    c.sheet = sheet;
    c.casc = casc;
    c.out = out;
    h = layout_children(&c, dom->root, root, 0, 0, viewport_width, 0);
    out->boxes[root].height = h;
    out->content_height = h;
  }
  return CAPY_LAYOUT_OK;
}

const char *capy_layout_warning_name(enum capy_layout_warning w) {
  switch (w) {
    case CAPY_LAYOUT_WARN_BOX_BUDGET:
      return "BOX_BUDGET";
    case CAPY_LAYOUT_WARN_DEPTH_LIMIT:
      return "DEPTH_LIMIT";
  }
  return "UNKNOWN";
}
