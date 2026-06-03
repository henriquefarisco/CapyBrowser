#include "dom_internal.h"

#include <string.h>

#include "html_entities.h"

void capy_dom_warn(struct capy_dom_doc *doc, enum capy_dom_warning code) {
  size_t i;
  for (i = 0; i < doc->warnings.count; i++) {
    if (doc->warnings.codes[i] == code) {
      return; /* record each warning once */
    }
  }
  if (doc->warnings.count < CAPY_DOM_WARN_MAX) {
    doc->warnings.codes[doc->warnings.count++] = code;
  }
}

int capy_dom_arena_put(struct capy_dom_doc *doc, char c) {
  if (doc->string_len >= CAPY_DOM_STRING_ARENA) {
    capy_dom_warn(doc, CAPY_DOM_WARN_STRING_BUDGET);
    doc->truncated = 1;
    return 0;
  }
  doc->strings[doc->string_len++] = c;
  return 1;
}

static char dom_to_lower(int c) {
  if (c >= 'A' && c <= 'Z') {
    return (char)(c - 'A' + 'a');
  }
  return (char)c;
}

/* A byte that must be dropped from interned strings (non-whitespace control). */
static int dom_drop_byte(uint32_t c) {
  if (c == 0x09u || c == 0x0Au || c == 0x0Cu || c == 0x0Du) {
    return 0; /* HTML whitespace is preserved in the DOM */
  }
  return c < 0x20u || c == 0x7Fu;
}

int capy_dom_intern(struct capy_dom_doc *doc, const char *src, size_t len,
                    int lower, int decode, size_t *off, size_t *out_len) {
  size_t start = doc->string_len;
  size_t i = 0;
  int ok = 1;

  while (i < len) {
    unsigned char c = (unsigned char)src[i];

    if (decode && c == '&') {
      uint32_t cp;
      size_t consumed = capy_html_charref_at(src + i, len - i, &cp);
      if (consumed > 0) {
        if (cp != CAPY_CP_INVALID && !dom_drop_byte(cp)) {
          char u[4];
          size_t k = capy_utf8_encode(cp, u);
          size_t j;
          for (j = 0; j < k; j++) {
            if (!capy_dom_arena_put(doc, u[j])) {
              ok = 0;
              break;
            }
          }
        }
        i += consumed;
        if (!ok) {
          break;
        }
        continue;
      }
    }

    if (!dom_drop_byte((uint32_t)c)) {
      char ch = lower ? dom_to_lower((int)c) : (char)c;
      if (!capy_dom_arena_put(doc, ch)) {
        ok = 0;
        break;
      }
    }
    i++;
  }

  *off = start;
  *out_len = doc->string_len - start;
  return ok;
}

size_t capy_dom_new_node(struct capy_dom_doc *doc,
                         enum capy_dom_node_type type) {
  struct capy_dom_node *n;
  if (doc->node_count >= CAPY_DOM_MAX_NODES) {
    capy_dom_warn(doc, CAPY_DOM_WARN_NODE_BUDGET);
    doc->truncated = 1;
    return CAPY_DOM_NONE;
  }
  n = &doc->nodes[doc->node_count];
  n->type = type;
  n->name[0] = '\0';
  n->attr_start = 0;
  n->attr_count = 0;
  n->text_off = 0;
  n->text_len = 0;
  n->parent = CAPY_DOM_NONE;
  n->first_child = CAPY_DOM_NONE;
  n->last_child = CAPY_DOM_NONE;
  n->next_sibling = CAPY_DOM_NONE;
  return doc->node_count++;
}

void capy_dom_append_child(struct capy_dom_doc *doc, size_t parent,
                           size_t child) {
  struct capy_dom_node *p;
  if (parent >= doc->node_count || child >= doc->node_count) {
    return;
  }
  p = &doc->nodes[parent];
  doc->nodes[child].parent = parent;
  if (p->first_child == CAPY_DOM_NONE) {
    p->first_child = child;
    p->last_child = child;
  } else {
    doc->nodes[p->last_child].next_sibling = child;
    p->last_child = child;
  }
}

const struct capy_dom_node *capy_dom_node_at(const struct capy_dom_doc *doc,
                                             size_t index) {
  if (!doc || index >= doc->node_count) {
    return NULL;
  }
  return &doc->nodes[index];
}

const char *capy_dom_string(const struct capy_dom_doc *doc, size_t off) {
  if (!doc || off > doc->string_len) {
    return NULL;
  }
  return doc->strings + off;
}

int capy_dom_find_attr(const struct capy_dom_doc *doc,
                       const struct capy_dom_node *node, const char *name,
                       size_t *value_off, size_t *value_len) {
  size_t i;
  size_t nlen;
  if (!doc || !node || !name) {
    return 0;
  }
  nlen = strlen(name);
  for (i = 0; i < node->attr_count; i++) {
    const struct capy_dom_attr *a = &doc->attrs[node->attr_start + i];
    if (a->name_len == nlen &&
        memcmp(doc->strings + a->name_off, name, nlen) == 0) {
      if (value_off) {
        *value_off = a->value_off;
      }
      if (value_len) {
        *value_len = a->value_len;
      }
      return 1;
    }
  }
  return 0;
}

const char *capy_dom_warning_name(enum capy_dom_warning w) {
  switch (w) {
    case CAPY_DOM_WARN_INPUT_TRUNCATED:
      return "INPUT_TRUNCATED";
    case CAPY_DOM_WARN_NODE_BUDGET:
      return "NODE_BUDGET";
    case CAPY_DOM_WARN_ATTR_BUDGET:
      return "ATTR_BUDGET";
    case CAPY_DOM_WARN_STRING_BUDGET:
      return "STRING_BUDGET";
    case CAPY_DOM_WARN_DEPTH_LIMIT:
      return "DEPTH_LIMIT";
    case CAPY_DOM_WARN_STRAY_END_TAG:
      return "STRAY_END_TAG";
    case CAPY_DOM_WARN_UNCLOSED_TAG:
      return "UNCLOSED_TAG";
    case CAPY_DOM_WARN_UNCLOSED_COMMENT:
      return "UNCLOSED_COMMENT";
  }
  return "UNKNOWN";
}
