/* capy-browser-core: production static-page pipeline. */

#include "page_render.h"

#include <string.h>

static int page_dom_has_warning(const struct capy_dom_doc *dom,
                                enum capy_dom_warning warning) {
  size_t i;
  for (i = 0; i < dom->warnings.count; i++) {
    if (dom->warnings.codes[i] == warning) {
      return 1;
    }
  }
  return 0;
}

/* Rebuild page-level metadata from the completed owned stages. */
static void page_refresh_metadata(struct capy_page *page) {
  page->script_present =
      page_dom_has_warning(&page->dom, CAPY_DOM_WARN_SCRIPT_BLOCKED);
  page->truncated = page->dom.truncated || page->stylesheet.truncated ||
                    page->layout.truncated || page->display_list.truncated;
  page->warnings.count = 0;
  if (page->script_present) {
    page->warnings.codes[page->warnings.count++] =
        CAPY_PAGE_WARN_SCRIPT_BLOCKED;
  }
}

int capy_page_render(const char *html, size_t html_len, const char *css,
                     size_t css_len, const char *base_url,
                     long viewport_width, struct capy_page *out) {
  static const char empty_css[] = "";

  if (out == NULL) {
    return CAPY_PAGE_ERR_NULL;
  }
  memset(out, 0, sizeof(*out));
  out->completed_stage = CAPY_PAGE_STAGE_NONE;

  if (html == NULL || (css == NULL && css_len != 0)) {
    return CAPY_PAGE_ERR_NULL;
  }
  if (css == NULL) {
    css = empty_css;
  }

  if (capy_html_parse(html, html_len, &out->dom) != CAPY_DOM_OK) {
    page_refresh_metadata(out);
    return CAPY_PAGE_ERR_HTML;
  }
  out->completed_stage = CAPY_PAGE_STAGE_HTML;

  if (capy_css_parse(css, css_len, &out->stylesheet) != CAPY_CSS_OK) {
    page_refresh_metadata(out);
    return CAPY_PAGE_ERR_CSS;
  }
  out->completed_stage = CAPY_PAGE_STAGE_CSS;

  if (capy_css_cascade(&out->dom, &out->stylesheet, &out->cascade) !=
      CAPY_CSS_CASCADE_OK) {
    page_refresh_metadata(out);
    return CAPY_PAGE_ERR_CASCADE;
  }
  out->completed_stage = CAPY_PAGE_STAGE_CASCADE;

  if (capy_layout(&out->dom, &out->stylesheet, &out->cascade, viewport_width,
                  &out->layout) != CAPY_LAYOUT_OK) {
    page_refresh_metadata(out);
    return CAPY_PAGE_ERR_LAYOUT;
  }
  out->completed_stage = CAPY_PAGE_STAGE_LAYOUT;

  if (capy_displaylist(&out->dom, &out->stylesheet, &out->cascade,
                       &out->layout, base_url, &out->display_list) !=
      CAPY_DL_OK) {
    page_refresh_metadata(out);
    return CAPY_PAGE_ERR_DISPLAY_LIST;
  }
  out->completed_stage = CAPY_PAGE_STAGE_DISPLAY_LIST;
  page_refresh_metadata(out);
  return CAPY_PAGE_OK;
}

const char *capy_page_status_name(enum capy_page_status status) {
  switch (status) {
    case CAPY_PAGE_OK:
      return "OK";
    case CAPY_PAGE_ERR_NULL:
      return "NULL";
    case CAPY_PAGE_ERR_HTML:
      return "HTML";
    case CAPY_PAGE_ERR_CSS:
      return "CSS";
    case CAPY_PAGE_ERR_CASCADE:
      return "CASCADE";
    case CAPY_PAGE_ERR_LAYOUT:
      return "LAYOUT";
    case CAPY_PAGE_ERR_DISPLAY_LIST:
      return "DISPLAY_LIST";
  }
  return "UNKNOWN";
}

const char *capy_page_stage_name(enum capy_page_stage stage) {
  switch (stage) {
    case CAPY_PAGE_STAGE_NONE:
      return "NONE";
    case CAPY_PAGE_STAGE_HTML:
      return "HTML";
    case CAPY_PAGE_STAGE_CSS:
      return "CSS";
    case CAPY_PAGE_STAGE_CASCADE:
      return "CASCADE";
    case CAPY_PAGE_STAGE_LAYOUT:
      return "LAYOUT";
    case CAPY_PAGE_STAGE_DISPLAY_LIST:
      return "DISPLAY_LIST";
  }
  return "UNKNOWN";
}

const char *capy_page_warning_name(enum capy_page_warning warning) {
  switch (warning) {
    case CAPY_PAGE_WARN_SCRIPT_BLOCKED:
      return "SCRIPT_BLOCKED";
  }
  return "UNKNOWN";
}
