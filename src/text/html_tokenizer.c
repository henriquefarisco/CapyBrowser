#include "html_tokenizer.h"

#include <string.h>

static int tk_is_space(int c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

static int tk_is_alpha(int c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int tk_is_digit(int c) { return c >= '0' && c <= '9'; }

static int tk_is_namechar(int c) {
  return tk_is_alpha(c) || tk_is_digit(c) || c == '-';
}

static char tk_lower(int c) {
  if (c >= 'A' && c <= 'Z') {
    return (char)(c - 'A' + 'a');
  }
  return (char)c;
}

/* Compare input[a..b) to a lower-case literal, case-insensitively. */
static int tk_range_eq_ci(const char *in, size_t a, size_t b, const char *lit) {
  size_t i;
  for (i = 0; a + i < b; i++) {
    if (lit[i] == '\0' || tk_lower((unsigned char)in[a + i]) != lit[i]) {
      return 0;
    }
  }
  return lit[i] == '\0';
}

static void tk_store_name(char *dst, const char *src, size_t len) {
  size_t i;
  if (len > CAPY_HTML_TAG_MAX) {
    len = CAPY_HTML_TAG_MAX;
  }
  for (i = 0; i < len; i++) {
    dst[i] = tk_lower((unsigned char)src[i]);
  }
  dst[len] = '\0';
}

void capy_html_tokenizer_init(struct capy_html_tokenizer *tk, const char *input,
                              size_t len) {
  tk->input = input;
  tk->len = len;
  tk->pos = 0;
  tk->raw_end[0] = '\0';
  tk->in_raw = 0;
}

/* Does input[j..] begin with the raw_end name followed by a tag delimiter? */
static int tk_raw_close_at(const struct capy_html_tokenizer *tk, size_t j) {
  size_t k = 0;
  while (tk->raw_end[k] != '\0') {
    if (j + k >= tk->len ||
        tk_lower((unsigned char)tk->input[j + k]) != tk->raw_end[k]) {
      return 0;
    }
    k++;
  }
  if (j + k >= tk->len) {
    return 1; /* "</name" at EOF: tolerant match */
  }
  {
    char c = tk->input[j + k];
    return tk_is_space((unsigned char)c) || c == '>' || c == '/';
  }
}

/* Parse a start or end tag beginning at tk->pos == '<'. */
static void tk_parse_tag(struct capy_html_tokenizer *tk,
                         struct capy_html_token *tok) {
  const char *in = tk->input;
  size_t len = tk->len;
  size_t p = tk->pos + 1; /* after '<' */

  if (p < len && in[p] == '!') {
    if (p + 2 < len && in[p + 1] == '-' && in[p + 2] == '-') {
      size_t i = tk->pos + 4;
      int found = 0;
      while (i + 2 < len) {
        if (in[i] == '-' && in[i + 1] == '-' && in[i + 2] == '>') {
          found = 1;
          break;
        }
        i++;
      }
      tok->type = CAPY_HTML_TOKEN_COMMENT;
      if (found) {
        tk->pos = i + 3;
      } else {
        tk->pos = len;
        tok->unclosed_comment = 1;
      }
      return;
    }
    {
      size_t i = tk->pos + 2;
      while (i < len && in[i] != '>') {
        i++;
      }
      tok->type = CAPY_HTML_TOKEN_DECL;
      tk->pos = (i < len) ? i + 1 : len;
      return;
    }
  }

  if (p < len && in[p] == '/') {
    size_t ns;
    p++;
    ns = p;
    while (p < len && tk_is_namechar((unsigned char)in[p])) {
      p++;
    }
    tk_store_name(tok->name, in + ns, p - ns);
    while (p < len && in[p] != '>') {
      p++;
    }
    tok->type = CAPY_HTML_TOKEN_END;
    if (p < len) {
      tk->pos = p + 1;
    } else {
      tk->pos = len;
      tok->unclosed_tag = 1;
    }
    return;
  }

  if (p < len && tk_is_alpha((unsigned char)in[p])) {
    size_t ns = p;
    int closed = 0;
    while (p < len && tk_is_namechar((unsigned char)in[p])) {
      p++;
    }
    tk_store_name(tok->name, in + ns, p - ns);

    while (p < len && in[p] != '>') {
      size_t as = 0;
      size_t ae = 0;
      size_t vs = 0;
      size_t ve = 0;
      int has_value = 0;

      if (in[p] == '/') {
        if (p + 1 < len && in[p + 1] == '>') {
          tok->self_closing = 1;
          p += 2;
          closed = 1;
          break;
        }
        p++;
        continue;
      }
      if (tk_is_space((unsigned char)in[p])) {
        p++;
        continue;
      }

      as = p;
      while (p < len && in[p] != '=' && in[p] != '>' && in[p] != '/' &&
             !tk_is_space((unsigned char)in[p])) {
        p++;
      }
      ae = p;
      while (p < len && tk_is_space((unsigned char)in[p])) {
        p++;
      }
      if (p < len && in[p] == '=') {
        p++;
        while (p < len && tk_is_space((unsigned char)in[p])) {
          p++;
        }
        if (p < len && (in[p] == '"' || in[p] == '\'')) {
          char q = in[p];
          p++;
          vs = p;
          while (p < len && in[p] != q) {
            p++;
          }
          ve = p;
          if (p < len) {
            p++;
          }
          has_value = 1;
        } else {
          vs = p;
          while (p < len && !tk_is_space((unsigned char)in[p]) &&
                 in[p] != '>') {
            p++;
          }
          ve = p;
          has_value = 1;
        }
      }

      if (!tok->has_href && has_value &&
          tk_range_eq_ci(in, as, ae, "href") && (ve - vs) <= CAPY_URL_MAX_LEN) {
        memcpy(tok->href, in + vs, ve - vs);
        tok->href[ve - vs] = '\0';
        tok->has_href = 1;
      }

      if (ae > as && tok->attr_count < CAPY_HTML_MAX_ATTRS) {
        struct capy_html_attr *at = &tok->attrs[tok->attr_count];
        at->name = in + as;
        at->name_len = ae - as;
        at->has_value = has_value;
        at->value = has_value ? (in + vs) : NULL;
        at->value_len = has_value ? (ve - vs) : 0;
        tok->attr_count += 1;
      }
    }

    if (!closed) {
      if (p < len && in[p] == '>') {
        p++;
      } else {
        tok->unclosed_tag = 1;
      }
    }
    tk->pos = p;
    tok->type = CAPY_HTML_TOKEN_START;

    if (!tok->self_closing &&
        (strcmp(tok->name, "script") == 0 || strcmp(tok->name, "style") == 0 ||
         strcmp(tok->name, "title") == 0 ||
         strcmp(tok->name, "textarea") == 0)) {
      strcpy(tk->raw_end, tok->name);
      tk->in_raw = 1;
    }
    return;
  }

  /* A '<' that starts neither a tag nor a comment: emit it as literal text. */
  tok->type = CAPY_HTML_TOKEN_TEXT;
  tok->text = in + tk->pos;
  tok->text_len = 1;
  tk->pos += 1;
}

int capy_html_tokenizer_next(struct capy_html_tokenizer *tk,
                             struct capy_html_token *tok) {
  memset(tok, 0, sizeof(*tok));

  if (tk->pos >= tk->len) {
    tok->type = CAPY_HTML_TOKEN_EOF;
    return 0;
  }

  if (tk->in_raw) {
    size_t start = tk->pos;
    size_t i = tk->pos;
    while (i < tk->len) {
      if (tk->input[i] == '<' && i + 1 < tk->len && tk->input[i + 1] == '/' &&
          tk_raw_close_at(tk, i + 2)) {
        break;
      }
      i++;
    }
    tk->in_raw = 0;
    if (i > start) {
      tok->type = CAPY_HTML_TOKEN_TEXT;
      tok->text = tk->input + start;
      tok->text_len = i - start;
      tk->pos = i;
      return 1;
    }
    /* close tag is right here: fall through to parse it normally */
  }

  if (tk->input[tk->pos] == '<') {
    tk_parse_tag(tk, tok);
    return 1;
  }

  {
    size_t start = tk->pos;
    size_t i = tk->pos;
    while (i < tk->len && tk->input[i] != '<') {
      i++;
    }
    tok->type = CAPY_HTML_TOKEN_TEXT;
    tok->text = tk->input + start;
    tok->text_len = i - start;
    tk->pos = i;
  }
  return 1;
}
