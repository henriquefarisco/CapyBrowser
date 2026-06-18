#include "html_text.h"

#include <string.h>

#include "html_entities.h"
#include "html_tokenizer.h"

#define CAPY_TEXT_LIST_NEST_MAX 8

struct emit_state {
  char *buf;
  size_t cap;
  size_t len;
  int line_has_content;
  int pending_space;
  int pending_newline;
  struct capy_text_doc *doc;

  int in_skip;     /* inside <script>/<style> */
  int in_pre;      /* inside <pre>: preserve spaces, tabs and newlines */
  int pre_lead;    /* pending strip of a single leading newline after <pre> */
  int in_title;    /* inside the first <title> */
  int title_seen;  /* a <title> was already captured */
  size_t title_len;
  int title_has_content;
  int title_pending_space;

  int list_depth;                    /* open <ul>/<ol> nesting depth */
  struct {
    int ordered;                     /* 1 for <ol>, 0 for <ul> */
    unsigned int counter;            /* 1-based item index for <ol> */
  } list_stack[CAPY_TEXT_LIST_NEST_MAX];

  int cur_link_pending;              /* inside an <a> whose href resolved */
  char cur_url[CAPY_URL_MAX_LEN + 1]; /* resolved URL, committed on </a> */

  /* deterministic warning flags */
  int w_input_trunc;
  int w_output_trunc;
  int w_title_trunc;
  int w_link_budget;
  int w_link_unresolved;
  int w_entity_invalid;
  int w_unclosed_tag;
  int w_unclosed_comment;
};

static void capy_text_warn_push(struct capy_text_warnings *w,
                                enum capy_text_warning code) {
  if (w->count < CAPY_TEXT_WARN_MAX) {
    w->codes[w->count] = code;
    w->count += 1;
  }
}

const char *capy_text_warning_name(enum capy_text_warning w) {
  switch (w) {
    case CAPY_TEXT_WARN_INPUT_TRUNCATED:
      return "INPUT_TRUNCATED";
    case CAPY_TEXT_WARN_OUTPUT_TRUNCATED:
      return "OUTPUT_TRUNCATED";
    case CAPY_TEXT_WARN_TITLE_TRUNCATED:
      return "TITLE_TRUNCATED";
    case CAPY_TEXT_WARN_LINK_BUDGET:
      return "LINK_BUDGET";
    case CAPY_TEXT_WARN_LINK_UNRESOLVED:
      return "LINK_UNRESOLVED";
    case CAPY_TEXT_WARN_ENTITY_INVALID:
      return "ENTITY_INVALID";
    case CAPY_TEXT_WARN_UNCLOSED_TAG:
      return "UNCLOSED_TAG";
    case CAPY_TEXT_WARN_UNCLOSED_COMMENT:
      return "UNCLOSED_COMMENT";
  }
  return "UNKNOWN";
}

static int out_raw_byte(struct emit_state *st, char c) {
  if (!st->buf || st->len + 1 >= st->cap) {
    st->w_output_trunc = 1;
    return 0;
  }
  st->buf[st->len++] = c;
  return 1;
}

static void body_emit_char(struct emit_state *st, char c) {
  if (st->pending_newline) {
    if (!out_raw_byte(st, '\n')) {
      return;
    }
    st->pending_newline = 0;
    st->pending_space = 0;
  } else if (st->pending_space && st->line_has_content) {
    if (!out_raw_byte(st, ' ')) {
      return;
    }
  }
  st->pending_space = 0;
  if (out_raw_byte(st, c)) {
    st->line_has_content = 1;
  }
}

static void body_space(struct emit_state *st) { st->pending_space = 1; }

static void body_flush_block(struct emit_state *st) {
  if (st->line_has_content) {
    st->pending_newline = 1;
    st->line_has_content = 0;
  }
  st->pending_space = 0;
}

static int cp_is_space(uint32_t cp) {
  return cp == 0x20u || cp == 0x09u || cp == 0x0Au || cp == 0x0Du ||
         cp == 0x0Cu || cp == 0xA0u;
}

