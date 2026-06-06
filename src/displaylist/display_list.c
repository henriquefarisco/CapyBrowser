/*
 * capy-browser-core: display-list emitter (Fase M3, part b).
 *
 * Pre-order walk of the M3a box tree emitting versioned draw nodes: background
 * rectangles, image placeholders and link bounds (per element, in that order),
 * then the element's children; text boxes emit a text run. Link hrefs are
 * resolved through the Fase C1 URL core. Pure, deterministic, fail-closed.
 */

#include "display_list.h"

#include "url_parse.h"

#include <string.h>

#define DL_NONE ((size_t)-1)

static void dl_warn(struct capy_dl *o, enum capy_dl_warning code) {
  size_t i;
  for (i = 0; i < o->warnings.count; i++) {
    if (o->warnings.codes[i] == code) {
      return;
    }
  }
  if (o->warnings.count < CAPY_DL_WARN_MAX) {
    o->warnings.codes[o->warnings.count++] = code;
  }
}

static int dl_is_space(unsigned char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

static int dl_put(struct capy_dl *o, char ch) {
  if (o->string_len >= CAPY_DL_STRING_ARENA) {
    dl_warn(o, CAPY_DL_WARN_STRING_BUDGET);
    o->truncated = 1;
    return 0;
  }
  o->strings[o->string_len++] = ch;
  return 1;
}

/* Copy bytes verbatim into the arena. Sets off/out_len. */
static void dl_intern_raw(struct capy_dl *o, const char *s, size_t len,
                          size_t *off, size_t *out_len) {
  size_t start = o->string_len;
  size_t i;
  for (i = 0; i < len; i++) {
    if (!dl_put(o, s[i])) {
      break;
    }
  }
  *off = start;
  *out_len = o->string_len - start;
}

/* Intern text with ASCII whitespace runs collapsed to single spaces, trimmed. */
static void dl_intern_text(struct capy_dl *o, const char *s, size_t len,
                           size_t *off, size_t *out_len) {
  size_t start = o->string_len;
  size_t i;
  int pending = 0;
  int wrote = 0;
  for (i = 0; i < len; i++) {
    unsigned char c = (unsigned char)s[i];
    if (dl_is_space(c)) {
      if (wrote) {
        pending = 1;
      }
      continue;
    }
    if (pending) {
      if (!dl_put(o, ' ')) {
        break;
      }
      pending = 0;
    }
    if (!dl_put(o, (char)c)) {
      break;
    }
    wrote = 1;
  }
  *off = start;
  *out_len = o->string_len - start;
}

static size_t dl_new_node(struct capy_dl *o, enum capy_dl_node_kind kind, long x,
                          long y, long w, long h) {
  struct capy_dl_node *n;
  if (o->node_count >= CAPY_DL_MAX_NODES) {
    dl_warn(o, CAPY_DL_WARN_NODE_BUDGET);
    o->truncated = 1;
    return DL_NONE;
  }
  n = &o->nodes[o->node_count];
  n->kind = kind;
  n->x = x;
  n->y = y;
  n->width = w;
  n->height = h;
  n->text_off = 0;
  n->text_len = 0;
  n->color_off = 0;
  n->color_len = 0;
  n->url_off = 0;
  n->url_len = 0;
  n->label_off = 0;
  n->label_len = 0;
  return o->node_count++;
}

struct dl_ctx {
  const struct capy_dom_doc *dom;
  const struct capy_css_stylesheet *sheet;
  const struct capy_css_cascade *casc;
  const struct capy_layout_tree *layout;
  const char *base_url;
  struct capy_dl *out;
};

static void dl_emit_text(struct dl_ctx *c, const struct capy_layout_box *box) {
  const struct capy_dom_node *tn = &c->dom->nodes[box->dom_node];
  const struct capy_css_computed *cs = &c->casc->styles[box->dom_node];
  size_t toff = 0;
  size_t tlen = 0;
  size_t nidx;
  dl_intern_text(c->out, c->dom->strings + tn->text_off, tn->text_len, &toff,
                 &tlen);
  nidx = dl_new_node(c->out, CAPY_DL_TEXT, box->x, box->y, box->width,
                     box->height);
  if (nidx == DL_NONE) {
    return;
  }
  c->out->nodes[nidx].text_off = toff;
  c->out->nodes[nidx].text_len = tlen;
  if (cs->set[CAPY_CSS_PROP_COLOR]) {
    size_t coff = 0;
    size_t clen = 0;
    dl_intern_raw(c->out, c->sheet->strings + cs->value_off[CAPY_CSS_PROP_COLOR],
                  cs->value_len[CAPY_CSS_PROP_COLOR], &coff, &clen);
    c->out->nodes[nidx].color_off = coff;
    c->out->nodes[nidx].color_len = clen;
  }
}

static void dl_emit_element(struct dl_ctx *c, const struct capy_layout_box *box,
                            const struct capy_dom_node *en) {
  const struct capy_css_computed *cs = &c->casc->styles[box->dom_node];

  if (cs->set[CAPY_CSS_PROP_BACKGROUND_COLOR]) {
    size_t coff = 0;
    size_t clen = 0;
    size_t nidx;
    dl_intern_raw(c->out,
                  c->sheet->strings + cs->value_off[CAPY_CSS_PROP_BACKGROUND_COLOR],
                  cs->value_len[CAPY_CSS_PROP_BACKGROUND_COLOR], &coff, &clen);
    nidx = dl_new_node(c->out, CAPY_DL_RECT, box->x, box->y, box->width,
                       box->height);
    if (nidx != DL_NONE) {
      c->out->nodes[nidx].color_off = coff;
      c->out->nodes[nidx].color_len = clen;
    }
  }

  if (strcmp(en->name, "img") == 0) {
    size_t nidx = dl_new_node(c->out, CAPY_DL_IMAGE, box->x, box->y, box->width,
                              box->height);
    if (nidx != DL_NONE) {
      size_t aoff = 0;
      size_t alen = 0;
      if (capy_dom_find_attr(c->dom, en, "alt", &aoff, &alen) && alen > 0) {
        size_t loff = 0;
        size_t llen = 0;
        dl_intern_raw(c->out, c->dom->strings + aoff, alen, &loff, &llen);
        c->out->nodes[nidx].label_off = loff;
        c->out->nodes[nidx].label_len = llen;
      }
    }
  }

  if (strcmp(en->name, "a") == 0) {
    size_t hoff = 0;
    size_t hlen = 0;
    if (capy_dom_find_attr(c->dom, en, "href", &hoff, &hlen) && hlen > 0 &&
        hlen <= CAPY_URL_MAX_LEN) {
      char hbuf[CAPY_URL_MAX_LEN + 1];
      struct capy_url url;
      memcpy(hbuf, c->dom->strings + hoff, hlen);
      hbuf[hlen] = '\0';
      if (capy_url_parse(hbuf, c->base_url, &url, NULL) == CAPY_URL_OK) {
        char ubuf[CAPY_URL_MAX_LEN + 1];
        int sl = capy_url_serialize(&url, ubuf, sizeof(ubuf));
        if (sl >= 0) {
          size_t uoff = 0;
          size_t ulen = 0;
          size_t nidx;
          dl_intern_raw(c->out, ubuf, (size_t)sl, &uoff, &ulen);
          nidx = dl_new_node(c->out, CAPY_DL_LINK, box->x, box->y, box->width,
                             box->height);
          if (nidx != DL_NONE) {
            c->out->nodes[nidx].url_off = uoff;
            c->out->nodes[nidx].url_len = ulen;
          }
        }
      }
    }
  }
}

static void dl_emit_box(struct dl_ctx *c, size_t box_idx) {
  const struct capy_layout_box *box = &c->layout->boxes[box_idx];
  size_t child;

  if (box->kind == CAPY_LAYOUT_TEXT) {
    dl_emit_text(c, box);
    return;
  }

  {
    const struct capy_dom_node *en = &c->dom->nodes[box->dom_node];
    if (en->type == CAPY_DOM_ELEMENT) {
      dl_emit_element(c, box, en);
    }
  }

  child = box->first_child;
  while (child != CAPY_LAYOUT_NONE) {
    dl_emit_box(c, child);
    child = c->layout->boxes[child].next_sibling;
  }
}

int capy_displaylist(const struct capy_dom_doc *dom,
                     const struct capy_css_stylesheet *sheet,
                     const struct capy_css_cascade *casc,
                     const struct capy_layout_tree *layout, const char *base_url,
                     struct capy_dl *out) {
  if (!dom || !sheet || !casc || !layout || !out) {
    return CAPY_DL_ERR_NULL;
  }
  out->version = CAPY_DL_VERSION;
  out->node_count = 0;
  out->string_len = 0;
  out->truncated = 0;
  out->warnings.count = 0;
  out->content_width = layout->viewport_width;
  out->content_height = layout->content_height;

  if (layout->root != CAPY_LAYOUT_NONE) {
    struct dl_ctx c;
    c.dom = dom;
    c.sheet = sheet;
    c.casc = casc;
    c.layout = layout;
    c.base_url = base_url;
    c.out = out;
    dl_emit_box(&c, layout->root);
  }
  return CAPY_DL_OK;
}

const char *capy_dl_node_kind_name(enum capy_dl_node_kind k) {
  switch (k) {
    case CAPY_DL_TEXT:
      return "text";
    case CAPY_DL_RECT:
      return "rect";
    case CAPY_DL_IMAGE:
      return "image";
    case CAPY_DL_LINK:
      return "link";
  }
  return "unknown";
}

const char *capy_dl_warning_name(enum capy_dl_warning w) {
  switch (w) {
    case CAPY_DL_WARN_NODE_BUDGET:
      return "NODE_BUDGET";
    case CAPY_DL_WARN_STRING_BUDGET:
      return "STRING_BUDGET";
  }
  return "UNKNOWN";
}
