#ifndef CAPY_PAGE_RENDER_H
#define CAPY_PAGE_RENDER_H

/*
 * capy-browser-core: production static-page pipeline.
 *
 * `capy_page_render` is the single integration entry point for the graphical
 * static engine. It composes the owned pure stages in their required order:
 *
 *   HTML -> DOM -> CSS -> cascade -> layout -> display-list
 *
 * The caller owns one `struct capy_page` arena. The function resets and fills
 * it without allocation; no cleanup call is required. All stage products stay
 * valid until that arena is reused. In particular, computed-style offsets
 * refer to `page->stylesheet.strings`, which is owned by the same arena.
 * Because the representation uses indices/offsets rather than internal
 * pointers, the complete structure may be copied as a unit. Individual members
 * must not be detached from their owning page while still in use.
 *
 * The structure is intentionally large. Kernel/embedded integrations should
 * place it in integration-owned heap/static storage, not a small task stack.
 */

#include <stddef.h>

#include "cascade.h"
#include "css_parse.h"
#include "display_list.h"
#include "dom.h"
#include "layout.h"

enum capy_page_status {
  CAPY_PAGE_OK = 0,
  CAPY_PAGE_ERR_NULL = -1,
  CAPY_PAGE_ERR_HTML = -2,
  CAPY_PAGE_ERR_CSS = -3,
  CAPY_PAGE_ERR_CASCADE = -4,
  CAPY_PAGE_ERR_LAYOUT = -5,
  CAPY_PAGE_ERR_DISPLAY_LIST = -6
};

/* Last successfully completed stage. On CAPY_PAGE_OK this is DISPLAY_LIST. */
enum capy_page_stage {
  CAPY_PAGE_STAGE_NONE = 0,
  CAPY_PAGE_STAGE_HTML = 1,
  CAPY_PAGE_STAGE_CSS = 2,
  CAPY_PAGE_STAGE_CASCADE = 3,
  CAPY_PAGE_STAGE_LAYOUT = 4,
  CAPY_PAGE_STAGE_DISPLAY_LIST = 5
};

/* Page-level policy warnings. Detailed parse/budget warnings remain available
 * on the corresponding owned stage product. Values are additive. */
enum capy_page_warning {
  CAPY_PAGE_WARN_SCRIPT_BLOCKED = 1 /* script was present and not executed */
};

#define CAPY_PAGE_WARN_MAX 8u

struct capy_page_warnings {
  enum capy_page_warning codes[CAPY_PAGE_WARN_MAX];
  size_t count;
};

struct capy_page {
  struct capy_dom_doc dom;
  struct capy_css_stylesheet stylesheet;
  struct capy_css_cascade cascade;
  struct capy_layout_tree layout;
  struct capy_dl display_list;

  enum capy_page_stage completed_stage;
  int script_present; /* 1 iff script markup was found and blocked */
  int truncated;      /* 1 iff any owned stage exhausted a declared budget */
  struct capy_page_warnings warnings;
};

/*
 * Render one static page into caller-owned `out`.
 *
 * `html` and `out` are required. `css` may be NULL only when `css_len` is zero,
 * which selects an empty stylesheet. `base_url` may be NULL; unresolved
 * relative resource/link URLs are then omitted by the display-list stage.
 * `viewport_width` is expressed in layout cells and is clamped to at least 1.
 *
 * The returned status identifies the first failed stage. Products through
 * `out->completed_stage` remain inspectable after a stage error. Tolerant parse
 * and budget exhaustion normally return CAPY_PAGE_OK with detailed warnings
 * and `out->truncated`, rather than a fatal error.
 */
int capy_page_render(const char *html, size_t html_len, const char *css,
                     size_t css_len, const char *base_url,
                     long viewport_width, struct capy_page *out);

const char *capy_page_status_name(enum capy_page_status status);
const char *capy_page_stage_name(enum capy_page_stage stage);
const char *capy_page_warning_name(enum capy_page_warning warning);

#endif /* CAPY_PAGE_RENDER_H */