static void body_emit_codepoint(struct emit_state *st, uint32_t cp) {
  char u[4];
  size_t k;
  size_t j;
  if (cp == CAPY_CP_INVALID) {
    /* WHATWG: a NULL / surrogate / out-of-range numeric reference resolves to
       U+FFFD REPLACEMENT CHARACTER, still flagged as a parse error. */
    st->w_entity_invalid = 1;
    k = capy_utf8_encode(0xFFFDu, u);
    for (j = 0; j < k; j++) {
      body_emit_char(st, u[j]);
    }
    return;
  }
  if (cp_is_space(cp)) {
    body_space(st);
    return;
  }
  if (cp < 0x20u || cp == 0x7Fu || (cp >= 0x80u && cp <= 0x9Fu)) {
    st->w_entity_invalid = 1;
    return;
  }
  k = capy_utf8_encode(cp, u);
  if (k == 0) {
    st->w_entity_invalid = 1;
    return;
  }
  for (j = 0; j < k; j++) {
    body_emit_char(st, u[j]);
  }
}

/* Emit one literal byte inside a <pre> block, bypassing whitespace
 * collapsing. Honors a pending block newline first (so the preformatted
 * run starts on its own line), then writes the byte verbatim. */
static void body_emit_pre_byte(struct emit_state *st, char c) {
  if (st->pending_newline) {
    if (!out_raw_byte(st, '\n')) {
      return;
    }
    st->pending_newline = 0;
    st->pending_space = 0;
  }
  if (out_raw_byte(st, c)) {
    st->line_has_content = (c != '\n');
  }
}

static void body_emit_text(struct emit_state *st, const char *t, size_t n) {
  size_t i = 0;
  while (i < n) {
    unsigned char c = (unsigned char)t[i];
    if (c == '&') {
      uint32_t cp;
      size_t consumed = capy_html_charref_at(t + i, n - i, &cp);
      if (st->in_pre) {
        st->pre_lead = 0;
      }
      if (consumed > 0) {
        body_emit_codepoint(st, cp);
        i += consumed;
        continue;
      }
      body_emit_char(st, '&');
      i++;
      continue;
    }
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f') {
      if (st->in_pre) {
        /* Preserve preformatted whitespace. CR is dropped so CRLF collapses
           to a single LF; a lone newline right after <pre> is stripped once
           (the HTML "leading newline in pre is ignored" rule). */
        if (c == '\r') {
          i++;
          continue;
        }
        if (c == '\n' || c == '\f') {
          if (st->pre_lead) {
            st->pre_lead = 0;
          } else {
            body_emit_pre_byte(st, '\n');
          }
          i++;
          continue;
        }
        st->pre_lead = 0;
        body_emit_pre_byte(st, (char)c);
        i++;
        continue;
      }
      body_space(st);
      i++;
      continue;
    }
    if (c < 0x20 || c == 0x7F) {
      i++; /* drop stray control bytes */
      continue;
    }
    if (st->in_pre) {
      st->pre_lead = 0;
    }
    body_emit_char(st, (char)c);
    i++;
  }
}

static void title_emit_char(struct emit_state *st, char c) {
  if (st->title_pending_space && st->title_has_content &&
      st->title_len < CAPY_TEXT_TITLE_MAX) {
    st->doc->title[st->title_len++] = ' ';
  }
  st->title_pending_space = 0;
  if (st->title_len < CAPY_TEXT_TITLE_MAX) {
    st->doc->title[st->title_len++] = c;
    st->title_has_content = 1;
  } else {
    st->w_title_trunc = 1;
  }
}

static void title_emit_codepoint(struct emit_state *st, uint32_t cp) {
  char u[4];
  size_t k;
  size_t j;
  if (cp == CAPY_CP_INVALID) {
    /* WHATWG: a NULL / surrogate / out-of-range numeric reference resolves to
       U+FFFD REPLACEMENT CHARACTER, still flagged as a parse error. */
    st->w_entity_invalid = 1;
    k = capy_utf8_encode(0xFFFDu, u);
    for (j = 0; j < k; j++) {
      title_emit_char(st, u[j]);
    }
    return;
  }
  if (cp_is_space(cp)) {
    st->title_pending_space = 1;
    return;
  }
  if (cp < 0x20u || cp == 0x7Fu || (cp >= 0x80u && cp <= 0x9Fu)) {
    st->w_entity_invalid = 1;
    return;
  }
  k = capy_utf8_encode(cp, u);
  if (k == 0) {
    st->w_entity_invalid = 1;
    return;
  }
  for (j = 0; j < k; j++) {
    title_emit_char(st, u[j]);
  }
}

