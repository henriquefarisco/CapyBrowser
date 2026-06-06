/*
 * capy-browser-core: CSS cascade (Fase M2, part 2).
 *
 * Matches simple selectors onto DOM elements, resolves each known property by
 * specificity then source order, and propagates inherited properties from
 * parent to child. Pure, deterministic, allocation-free.
 */

#include "cascade.h"

#include <string.h>

static int prop_eq(const char *a, size_t alen, const char *lit) {
  size_t n = strlen(lit);
  return alen == n && memcmp(a, lit, n) == 0;
}

/* Map a (lower-cased) property name to a known index, or -1 if unsupported. */
static int prop_index(const char *name, size_t len) {
  if (prop_eq(name, len, "display")) {
    return CAPY_CSS_PROP_DISPLAY;
  }
  if (prop_eq(name, len, "color")) {
    return CAPY_CSS_PROP_COLOR;
  }
  if (prop_eq(name, len, "background-color")) {
    return CAPY_CSS_PROP_BACKGROUND_COLOR;
  }
  if (prop_eq(name, len, "font-weight")) {
    return CAPY_CSS_PROP_FONT_WEIGHT;
  }
  if (prop_eq(name, len, "font-style")) {
    return CAPY_CSS_PROP_FONT_STYLE;
  }
  if (prop_eq(name, len, "text-align")) {
    return CAPY_CSS_PROP_TEXT_ALIGN;
  }
  if (prop_eq(name, len, "text-decoration")) {
    return CAPY_CSS_PROP_TEXT_DECORATION;
  }
  return -1;
}

/* Inherited property subset (color and font/text typography). */
static int prop_inherited(int p) {
  return p == CAPY_CSS_PROP_COLOR || p == CAPY_CSS_PROP_FONT_WEIGHT ||
         p == CAPY_CSS_PROP_FONT_STYLE || p == CAPY_CSS_PROP_TEXT_ALIGN;
}

static int selector_specificity(enum capy_css_selector_kind k) {
  switch (k) {
    case CAPY_CSS_SEL_ID:
      return 100;
    case CAPY_CSS_SEL_CLASS:
      return 10;
    case CAPY_CSS_SEL_TYPE:
      return 1;
    case CAPY_CSS_SEL_UNIVERSAL:
    default:
      return 0;
  }
}

static int bytes_eq(const char *a, size_t alen, const char *b, size_t blen) {
  return alen == blen && (alen == 0 || memcmp(a, b, alen) == 0);
}

/* Does element `node` match `sel` (whose name lives in sheet->strings)? */
static int selector_matches(const struct capy_dom_doc *dom,
                            const struct capy_dom_node *node,
                            const struct capy_css_stylesheet *sheet,
                            const struct capy_css_selector *sel) {
  const char *sname = sheet->strings + sel->name_off;
  size_t slen = sel->name_len;
  switch (sel->kind) {
    case CAPY_CSS_SEL_UNIVERSAL:
      return 1;
    case CAPY_CSS_SEL_TYPE:
      return bytes_eq(node->name, strlen(node->name), sname, slen);
    case CAPY_CSS_SEL_ID: {
      size_t voff = 0;
      size_t vlen = 0;
      if (!capy_dom_find_attr(dom, node, "id", &voff, &vlen)) {
        return 0;
      }
      return bytes_eq(dom->strings + voff, vlen, sname, slen);
    }
    case CAPY_CSS_SEL_CLASS: {
      size_t voff = 0;
      size_t vlen = 0;
      const char *cls;
      size_t i;
      size_t start;
      if (!capy_dom_find_attr(dom, node, "class", &voff, &vlen)) {
        return 0;
      }
      cls = dom->strings + voff;
      i = 0;
      start = 0;
      for (;;) {
        int sp = (i >= vlen) || cls[i] == ' ' || cls[i] == '\t' ||
                 cls[i] == '\n' || cls[i] == '\r' || cls[i] == '\f';
        if (sp) {
          if (i > start && bytes_eq(cls + start, i - start, sname, slen)) {
            return 1;
          }
          start = i + 1;
        }
        if (i >= vlen) {
          break;
        }
        i++;
      }
      return 0;
    }
    default:
      return 0;
  }
}

