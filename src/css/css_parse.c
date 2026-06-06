/*
 * capy-browser-core: tolerant CSS parser (Fase M2, part 1).
 *
 * A single-pass lexer + parser over the CSS bytes producing a deterministic
 * stylesheet (see css_parse.h). Mirrors the DOM arena style: once-each ordered
 * warnings, fail-closed budgets, no allocation. Only a simple-selector subset
 * is kept; everything else is recovered from with a deterministic warning.
 */

#include "css_parse.h"

/* Record a warning once, in fixed canonical order, saturating at the cap. */
static void css_warn(struct capy_css_stylesheet *s,
                     enum capy_css_warning code) {
  size_t i;
  for (i = 0; i < s->warnings.count; i++) {
    if (s->warnings.codes[i] == code) {
      return;
    }
  }
  if (s->warnings.count < CAPY_CSS_WARN_MAX) {
    s->warnings.codes[s->warnings.count++] = code;
  }
}

static char css_to_lower(unsigned char c) {
  if (c >= 'A' && c <= 'Z') {
    return (char)(c - 'A' + 'a');
  }
  return (char)c;
}

/* A byte dropped from interned strings (all control bytes incl. whitespace). */
static int css_drop_byte(unsigned char c) {
  return c < 0x20u || c == 0x7Fu;
}