static void title_emit_text(struct emit_state *st, const char *t, size_t n) {
  size_t i = 0;
  while (i < n) {
    unsigned char c = (unsigned char)t[i];
    if (c == '&') {
      uint32_t cp;
      size_t consumed = capy_html_charref_at(t + i, n - i, &cp);
      if (consumed > 0) {
        title_emit_codepoint(st, cp);
        i += consumed;
        continue;
      }
      title_emit_char(st, '&');
      i++;
      continue;
    }
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f') {
      st->title_pending_space = 1;
      i++;
      continue;
    }
    if (c < 0x20 || c == 0x7F) {
      i++;
      continue;
    }
    title_emit_char(st, (char)c);
    i++;
  }
}

/* Decode entities in a raw href into dst (no whitespace handling). Returns the
 * length, or -1 on overflow. */
static int decode_href(const char *src, char *dst, size_t dstcap) {
  size_t i = 0;
  size_t o = 0;
  size_t n = strlen(src);
  while (i < n) {
    if (src[i] == '&') {
      uint32_t cp;
      size_t consumed = capy_html_charref_at(src + i, n - i, &cp);
      if (consumed > 0 && cp != CAPY_CP_INVALID) {
        char u[4];
        size_t k = capy_utf8_encode(cp, u);
        size_t j;
        if (k == 0) {
          if (o + 1 >= dstcap) {
            return -1;
          }
          dst[o++] = '&';
          i++;
          continue;
        }
        for (j = 0; j < k; j++) {
          if (o + 1 >= dstcap) {
            return -1;
          }
          dst[o++] = u[j];
        }
        i += consumed;
        continue;
      }
    }
    if (o + 1 >= dstcap) {
      return -1;
    }
    dst[o++] = src[i];
    i++;
  }
  dst[o] = '\0';
  return (int)o;
}

static int is_block_element(const char *n) {
  static const char *const blocks[] = {
      "p",      "div",     "h1",     "h2",       "h3",      "h4",
      "h5",     "h6",      "ul",     "ol",       "li",      "dl",
      "dt",     "dd",      "table",  "tr",       "td",      "th",
      "thead",  "tbody",   "tfoot",  "caption",  "blockquote", "pre",
      "section","article", "header", "footer",   "nav",     "aside",
      "main",   "figure",  "figcaption", "form", "fieldset", "address",
      "hr",     "br"};
  size_t i;
  for (i = 0; i < sizeof(blocks) / sizeof(blocks[0]); i++) {
    if (strcmp(n, blocks[i]) == 0) {
      return 1;
    }
  }
  return 0;
}

static void emit_link_marker(struct emit_state *st, int num) {
  char tmp[8];
  int k = 0;
  body_emit_char(st, '[');
  if (num <= 0) {
    body_emit_char(st, '0');
  } else {
    while (num > 0 && k < (int)sizeof(tmp)) {
      tmp[k++] = (char)('0' + (num % 10));
      num /= 10;
    }
    while (k > 0) {
      k--;
      body_emit_char(st, tmp[k]);
    }
  }
  body_emit_char(st, ']');
}

static void handle_anchor_start(struct emit_state *st,
                                const struct capy_html_token *tok,
                                const char *base_url) {
  char decoded[CAPY_URL_MAX_LEN + 1];
  struct capy_url u;
  st->cur_link_pending = 0;
  if (!tok->has_href) {
    return;
  }
  if (st->doc->link_count >= CAPY_TEXT_MAX_LINKS) {
    st->w_link_budget = 1;
    return;
  }
  if (decode_href(tok->href, decoded, sizeof(decoded)) < 0 ||
      capy_url_parse(decoded, base_url, &u, NULL) != CAPY_URL_OK ||
      capy_url_serialize(&u, st->cur_url, sizeof(st->cur_url)) < 0) {
    st->w_link_unresolved = 1;
    return;
  }
  st->cur_link_pending = 1; /* committed to links[] only when </a> closes */
}

