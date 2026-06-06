#ifndef CAPY_LAYOUT_H
#define CAPY_LAYOUT_H

/*
 * capy-browser-core: static block layout (Fase M3, part a).
 *
 * Consumes the Fase M1 DOM and the Fase M2 computed styles and produces a box
 * tree with geometry, the input to the display-list emitter (Fase M3, part b).
 *
 * This is a deliberately simple first layout: a vertical block flow. Every
 * rendered element becomes a block box that stacks its children (child element
 * boxes and text line-boxes) vertically and spans the parent's content width;
 * `display: none` (from the cascade) removes an element and its subtree. Text
 * nodes collapse ASCII whitespace and greedy-wrap to the content width, each
 * becoming a text box whose height is its line count. There is no inline flow,
 * margins/padding/borders or float/positioning yet (additive later).
 *
 * Geometry is in abstract cells (1 column wide, 1 line tall) -- a monospace
 * approximation that is deterministic and font-independent; real pixel metrics
 * arrive with a font backend. Pure, allocation-free (caller-provided arena),
 * fail-closed (box/depth budgets). No network, filesystem, clock or RNG.
 */

#include <stddef.h>

#include "cascade.h"
#include "css_parse.h"
#include "dom.h"

/* Arena capacities (alpha; configurable per integration stage). */
#define CAPY_LAYOUT_MAX_BOXES 2048u
#define CAPY_LAYOUT_MAX_DEPTH 128u

/* Sentinel "no box" index. */
#define CAPY_LAYOUT_NONE ((size_t)-1)

enum capy_layout_status {
  CAPY_LAYOUT_OK = 0,
  CAPY_LAYOUT_ERR_NULL = -1
};

enum capy_layout_box_kind {
  CAPY_LAYOUT_BLOCK = 0, /* from an element */
  CAPY_LAYOUT_TEXT = 1   /* from a text node (one or more wrapped lines) */
};

enum capy_layout_warning {
  CAPY_LAYOUT_WARN_BOX_BUDGET = 1, /* box pool full */
  CAPY_LAYOUT_WARN_DEPTH_LIMIT = 2 /* nesting exceeded CAPY_LAYOUT_MAX_DEPTH */
};

#define CAPY_LAYOUT_WARN_MAX 8u

struct capy_layout_warnings {
  enum capy_layout_warning codes[CAPY_LAYOUT_WARN_MAX];
  size_t count;
};

/* A laid-out box. Geometry is in cells; (x,y) is the top-left, content-box. */
struct capy_layout_box {
  enum capy_layout_box_kind kind;
  size_t dom_node; /* source DOM node index (element for BLOCK, text for TEXT) */
  long x;
  long y;
  long width;
  long height;
  size_t parent;
  size_t first_child;
  size_t last_child;
  size_t next_sibling;
};

struct capy_layout_tree {
  struct capy_layout_box boxes[CAPY_LAYOUT_MAX_BOXES];
  size_t box_count;
  size_t root; /* synthetic viewport box (maps to the DOM document node) */
  long viewport_width;
  long content_height;
  int truncated;
  struct capy_layout_warnings warnings;
};

/*
 * Lay out the styled document into *out. Always resets *out. viewport_width is
 * the available content width in cells (clamped to >= 1). Returns
 * CAPY_LAYOUT_OK, or CAPY_LAYOUT_ERR_NULL on a NULL argument. Budget exhaustion
 * sets out->truncated and the matching warning rather than failing.
 */
int capy_layout(const struct capy_dom_doc *dom,
                const struct capy_css_stylesheet *sheet,
                const struct capy_css_cascade *casc, long viewport_width,
                struct capy_layout_tree *out);

const char *capy_layout_warning_name(enum capy_layout_warning w);

#endif /* CAPY_LAYOUT_H */