static int css_is_space(unsigned char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

static int css_is_ident_char(unsigned char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '-' || c == '_';
}

/*
 * Intern bytes into the string arena, optionally lower-casing, dropping control
 * bytes. Sets off/out_len to the interned range. Returns 1 if everything fit,
 * 0 if the arena filled (the partial range is still set).
 */
static int css_intern(struct capy_css_stylesheet *s, const char *src,
                      size_t len, int lower, size_t *off, size_t *out_len) {
  size_t start = s->string_len;
  size_t i;
  int ok = 1;
  for (i = 0; i < len; i++) {
    unsigned char c = (unsigned char)src[i];
    if (css_drop_byte(c)) {
      continue;
    }
    if (s->string_len >= CAPY_CSS_STRING_ARENA) {
      css_warn(s, CAPY_CSS_WARN_STRING_BUDGET);
      s->truncated = 1;
      ok = 0;
      break;
    }
    s->strings[s->string_len++] = lower ? css_to_lower(c) : (char)c;
  }
  *off = start;
  *out_len = s->string_len - start;
  return ok;
}

struct css_lex {
  const char *s;
  size_t len;
  size_t pos;
};

/* Skip whitespace and CSS block comments (warns on an unclosed comment). */
static void css_skip_ws(struct css_lex *lx, struct capy_css_stylesheet *sheet) {
  while (lx->pos < lx->len) {
    unsigned char c = (unsigned char)lx->s[lx->pos];
    if (css_is_space(c)) {
      lx->pos++;
      continue;
    }
    if (c == '/' && lx->pos + 1 < lx->len && lx->s[lx->pos + 1] == '*') {
      lx->pos += 2;
      while (lx->pos + 1 < lx->len &&
             !(lx->s[lx->pos] == '*' && lx->s[lx->pos + 1] == '/')) {
        lx->pos++;
      }
      if (lx->pos + 1 < lx->len) {
        lx->pos += 2; /* consume the closing delimiter */
      } else {
        lx->pos = lx->len;
        css_warn(sheet, CAPY_CSS_WARN_UNCLOSED_COMMENT);
      }
      continue;
    }
    break;
  }
}

/* Skip a balanced {...} block. Assumes lx->pos is at the opening '{'. */
static void css_skip_block(struct css_lex *lx) {
  size_t depth = 0;
  while (lx->pos < lx->len) {
    char c = lx->s[lx->pos++];
    if (c == '{') {
      depth++;
    } else if (c == '}') {
      if (depth > 0) {
        depth--;
      }
      if (depth == 0) {
        return;
      }
    }
  }
}

/* Skip an at-rule: to ';' (statement) or past its {...} block. */
static void css_skip_at_rule(struct css_lex *lx,
                             struct capy_css_stylesheet *s) {
  css_warn(s, CAPY_CSS_WARN_AT_RULE_SKIPPED);
  while (lx->pos < lx->len) {
    char c = lx->s[lx->pos];
    if (c == ';') {
      lx->pos++;
      return;
    }
    if (c == '{') {
      css_skip_block(lx);
      return;
    }
    lx->pos++;
  }
}

/* Advance past a malformed declaration: to ';' (consumed) or '}' (kept). */
static void css_skip_to_decl_end(struct css_lex *lx) {
  while (lx->pos < lx->len) {
    char c = lx->s[lx->pos];
    if (c == ';') {
      lx->pos++;
      return;
    }
    if (c == '}') {
      return;
    }
    lx->pos++;
  }
}

/* Intern and store one declaration; warn and drop if it vanishes. */
static void css_add_decl(struct capy_css_stylesheet *s, const char *name,
                         size_t nlen, const char *val, size_t vlen) {
  struct capy_css_decl *d;
  size_t poff = 0;
  size_t plen = 0;
  size_t voff = 0;
  size_t vlen2 = 0;
  if (s->decl_count >= CAPY_CSS_MAX_DECLS) {
    css_warn(s, CAPY_CSS_WARN_DECL_BUDGET);
    s->truncated = 1;
    return;
  }
  (void)css_intern(s, name, nlen, 1, &poff, &plen);
  (void)css_intern(s, val, vlen, 0, &voff, &vlen2);
  if (plen == 0 || vlen2 == 0) {
    css_warn(s, CAPY_CSS_WARN_DECL_SKIPPED);
    return;
  }
  d = &s->decls[s->decl_count++];
  d->prop_off = poff;
  d->prop_len = plen;
  d->value_off = voff;
  d->value_len = vlen2;
}

/* Parse a {...} block into a contiguous declaration range. */
static void css_parse_block(struct css_lex *lx, struct capy_css_stylesheet *s,
                            size_t *decl_start, size_t *decl_count) {
  *decl_start = s->decl_count;
  if (lx->pos < lx->len && lx->s[lx->pos] == '{') {
    lx->pos++;
  }
  for (;;) {
    size_t name_start;
    size_t name_end;
    size_t val_start;
    size_t val_end;
    css_skip_ws(lx, s);
    if (lx->pos >= lx->len) {
      css_warn(s, CAPY_CSS_WARN_UNCLOSED_BLOCK);
      break;
    }
    if (lx->s[lx->pos] == '}') {
      lx->pos++;
      break;
    }
    name_start = lx->pos;
    while (lx->pos < lx->len) {
      char c = lx->s[lx->pos];
      if (c == ':' || c == ';' || c == '}' || css_is_space((unsigned char)c)) {
        break;
      }
      lx->pos++;
    }
    name_end = lx->pos;
    css_skip_ws(lx, s);
    if (lx->pos >= lx->len || lx->s[lx->pos] != ':') {
      css_warn(s, CAPY_CSS_WARN_DECL_SKIPPED);
      css_skip_to_decl_end(lx);
      continue;
    }
    lx->pos++; /* consume the colon */
    css_skip_ws(lx, s);
    val_start = lx->pos;
    while (lx->pos < lx->len && lx->s[lx->pos] != ';' && lx->s[lx->pos] != '}') {
      lx->pos++;
    }
    val_end = lx->pos;
    while (val_end > val_start &&
           css_is_space((unsigned char)lx->s[val_end - 1])) {
      val_end--;
    }
    if (name_end > name_start && val_end > val_start) {
      css_add_decl(s, lx->s + name_start, name_end - name_start,
                   lx->s + val_start, val_end - val_start);
    } else {
      css_warn(s, CAPY_CSS_WARN_DECL_SKIPPED);
    }
    if (lx->pos < lx->len && lx->s[lx->pos] == ';') {
      lx->pos++;
      continue;
    }
    if (lx->pos < lx->len && lx->s[lx->pos] == '}') {
      lx->pos++;
      break;
    }
    css_warn(s, CAPY_CSS_WARN_UNCLOSED_BLOCK);
    break;
  }
  *decl_count = s->decl_count - *decl_start;
}

static int css_all_ident(const char *p, size_t n) {
  size_t i;
  if (n == 0) {
    return 0;
  }
  for (i = 0; i < n; i++) {
    if (!css_is_ident_char((unsigned char)p[i])) {
      return 0;
    }
  }
  return 1;
}

static void css_trim(const char **p, size_t *n) {
  const char *str = *p;
  size_t len = *n;
  while (len > 0 && css_is_space((unsigned char)str[0])) {
    str++;
    len--;
  }
  while (len > 0 && css_is_space((unsigned char)str[len - 1])) {
    len--;
  }
  *p = str;
  *n = len;
}

/* Classify one comma-separated selector entry and, if supported, emit a rule. */
static void css_emit_one_selector(struct capy_css_stylesheet *s, const char *p,
                                  size_t n, size_t decl_start,
                                  size_t decl_count) {
  struct capy_css_rule *r;
  enum capy_css_selector_kind kind = CAPY_CSS_SEL_UNIVERSAL;
  const char *name = p;
  size_t namelen = n;

  css_trim(&name, &namelen);
  if (namelen == 0) {
    css_warn(s, CAPY_CSS_WARN_SELECTOR_SKIPPED);
    return;
  }
  if (namelen == 1 && name[0] == '*') {
    kind = CAPY_CSS_SEL_UNIVERSAL;
    namelen = 0;
  } else if (name[0] == '.' && css_all_ident(name + 1, namelen - 1)) {
    kind = CAPY_CSS_SEL_CLASS;
    name++;
    namelen--;
  } else if (name[0] == '#' && css_all_ident(name + 1, namelen - 1)) {
    kind = CAPY_CSS_SEL_ID;
    name++;
    namelen--;
  } else if (css_all_ident(name, namelen)) {
    kind = CAPY_CSS_SEL_TYPE;
  } else {
    css_warn(s, CAPY_CSS_WARN_SELECTOR_SKIPPED);
    return;
  }

  if (s->rule_count >= CAPY_CSS_MAX_RULES) {
    css_warn(s, CAPY_CSS_WARN_RULE_BUDGET);
    s->truncated = 1;
    return;
  }
  r = &s->rules[s->rule_count++];
  r->selector.kind = kind;
  if (namelen > 0) {
    size_t off = 0;
    size_t olen = 0;
    (void)css_intern(s, name, namelen, kind == CAPY_CSS_SEL_TYPE, &off, &olen);
    r->selector.name_off = off;
    r->selector.name_len = olen;
  } else {
    r->selector.name_off = 0;
    r->selector.name_len = 0;
  }
  r->decl_start = decl_start;
  r->decl_count = decl_count;
}

/* Split a selector prelude on commas and emit each supported selector. */
static void css_emit_selectors(struct capy_css_stylesheet *s,
                               const char *prelude, size_t n,
                               size_t decl_start, size_t decl_count) {
  size_t start = 0;
  size_t i;
  for (i = 0; i <= n; i++) {
    if (i == n || prelude[i] == ',') {
      css_emit_one_selector(s, prelude + start, i - start, decl_start,
                            decl_count);
      start = i + 1;
    }
  }
}

/* Parse one qualified rule: a selector prelude followed by a {...} block. */
static void css_parse_qualified_rule(struct css_lex *lx,
                                     struct capy_css_stylesheet *s) {
  size_t prelude_start = lx->pos;
  size_t prelude_end;
  size_t decl_start = 0;
  size_t decl_count = 0;
  while (lx->pos < lx->len && lx->s[lx->pos] != '{' && lx->s[lx->pos] != '}') {
    lx->pos++;
  }
  if (lx->pos >= lx->len || lx->s[lx->pos] == '}') {
    if (lx->pos < lx->len && lx->s[lx->pos] == '}') {
      lx->pos++; /* drop a stray brace */
    }
    css_warn(s, CAPY_CSS_WARN_SELECTOR_SKIPPED);
    return;
  }
  prelude_end = lx->pos;
  css_parse_block(lx, s, &decl_start, &decl_count);
  css_emit_selectors(s, lx->s + prelude_start, prelude_end - prelude_start,
                     decl_start, decl_count);
}

int capy_css_parse(const char *css, size_t css_len,
                   struct capy_css_stylesheet *out) {
  struct css_lex lx;
  if (!css || !out) {
    return CAPY_CSS_ERR_NULL;
  }
  out->rule_count = 0;
  out->decl_count = 0;
  out->string_len = 0;
  out->truncated = 0;
  out->warnings.count = 0;

  if (css_len > CAPY_CSS_MAX_INPUT) {
    css_len = CAPY_CSS_MAX_INPUT;
    css_warn(out, CAPY_CSS_WARN_INPUT_TRUNCATED);
    out->truncated = 1;
  }
  lx.s = css;
  lx.len = css_len;
  lx.pos = 0;

  for (;;) {
    css_skip_ws(&lx, out);
    if (lx.pos >= lx.len) {
      break;
    }
    if (lx.s[lx.pos] == '@') {
      css_skip_at_rule(&lx, out);
      continue;
    }
    css_parse_qualified_rule(&lx, out);
  }
  return CAPY_CSS_OK;
}

const char *capy_css_string(const struct capy_css_stylesheet *sheet,
                            size_t off) {
  if (!sheet || off > sheet->string_len) {
    return NULL;
  }
  return sheet->strings + off;
}

const char *capy_css_selector_kind_name(enum capy_css_selector_kind k) {
  switch (k) {
    case CAPY_CSS_SEL_UNIVERSAL:
      return "universal";
    case CAPY_CSS_SEL_TYPE:
      return "type";
    case CAPY_CSS_SEL_CLASS:
      return "class";
    case CAPY_CSS_SEL_ID:
      return "id";
  }
  return "unknown";
}

const char *capy_css_warning_name(enum capy_css_warning w) {
  switch (w) {
    case CAPY_CSS_WARN_INPUT_TRUNCATED:
      return "INPUT_TRUNCATED";
    case CAPY_CSS_WARN_RULE_BUDGET:
      return "RULE_BUDGET";
    case CAPY_CSS_WARN_DECL_BUDGET:
      return "DECL_BUDGET";
    case CAPY_CSS_WARN_STRING_BUDGET:
      return "STRING_BUDGET";
    case CAPY_CSS_WARN_AT_RULE_SKIPPED:
      return "AT_RULE_SKIPPED";
    case CAPY_CSS_WARN_SELECTOR_SKIPPED:
      return "SELECTOR_SKIPPED";
    case CAPY_CSS_WARN_DECL_SKIPPED:
      return "DECL_SKIPPED";
    case CAPY_CSS_WARN_UNCLOSED_BLOCK:
      return "UNCLOSED_BLOCK";
    case CAPY_CSS_WARN_UNCLOSED_COMMENT:
      return "UNCLOSED_COMMENT";
  }
  return "UNKNOWN";
}