static void list_push(struct emit_state *st, int ordered) {
  if (st->list_depth < CAPY_TEXT_LIST_NEST_MAX) {
    st->list_stack[st->list_depth].ordered = ordered;
    st->list_stack[st->list_depth].counter = 0u;
  }
  if (st->list_depth < 0x7fff) {
    st->list_depth += 1;
  }
}

static void list_pop(struct emit_state *st) {
  if (st->list_depth > 0) {
    st->list_depth -= 1;
  }
}

/* Emit the leading marker for a <li>: "- " for an unordered list, "N. " for
 * an ordered one (1-based, per innermost list), indented two spaces per
 * nesting level. A stray <li> with no open list falls back to a bullet. */
static void emit_list_marker(struct emit_state *st) {
  int level = st->list_depth > 0 ? st->list_depth - 1 : 0;
  int ordered = 0;
  unsigned int n = 0u;
  int i;
  if (level > CAPY_TEXT_LIST_NEST_MAX - 1) {
    level = CAPY_TEXT_LIST_NEST_MAX - 1;
  }
  if (st->list_depth > 0 && st->list_depth <= CAPY_TEXT_LIST_NEST_MAX) {
    ordered = st->list_stack[st->list_depth - 1].ordered;
    if (ordered) {
      st->list_stack[st->list_depth - 1].counter += 1u;
      n = st->list_stack[st->list_depth - 1].counter;
    }
  }
  for (i = 0; i < level * 2; i++) {
    body_emit_char(st, ' ');
  }
  if (ordered) {
    char tmp[12];
    int k = 0;
    if (n == 0u) {
      tmp[k++] = '0';
    }
    while (n > 0u && k < (int)sizeof(tmp)) {
      tmp[k++] = (char)('0' + (int)(n % 10u));
      n /= 10u;
    }
    while (k > 0) {
      k--;
      body_emit_char(st, tmp[k]);
    }
    body_emit_char(st, '.');
    body_emit_char(st, ' ');
  } else {
    body_emit_char(st, '-');
    body_emit_char(st, ' ');
  }
}

static void handle_start(struct emit_state *st,
                         const struct capy_html_token *tok,
                         const char *base_url) {
  const char *n = tok->name;
  if (!tok->self_closing &&
      (strcmp(n, "script") == 0 || strcmp(n, "style") == 0)) {
    st->in_skip = 1;
    return;
  }
  if (!tok->self_closing && strcmp(n, "title") == 0) {
    if (!st->title_seen) {
      st->in_title = 1;
      st->title_seen = 1;
    }
    return;
  }
  if (strcmp(n, "a") == 0) {
    handle_anchor_start(st, tok, base_url);
    return;
  }
  if (is_block_element(n)) {
    body_flush_block(st);
    if (!tok->self_closing && strcmp(n, "pre") == 0) {
      st->in_pre = 1;
      st->pre_lead = 1; /* strip a single leading newline inside the block */
    } else if (!tok->self_closing &&
               (strcmp(n, "ul") == 0 || strcmp(n, "ol") == 0)) {
      list_push(st, strcmp(n, "ol") == 0);
    } else if (strcmp(n, "li") == 0) {
      emit_list_marker(st);
    }
  }
}

static void handle_end(struct emit_state *st,
                       const struct capy_html_token *tok) {
  const char *n = tok->name;
  if (strcmp(n, "script") == 0 || strcmp(n, "style") == 0) {
    st->in_skip = 0;
    return;
  }
  if (strcmp(n, "title") == 0) {
    st->in_title = 0;
    return;
  }
  if (strcmp(n, "a") == 0) {
    if (st->cur_link_pending && st->doc->link_count < CAPY_TEXT_MAX_LINKS) {
      strcpy(st->doc->links[st->doc->link_count].url, st->cur_url);
      st->doc->link_count += 1;
      emit_link_marker(st, (int)st->doc->link_count);
    }
    st->cur_link_pending = 0;
    return;
  }
  if (is_block_element(n)) {
    if (strcmp(n, "pre") == 0) {
      st->in_pre = 0;
      st->pre_lead = 0;
    } else if (strcmp(n, "ul") == 0 || strcmp(n, "ol") == 0) {
      list_pop(st);
    }
    body_flush_block(st);
  }
}

