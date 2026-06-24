#ifndef CAPY_DISPLAY_LIST_H
#define CAPY_DISPLAY_LIST_H

/*
 * capy-browser-core: display-list emitter (Fase M3, part b).
 *
 * Walks the Fase M3a box tree (with M2 computed styles and the M1 DOM) and
 * emits a flat, ordered, versioned list of draw nodes that a compositor (the
 * CapyOS render backend, later) consumes -- text runs, background rectangles,
 * image placeholders and link bounds (with URLs resolved through the Fase C1
 * URL core), plus the scroll extent. There is no compositor-specific
 * assumption; the list is pure data.
 *
 * Versioned and additive: `version` is CAPY_DL_VERSION; new node kinds, node
 * fields and warnings are appended, never repurposed. Deterministic: the same
 * `(DOM, stylesheet, computed styles, box tree, base URL)` yields the same node
 * sequence, geometry and strings. Pure, allocation-free (caller-provided
 * arena), fail-closed (node/string budgets). No network, filesystem, clock,
 * RNG or image decode (placeholders only).
 */

#include <stddef.h>

#include "cascade.h"
#include "css_parse.h"
#include "dom.h"
#include "layout.h"

#define CAPY_DL_VERSION 1

/* Arena capacities (alpha; configurable per integration stage). */
#define CAPY_DL_MAX_NODES 4096u
#define CAPY_DL_STRING_ARENA 65536u

enum capy_dl_status {
  CAPY_DL_OK = 0,
  CAPY_DL_ERR_NULL = -1
};

/* Node kinds. Additive: append new kinds; never renumber. */
enum capy_dl_node_kind {
  CAPY_DL_TEXT = 0,  /* a run of text */
  CAPY_DL_RECT = 1,  /* a filled rectangle (e.g. a background) */
  CAPY_DL_IMAGE = 2, /* an image: alt label + resolved src URL (no decode here) */
  CAPY_DL_LINK = 3   /* a link bound with a resolved absolute URL */
};

enum capy_dl_warning {
  CAPY_DL_WARN_NODE_BUDGET = 1,  /* node pool full */
  CAPY_DL_WARN_STRING_BUDGET = 2 /* string arena full */
};

#define CAPY_DL_WARN_MAX 8u

struct capy_dl_warnings {
  enum capy_dl_warning codes[CAPY_DL_WARN_MAX];
  size_t count;
};

/*
 * A draw node. Geometry is in layout cells (top-left origin). String payloads
 * are ranges into dl->strings; an unused payload has a zero length. Per kind:
 *   TEXT  -> text (run bytes), color (if the node has a computed color);
 *   RECT  -> color (the fill);
 *   IMAGE -> label (the alt / a11y text, if any), url (the resolved absolute
 *            src URL, if it resolves -- lets the consumer fetch/decode it);
 *   LINK  -> url (the resolved absolute URL).
 */
struct capy_dl_node {
  enum capy_dl_node_kind kind;
  long x;
  long y;
  long width;
  long height;
  size_t text_off;
  size_t text_len;
  size_t color_off;
  size_t color_len;
  size_t url_off;
  size_t url_len;
  size_t label_off;
  size_t label_len;
};

struct capy_dl {
  int version;
  struct capy_dl_node nodes[CAPY_DL_MAX_NODES];
  size_t node_count;
  char strings[CAPY_DL_STRING_ARENA];
  size_t string_len;
  long content_width;  /* scroll extent (viewport width) */
  long content_height; /* scroll extent (laid-out height) */
  int truncated;
  struct capy_dl_warnings warnings;
};

/*
 * Emit the display list for a laid-out, styled document. base_url (may be NULL)
 * resolves link hrefs through Fase C1; an href that cannot resolve to an
 * absolute URL is dropped (no LINK node). Always resets *out. Returns
 * CAPY_DL_OK, or CAPY_DL_ERR_NULL on a NULL argument. Budget exhaustion sets
 * out->truncated and the matching warning.
 */
int capy_displaylist(const struct capy_dom_doc *dom,
                     const struct capy_css_stylesheet *sheet,
                     const struct capy_css_cascade *casc,
                     const struct capy_layout_tree *layout, const char *base_url,
                     struct capy_dl *out);

const char *capy_dl_node_kind_name(enum capy_dl_node_kind k);
const char *capy_dl_warning_name(enum capy_dl_warning w);

#endif /* CAPY_DISPLAY_LIST_H */