/* Apply every matching rule's declarations to one element's computed style. */
static void cascade_apply_rules(const struct capy_dom_doc *dom,
                                const struct capy_dom_node *node,
                                const struct capy_css_stylesheet *sheet,
                                struct capy_css_computed *cs) {
  int best_spec[CAPY_CSS_PROP_COUNT];
  size_t p;
  size_t ri;
  for (p = 0; p < CAPY_CSS_PROP_COUNT; p++) {
    best_spec[p] = -1;
  }
  for (ri = 0; ri < sheet->rule_count; ri++) {
    const struct capy_css_rule *r = &sheet->rules[ri];
    int spec;
    size_t di;
    if (!selector_matches(dom, node, sheet, &r->selector)) {
      continue;
    }
    spec = selector_specificity(r->selector.kind);
    for (di = 0; di < r->decl_count; di++) {
      const struct capy_css_decl *d = &sheet->decls[r->decl_start + di];
      int pi = prop_index(sheet->strings + d->prop_off, d->prop_len);
      if (pi < 0) {
        continue;
      }
      /* Source order: iterate ascending, so a later equal-specificity rule
       * overrides (>=). A lower-specificity later rule does not. */
      if (!cs->set[pi] || spec >= best_spec[pi]) {
        cs->set[pi] = 1;
        cs->value_off[pi] = d->value_off;
        cs->value_len[pi] = d->value_len;
        best_spec[pi] = spec;
      }
    }
  }
}

static void cascade_node(const struct capy_dom_doc *dom,
                         const struct capy_css_stylesheet *sheet,
                         struct capy_css_cascade *out, size_t idx,
                         size_t parent_idx) {
  const struct capy_dom_node *node = &dom->nodes[idx];
  struct capy_css_computed *cs = &out->styles[idx];
  size_t child;

  if (node->type == CAPY_DOM_ELEMENT) {
    cascade_apply_rules(dom, node, sheet, cs);
  }
  if (parent_idx != CAPY_DOM_NONE) {
    const struct capy_css_computed *ps = &out->styles[parent_idx];
    size_t p;
    for (p = 0; p < CAPY_CSS_PROP_COUNT; p++) {
      if (!cs->set[p] && prop_inherited((int)p) && ps->set[p]) {
        cs->set[p] = 1;
        cs->value_off[p] = ps->value_off[p];
        cs->value_len[p] = ps->value_len[p];
      }
    }
  }
  child = node->first_child;
  while (child != CAPY_DOM_NONE) {
    cascade_node(dom, sheet, out, child, idx);
    child = dom->nodes[child].next_sibling;
  }
}

int capy_css_cascade(const struct capy_dom_doc *dom,
                     const struct capy_css_stylesheet *sheet,
                     struct capy_css_cascade *out) {
  if (!dom || !sheet || !out) {
    return CAPY_CSS_CASCADE_ERR_NULL;
  }
  memset(out, 0, sizeof(*out));
  out->node_count = dom->node_count;
  if (dom->node_count > 0) {
    cascade_node(dom, sheet, out, dom->root, CAPY_DOM_NONE);
  }
  return CAPY_CSS_CASCADE_OK;
}

const char *capy_css_prop_name(enum capy_css_prop p) {
  switch (p) {
    case CAPY_CSS_PROP_DISPLAY:
      return "display";
    case CAPY_CSS_PROP_COLOR:
      return "color";
    case CAPY_CSS_PROP_BACKGROUND_COLOR:
      return "background-color";
    case CAPY_CSS_PROP_FONT_WEIGHT:
      return "font-weight";
    case CAPY_CSS_PROP_FONT_STYLE:
      return "font-style";
    case CAPY_CSS_PROP_TEXT_ALIGN:
      return "text-align";
    case CAPY_CSS_PROP_TEXT_DECORATION:
      return "text-decoration";
  }
  return "unknown";
}
