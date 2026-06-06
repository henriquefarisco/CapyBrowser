#ifndef CAPY_CSS_CASCADE_H
#define CAPY_CSS_CASCADE_H

/*
 * capy-browser-core: CSS cascade (Fase M2, part 2).
 *
 * Matches the parsed stylesheet (css_parse.h) onto the Fase M1 DOM and computes,
 * for every node, a computed style over a documented subset of properties.
 * Deterministic: for each property the winning declaration is chosen by
 * specificity then source order; inherited properties fall back to the parent.
 *
 * Pure and allocation-free (the caller provides a `struct capy_css_cascade`
 * arena sized to the DOM node pool). Computed values are ranges into the
 * originating stylesheet's string arena (valid while that sheet lives). No
 * network, filesystem, clock or RNG.
 */

#include <stddef.h>

#include "css_parse.h"
#include "dom.h"

/* Known (longhand) property subset the cascade understands. Additive: append
 * new properties before _COUNT; never renumber existing ones. */
enum capy_css_prop {
  CAPY_CSS_PROP_DISPLAY = 0,
  CAPY_CSS_PROP_COLOR = 1,
  CAPY_CSS_PROP_BACKGROUND_COLOR = 2,
  CAPY_CSS_PROP_FONT_WEIGHT = 3,
  CAPY_CSS_PROP_FONT_STYLE = 4,
  CAPY_CSS_PROP_TEXT_ALIGN = 5,
  CAPY_CSS_PROP_TEXT_DECORATION = 6
};

#define CAPY_CSS_PROP_COUNT 7u

enum capy_css_cascade_status {
  CAPY_CSS_CASCADE_OK = 0,
  CAPY_CSS_CASCADE_ERR_NULL = -1
};

/*
 * Computed style for one node: per known property a `set` flag plus a value
 * range into the originating stylesheet's string arena (sheet->strings).
 */
struct capy_css_computed {
  int set[CAPY_CSS_PROP_COUNT];
  size_t value_off[CAPY_CSS_PROP_COUNT];
  size_t value_len[CAPY_CSS_PROP_COUNT];
};

/* One computed style per DOM node, indexed by node index. */
struct capy_css_cascade {
  struct capy_css_computed styles[CAPY_DOM_MAX_NODES];
  size_t node_count;
};

/*
 * Compute the cascaded + inherited style for every DOM node. Always resets
 * *out. Returns CAPY_CSS_CASCADE_OK, or CAPY_CSS_CASCADE_ERR_NULL on a NULL
 * argument. Computed values reference sheet->strings.
 */
int capy_css_cascade(const struct capy_dom_doc *dom,
                     const struct capy_css_stylesheet *sheet,
                     struct capy_css_cascade *out);

/* Stable property name (diagnostics/tests); "unknown" if unrecognized. */
const char *capy_css_prop_name(enum capy_css_prop p);

#endif /* CAPY_CSS_CASCADE_H */
