#ifndef CAPY_DOM_H
#define CAPY_DOM_H

/*
 * capy-browser-core: tolerant HTML -> DOM-like tree (Fase M1).
 *
 * Builds a deterministic element/text tree from HTML, reusing the Fase C2
 * tokenizer. Tolerant (never aborts on malformed input; recovers with
 * warnings) and allocation-free: the caller provides a `struct capy_dom_doc`
 * arena (node pool + attribute pool + string arena). This is the structural
 * substrate for CSS (Fase M2) and static layout / display-list (Fase M3); it is
 * not a full HTML5 tree builder (no implied-tag insertion or adoption agency).
 */

#include <stddef.h>
#include <stdint.h>

/* Arena capacities (alpha; configurable per integration stage). */
#define CAPY_DOM_MAX_NODES 1024u
#define CAPY_DOM_MAX_ATTRS 1024u
#define CAPY_DOM_STRING_ARENA 65536u
#define CAPY_DOM_MAX_DEPTH 128u
#define CAPY_DOM_TAG_MAX 32u

/* Sentinel "no node / no offset" index. */
#define CAPY_DOM_NONE ((size_t)-1)

enum capy_dom_status {
  CAPY_DOM_OK = 0,
  CAPY_DOM_ERR_NULL = -1
};

enum capy_dom_node_type {
  CAPY_DOM_DOCUMENT = 0, /* synthetic root */
  CAPY_DOM_ELEMENT = 1,
  CAPY_DOM_TEXT = 2
};

/* Deterministic warnings, recorded once each in first-occurrence order during
 * the single parse pass (same input -> same sequence). */
enum capy_dom_warning {
  CAPY_DOM_WARN_INPUT_TRUNCATED = 1,  /* HTML exceeded the input budget */
  CAPY_DOM_WARN_NODE_BUDGET = 2,      /* node pool full */
  CAPY_DOM_WARN_ATTR_BUDGET = 3,      /* attribute pool full */
  CAPY_DOM_WARN_STRING_BUDGET = 4,    /* string arena full */
  CAPY_DOM_WARN_DEPTH_LIMIT = 5,      /* nesting exceeded CAPY_DOM_MAX_DEPTH */
  CAPY_DOM_WARN_STRAY_END_TAG = 6,    /* end tag with no open match */
  CAPY_DOM_WARN_UNCLOSED_TAG = 7,     /* a tag ran to end-of-input */
  CAPY_DOM_WARN_UNCLOSED_COMMENT = 8, /* a comment ran to end-of-input */
  CAPY_DOM_WARN_SCRIPT_BLOCKED = 9    /* script present; never executed */
};

#define CAPY_DOM_WARN_MAX 16u

struct capy_dom_warnings {
  enum capy_dom_warning codes[CAPY_DOM_WARN_MAX];
  size_t count;
};

/* An attribute: name/value as ranges into doc->strings (name lower-cased,
 * value entity-decoded). value_len is 0 for a value-less attribute. */
struct capy_dom_attr {
  size_t name_off;
  size_t name_len;
  size_t value_off;
  size_t value_len;
  int has_value;
};

struct capy_dom_node {
  enum capy_dom_node_type type;
  char name[CAPY_DOM_TAG_MAX + 1]; /* ELEMENT: lower-cased tag; "" otherwise */
  size_t attr_start;               /* ELEMENT: first attr index in doc->attrs */
  size_t attr_count;
  size_t text_off; /* TEXT: range into doc->strings */
  size_t text_len;
  size_t parent;
  size_t first_child;
  size_t last_child;
  size_t next_sibling;
};

struct capy_dom_doc {
  struct capy_dom_node nodes[CAPY_DOM_MAX_NODES];
  size_t node_count;
  struct capy_dom_attr attrs[CAPY_DOM_MAX_ATTRS];
  size_t attr_count;
  char strings[CAPY_DOM_STRING_ARENA];
  size_t string_len;
  size_t root; /* index of the synthetic CAPY_DOM_DOCUMENT node */
  int truncated;
  struct capy_dom_warnings warnings;
};

/*
 * Parse HTML into a DOM-like tree. Always resets *doc first. Tolerant: returns
 * CAPY_DOM_OK except for a NULL argument (CAPY_DOM_ERR_NULL). Arena exhaustion
 * sets doc->truncated and the matching warning rather than failing.
 */
int capy_html_parse(const char *html, size_t html_len, struct capy_dom_doc *doc);

/* Accessors (also usable by M2/M3 and tests). */
const struct capy_dom_node *capy_dom_node_at(const struct capy_dom_doc *doc,
                                             size_t index);
const char *capy_dom_string(const struct capy_dom_doc *doc, size_t off);

/* Find an attribute by (lower-case) name on an element node. Returns 1 and sets
 * value_off/value_len on success, 0 otherwise. */
int capy_dom_find_attr(const struct capy_dom_doc *doc,
                       const struct capy_dom_node *node, const char *name,
                       size_t *value_off, size_t *value_len);

const char *capy_dom_warning_name(enum capy_dom_warning w);

#endif /* CAPY_DOM_H */
