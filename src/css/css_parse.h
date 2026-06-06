#ifndef CAPY_CSS_PARSE_H
#define CAPY_CSS_PARSE_H

/*
 * capy-browser-core: tolerant CSS parser (Fase M2, part 1).
 *
 * Parses a CSS text into a deterministic stylesheet of rules, each a simple
 * selector plus a list of property:value declarations. It is the input to the
 * cascade (Fase M2, part 2), which matches rules onto the Fase M1 DOM.
 *
 * Like the DOM, it is pure, allocation-free (the caller provides a
 * `struct capy_css_stylesheet` arena), tolerant (never aborts on malformed
 * input; recovers with deterministic, once-each warnings) and fail-closed
 * (arena/rule/declaration budgets cap memory). No network, filesystem, clock
 * or RNG.
 *
 * Supported selector subset (documented in docs/compatibility.md): a single
 * simple selector per comma-separated entry -- universal (`*`), type (`tag`),
 * class (`.name`) or id (`#name`). Compound, descendant, combinator, attribute
 * and pseudo selectors are dropped with CAPY_CSS_WARN_SELECTOR_SKIPPED.
 * At-rules (`@media`, ...) are skipped with CAPY_CSS_WARN_AT_RULE_SKIPPED.
 */

#include <stddef.h>

/* Arena capacities (alpha; configurable per integration stage). */
#define CAPY_CSS_MAX_INPUT (256u * 1024u)
#define CAPY_CSS_MAX_RULES 256u
#define CAPY_CSS_MAX_DECLS 1024u
#define CAPY_CSS_STRING_ARENA 32768u

enum capy_css_status {
  CAPY_CSS_OK = 0,
  CAPY_CSS_ERR_NULL = -1
};

enum capy_css_selector_kind {
  CAPY_CSS_SEL_UNIVERSAL = 0, /* *      */
  CAPY_CSS_SEL_TYPE = 1,      /* tag    */
  CAPY_CSS_SEL_CLASS = 2,     /* .name  */
  CAPY_CSS_SEL_ID = 3         /* #name  */
};

/* Deterministic warnings, recorded once each in first-occurrence order. */
enum capy_css_warning {
  CAPY_CSS_WARN_INPUT_TRUNCATED = 1,  /* CSS exceeded the input budget */
  CAPY_CSS_WARN_RULE_BUDGET = 2,      /* rule pool full */
  CAPY_CSS_WARN_DECL_BUDGET = 3,      /* declaration pool full */
  CAPY_CSS_WARN_STRING_BUDGET = 4,    /* string arena full */
  CAPY_CSS_WARN_AT_RULE_SKIPPED = 5,  /* an @-rule was skipped */
  CAPY_CSS_WARN_SELECTOR_SKIPPED = 6, /* an unsupported selector was dropped */
  CAPY_CSS_WARN_DECL_SKIPPED = 7,     /* a malformed declaration was dropped */
  CAPY_CSS_WARN_UNCLOSED_BLOCK = 8,   /* a `{` block ran to end-of-input */
  CAPY_CSS_WARN_UNCLOSED_COMMENT = 9  /* a comment ran to end-of-input */
};

#define CAPY_CSS_WARN_MAX 16u

struct capy_css_warnings {
  enum capy_css_warning codes[CAPY_CSS_WARN_MAX];
  size_t count;
};

/* A declaration: property name (lower-cased) + value (raw, end-trimmed), both
 * ranges into sheet->strings. */
struct capy_css_decl {
  size_t prop_off;
  size_t prop_len;
  size_t value_off;
  size_t value_len;
};

/* A simple selector. name is a range into sheet->strings (empty for the
 * universal selector; lower-cased for type selectors, case-preserved fo
 * class/id to match HTML class/id values). */
struct capy_css_selector {
  enum capy_css_selector_kind kind;
  size_t name_off;
  size_t name_len;
};

/* A qualified rule: one selector + a contiguous range of declarations. Comma
 * lists are expanded so each rule carries exactly one selector; the rules
 * keep source order, which the cascade uses for tie-breaking. */
struct capy_css_rule {
  struct capy_css_selector selector;
  size_t decl_start;
  size_t decl_count;
};

struct capy_css_stylesheet {
  struct capy_css_rule rules[CAPY_CSS_MAX_RULES];
  size_t rule_count;
  struct capy_css_decl decls[CAPY_CSS_MAX_DECLS];
  size_t decl_count;
  char strings[CAPY_CSS_STRING_ARENA];
  size_t string_len;
  int truncated;
  struct capy_css_warnings warnings;
};

/*
 * Parse CSS into a stylesheet. Always resets *out first. Tolerant: returns
 * CAPY_CSS_OK except for a NULL argument (CAPY_CSS_ERR_NULL). Budget exhaustion
 * sets out->truncated and the matching warning rather than failing.
 */
int capy_css_parse(const char *css, size_t css_len,
                   struct capy_css_stylesheet *out);

/* Accessors (also usable by the cascade and tests). */
const char *capy_css_string(const struct capy_css_stylesheet *sheet,
                            size_t off);
const char *capy_css_selector_kind_name(enum capy_css_selector_kind k);
const char *capy_css_warning_name(enum capy_css_warning w);

#endif /* CAPY_CSS_PARSE_H */