int capy_html_to_text(const uint8_t *html, size_t html_len, const char *base_url,
                      char *text_buf, size_t text_cap,
                      struct capy_text_doc *out) {
  struct capy_html_tokenizer tk;
  struct capy_html_token tok;
  struct emit_state st;
  size_t in_len = html_len;

  if (!html || !out) {
    return CAPY_TEXT_ERR_NULL;
  }

  memset(out, 0, sizeof(*out));
  memset(&st, 0, sizeof(st));
  st.buf = text_buf;
  st.cap = text_cap;
  st.doc = out;
  if (text_buf && text_cap > 0) {
    text_buf[0] = '\0';
  }

  if (in_len > CAPY_TEXT_MAX_INPUT) {
    in_len = CAPY_TEXT_MAX_INPUT;
    st.w_input_trunc = 1;
  }

  capy_html_tokenizer_init(&tk, (const char *)html, in_len);
  while (capy_html_tokenizer_next(&tk, &tok)) {
    switch (tok.type) {
      case CAPY_HTML_TOKEN_TEXT:
        if (st.in_skip) {
          break;
        }
        if (st.in_title) {
          title_emit_text(&st, tok.text, tok.text_len);
        } else {
          body_emit_text(&st, tok.text, tok.text_len);
        }
        break;
      case CAPY_HTML_TOKEN_START:
        handle_start(&st, &tok, base_url);
        break;
      case CAPY_HTML_TOKEN_END:
        handle_end(&st, &tok);
        break;
      case CAPY_HTML_TOKEN_COMMENT:
        if (tok.unclosed_comment) {
          st.w_unclosed_comment = 1;
        }
        break;
      case CAPY_HTML_TOKEN_DECL:
      case CAPY_HTML_TOKEN_EOF:
        break;
    }
    if (tok.unclosed_tag) {
      st.w_unclosed_tag = 1;
    }
  }

  if (st.buf && st.cap > 0) {
    st.buf[st.len] = '\0';
  }
  out->text_len = st.len;
  out->title[st.title_len] = '\0';
  out->has_title = (st.title_len > 0) ? 1 : 0;
  out->truncated = st.w_input_trunc || st.w_output_trunc || st.w_title_trunc ||
                   st.w_link_budget;

  if (st.w_input_trunc) {
    capy_text_warn_push(&out->warnings, CAPY_TEXT_WARN_INPUT_TRUNCATED);
  }
  if (st.w_output_trunc) {
    capy_text_warn_push(&out->warnings, CAPY_TEXT_WARN_OUTPUT_TRUNCATED);
  }
  if (st.w_title_trunc) {
    capy_text_warn_push(&out->warnings, CAPY_TEXT_WARN_TITLE_TRUNCATED);
  }
  if (st.w_link_budget) {
    capy_text_warn_push(&out->warnings, CAPY_TEXT_WARN_LINK_BUDGET);
  }
  if (st.w_link_unresolved) {
    capy_text_warn_push(&out->warnings, CAPY_TEXT_WARN_LINK_UNRESOLVED);
  }
  if (st.w_entity_invalid) {
    capy_text_warn_push(&out->warnings, CAPY_TEXT_WARN_ENTITY_INVALID);
  }
  if (st.w_unclosed_tag) {
    capy_text_warn_push(&out->warnings, CAPY_TEXT_WARN_UNCLOSED_TAG);
  }
  if (st.w_unclosed_comment) {
    capy_text_warn_push(&out->warnings, CAPY_TEXT_WARN_UNCLOSED_COMMENT);
  }

  return CAPY_TEXT_OK;
}
