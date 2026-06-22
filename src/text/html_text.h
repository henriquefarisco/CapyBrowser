#ifndef CAPY_HTML_TEXT_H
#define CAPY_HTML_TEXT_H

/*
 * capy-browser-core: HTML-to-text surface ("CapyBrowse Text", Fase C2).
 *
 * Turns an HTML document into the deterministic text view a user reads in text
 * mode: a title, normalized body blocks, and numbered links whose targets are
 * resolved against a base URL through the Fase C1 URL core.
 *
 * Guarantees:
 *   - pure and deterministic: same (html, base) -> same body, title, links and
 *     warning set; no network, filesystem, clock or RNG;
 *   - tolerant: malformed HTML never aborts; it recovers with warnings;
 *   - fail-closed limits: input/output/link/title budgets are bounded;
 *   - clean output: the body and titles contain no control bytes other than the
 *     block separator '\n' (CRLF, tabs and other controls are normalized away);
 *   - no scripting: <script>/<style> content is dropped (JS stays blocked).
 */

#include <stddef.h>
#include <stdint.h>

#include "url_parse.h" /* CAPY_URL_MAX_LEN, capy_url_parse */

/* Resource limits (alpha targets; configurable per integration stage). */
#define CAPY_TEXT_MAX_INPUT (256u * 1024u) /* 256 KiB HTML input */
#define CAPY_TEXT_TITLE_MAX 256u
#define CAPY_TEXT_MAX_LINKS 64u
#define CAPY_TEXT_LINK_TEXT_MAX 160u

/* Return codes. 0 on success; tolerant parsing rarely fails (it warns). */
enum capy_text_status {
  CAPY_TEXT_OK = 0,
  CAPY_TEXT_ERR_NULL = -1 /* NULL html or out pointer */
};

/* Deterministic warnings, emitted once each in fixed canonical (enum) order. */
enum capy_text_warning {
  CAPY_TEXT_WARN_INPUT_TRUNCATED = 1,  /* HTML exceeded CAPY_TEXT_MAX_INPUT */
  CAPY_TEXT_WARN_OUTPUT_TRUNCATED = 2, /* body did not fit the caller buffer */
  CAPY_TEXT_WARN_TITLE_TRUNCATED = 3,  /* <title> exceeded CAPY_TEXT_TITLE_MAX */
  CAPY_TEXT_WARN_LINK_BUDGET = 4,      /* more than CAPY_TEXT_MAX_LINKS links */
  CAPY_TEXT_WARN_LINK_UNRESOLVED = 5,  /* an href could not be resolved */
  CAPY_TEXT_WARN_ENTITY_INVALID = 6,   /* an entity decoded to a control byte */
  CAPY_TEXT_WARN_UNCLOSED_TAG = 7,     /* a tag ran to end-of-input */
  CAPY_TEXT_WARN_UNCLOSED_COMMENT = 8  /* a comment ran to end-of-input */
};

#define CAPY_TEXT_WARN_MAX 16u

struct capy_text_warnings {
  enum capy_text_warning codes[CAPY_TEXT_WARN_MAX];
  size_t count;
};

/* A numbered link. The number is the 1-based index into the links array. */
struct capy_text_link {
  char url[CAPY_URL_MAX_LEN + 1]; /* resolved, normalized absolute URL */
  char text[CAPY_TEXT_LINK_TEXT_MAX + 1]; /* trimmed UTF-8 anchor label, "" if none */
};

struct capy_text_doc {
  char title[CAPY_TEXT_TITLE_MAX + 1];
  int has_title;
  size_t text_len; /* bytes written to the caller-provided body buffer */
  struct capy_text_link links[CAPY_TEXT_MAX_LINKS];
  size_t link_count;
  int truncated; /* 1 if any input/output/link/title budget was hit */
  struct capy_text_warnings warnings;
};

/*
 * Render HTML into the CapyBrowse Text view.
 *
 *   html / html_len : input bytes (UTF-8). Not required to be NUL-terminated.
 *   base_url        : absolute base for resolving relative links; may be NULL
 *                     (then only already-absolute links resolve).
 *   text_buf/text_cap: caller buffer for the normalized body (NUL-terminated on
 *                     return). Inline links appear as "[n]" markers.
 *   out             : receives title, link targets, warnings and truncation.
 *
 * Returns CAPY_TEXT_OK or CAPY_TEXT_ERR_NULL. Always resets *out first.
 */
int capy_html_to_text(const uint8_t *html, size_t html_len, const char *base_url,
                      char *text_buf, size_t text_cap, struct capy_text_doc *out);

/* Stable name for a warning code (fixtures/logs); "UNKNOWN" if unrecognized. */
const char *capy_text_warning_name(enum capy_text_warning w);

#endif /* CAPY_HTML_TEXT_H */
