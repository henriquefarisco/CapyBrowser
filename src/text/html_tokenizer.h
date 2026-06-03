#ifndef CAPY_HTML_TOKENIZER_H
#define CAPY_HTML_TOKENIZER_H

/*
 * Tolerant, deterministic HTML tokenizer. It never aborts on malformed input;
 * it recovers and flags the recovery via per-token flags. Designed to be
 * reused by the future DOM parser (Fase M1); for Fase C2 it captures only the
 * `href` attribute (full attribute lists arrive with the DOM parser).
 */

#include <stddef.h>

#include "url_parse.h" /* CAPY_URL_MAX_LEN */

#define CAPY_HTML_TAG_MAX 32u
#define CAPY_HTML_MAX_ATTRS 32u

/* An attribute as spans into the tokenizer input (not owned, valid for the
 * lifetime of the parse). Raw bytes; entities are decoded by the consumer. */
struct capy_html_attr {
  const char *name;
  size_t name_len;
  const char *value; /* NULL when has_value == 0 */
  size_t value_len;
  int has_value;
};

enum capy_html_token_type {
  CAPY_HTML_TOKEN_EOF = 0,
  CAPY_HTML_TOKEN_TEXT,    /* raw (undecoded) text run */
  CAPY_HTML_TOKEN_START,   /* <name ...> */
  CAPY_HTML_TOKEN_END,     /* </name> */
  CAPY_HTML_TOKEN_COMMENT, /* <!-- ... --> */
  CAPY_HTML_TOKEN_DECL     /* <!DOCTYPE ...> and other <! ... > declarations */
};

struct capy_html_token {
  enum capy_html_token_type type;
  const char *text; /* TEXT: span into the tokenizer input (not owned) */
  size_t text_len;
  char name[CAPY_HTML_TAG_MAX + 1]; /* START/END: lower-cased tag name */
  int self_closing;
  int has_href;                    /* START: an href value was captured */
  char href[CAPY_URL_MAX_LEN + 1]; /* START: raw href (entities undecoded) */
  struct capy_html_attr attrs[CAPY_HTML_MAX_ATTRS]; /* START: all attributes */
  size_t attr_count;               /* number of attrs recorded (<= MAX_ATTRS) */
  int unclosed_tag;                /* tag ran to end-of-input without '>' */
  int unclosed_comment;            /* comment ran to end-of-input without --> */
};

struct capy_html_tokenizer {
  const char *input;
  size_t len;
  size_t pos;
  char raw_end[CAPY_HTML_TAG_MAX + 1]; /* rawtext close-tag name (lower-case) */
  int in_raw;                          /* inside script/style/title/textarea */
};

void capy_html_tokenizer_init(struct capy_html_tokenizer *tk, const char *input,
                              size_t len);

/* Produce the next token. Returns 1 if a token was produced, 0 at EOF. */
int capy_html_tokenizer_next(struct capy_html_tokenizer *tk,
                             struct capy_html_token *tok);

#endif /* CAPY_HTML_TOKENIZER_H */
