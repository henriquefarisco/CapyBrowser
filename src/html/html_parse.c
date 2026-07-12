#include "dom.h"

#include <string.h>

#include "dom_internal.h"
#include "html_tokenizer.h"

/* Input budget, aligned with the HTML-to-text surface (Fase C2). */
#define CAPY_HTML_PARSE_MAX_INPUT (256u * 1024u)

static int is_void_element(const char *name) {
  static const char *const voids[] = {
      "area", "base",  "br",    "col",  "embed", "hr",  "img",
      "input", "link", "meta",  "param", "source", "track", "wbr"};
  size_t i;
  for (i = 0; i < sizeof(voids) / sizeof(voids[0]); i++) {
    if (strcmp(name, voids[i]) == 0) {
      return 1;
    }
  }
  return 0;
}

/* Copy a start tag's attributes into the doc attribute pool. */
static void build_attrs(struct capy_dom_doc *doc,
                        const struct capy_html_token *tok, size_t node_idx) {
  size_t start = doc->attr_count;
  size_t count = 0;
  size_t i;
  for (i = 0; i < tok->attr_count; i++) {
    const struct capy_html_attr *src = &tok->attrs[i];
    struct capy_dom_attr *dst;
    size_t noff = 0;
    size_t nlen = 0;
    size_t voff = 0;
    size_t vlen = 0;
    if (doc->attr_count >= CAPY_DOM_MAX_ATTRS) {
      capy_dom_warn(doc, CAPY_DOM_WARN_ATTR_BUDGET);
      doc->truncated = 1;
      break;
    }
    capy_dom_intern(doc, src->name, src->name_len, 1, 0, &noff, &nlen);
    if (src->has_value) {
      capy_dom_intern(doc, src->value, src->value_len, 0, 1, &voff, &vlen);
    }
    dst = &doc->attrs[doc->attr_count];
    dst->name_off = noff;
    dst->name_len = nlen;
    dst->value_off = voff;
    dst->value_len = vlen;
    dst->has_value = src->has_value;
    doc->attr_count++;
    count++;
  }
  doc->nodes[node_idx].attr_start = start;
  doc->nodes[node_idx].attr_count = count;
}

int capy_html_parse(const char *html, size_t html_len,
                    struct capy_dom_doc *doc) {
  struct capy_html_tokenizer tk;
  struct capy_html_token tok;
  size_t stack[CAPY_DOM_MAX_DEPTH];
  size_t depth;
  size_t in_len = html_len;
  size_t root;

  if (!html || !doc) {
    return CAPY_DOM_ERR_NULL;
  }
  memset(doc, 0, sizeof(*doc));

  root = capy_dom_new_node(doc, CAPY_DOM_DOCUMENT);
  doc->root = root; /* index 0; the pool is non-empty so this never fails */
  stack[0] = root;
  depth = 1;

  if (in_len > CAPY_HTML_PARSE_MAX_INPUT) {
    in_len = CAPY_HTML_PARSE_MAX_INPUT;
    capy_dom_warn(doc, CAPY_DOM_WARN_INPUT_TRUNCATED);
    doc->truncated = 1;
  }

  capy_html_tokenizer_init(&tk, html, in_len);
  while (capy_html_tokenizer_next(&tk, &tok)) {
    size_t parent = stack[depth - 1];

    switch (tok.type) {
      case CAPY_HTML_TOKEN_TEXT: {
        size_t off = 0;
        size_t len = 0;
        int decode = 1;
        if (doc->nodes[parent].type == CAPY_DOM_ELEMENT &&
            (strcmp(doc->nodes[parent].name, "script") == 0 ||
             strcmp(doc->nodes[parent].name, "style") == 0)) {
          decode = 0; /* script/style content is not entity-decoded */
        }
        capy_dom_intern(doc, tok.text, tok.text_len, 0, decode, &off, &len);
        if (len > 0) {
          size_t idx = capy_dom_new_node(doc, CAPY_DOM_TEXT);
          if (idx != CAPY_DOM_NONE) {
            doc->nodes[idx].text_off = off;
            doc->nodes[idx].text_len = len;
            capy_dom_append_child(doc, parent, idx);
          }
        }
        break;
      }
      case CAPY_HTML_TOKEN_START: {
        size_t idx = capy_dom_new_node(doc, CAPY_DOM_ELEMENT);
        if (strcmp(tok.name, "script") == 0) {
          /* Script nodes remain inspectable in the DOM, but the static engine
           * never executes or lays them out. Surface that policy decision to
           * production callers as a deterministic parser warning. */
          capy_dom_warn(doc, CAPY_DOM_WARN_SCRIPT_BLOCKED);
        }
        if (idx != CAPY_DOM_NONE) {
          strcpy(doc->nodes[idx].name, tok.name);
          build_attrs(doc, &tok, idx);
          capy_dom_append_child(doc, parent, idx);
          if (!tok.self_closing && !is_void_element(doc->nodes[idx].name)) {
            if (depth < CAPY_DOM_MAX_DEPTH) {
              stack[depth++] = idx;
            } else {
              capy_dom_warn(doc, CAPY_DOM_WARN_DEPTH_LIMIT);
              doc->truncated = 1;
            }
          }
        }
        if (tok.unclosed_tag) {
          capy_dom_warn(doc, CAPY_DOM_WARN_UNCLOSED_TAG);
        }
        break;
      }
      case CAPY_HTML_TOKEN_END: {
        size_t k = depth;
        int found = 0;
        while (k > 1) {
          k--;
          if (doc->nodes[stack[k]].type == CAPY_DOM_ELEMENT &&
              strcmp(doc->nodes[stack[k]].name, tok.name) == 0) {
            found = 1;
            break;
          }
        }
        if (found) {
          depth = k; /* close the matched element and anything still open in it */
        } else {
          capy_dom_warn(doc, CAPY_DOM_WARN_STRAY_END_TAG);
        }
        if (tok.unclosed_tag) {
          capy_dom_warn(doc, CAPY_DOM_WARN_UNCLOSED_TAG);
        }
        break;
      }
      case CAPY_HTML_TOKEN_COMMENT:
        if (tok.unclosed_comment) {
          capy_dom_warn(doc, CAPY_DOM_WARN_UNCLOSED_COMMENT);
        }
        break;
      case CAPY_HTML_TOKEN_DECL:
      case CAPY_HTML_TOKEN_EOF:
        break;
    }
  }

  return CAPY_DOM_OK;
}
