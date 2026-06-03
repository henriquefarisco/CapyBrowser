#include "html_entities.h"

#include <string.h>

static int ent_is_digit(int c) { return c >= '0' && c <= '9'; }

static int ent_is_hex(int c) {
  return ent_is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static int ent_is_alnum(int c) {
  return ent_is_digit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int ent_hex_val(int c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return 10 + (c - 'a');
  }
  return 10 + (c - 'A');
}

/* Common named entities (subset). Names are case-sensitive, as in HTML. */
struct capy_named_entity {
  const char *name;
  uint32_t cp;
};

static const struct capy_named_entity g_entities[] = {
    {"amp", 38u},      {"lt", 60u},       {"gt", 62u},      {"quot", 34u},
    {"apos", 39u},     {"nbsp", 160u},    {"copy", 169u},   {"reg", 174u},
    {"trade", 8482u},  {"mdash", 8212u},  {"ndash", 8211u}, {"hellip", 8230u},
    {"ldquo", 8220u},  {"rdquo", 8221u},  {"lsquo", 8216u}, {"rsquo", 8217u},
    {"laquo", 171u},   {"raquo", 187u},   {"deg", 176u},    {"plusmn", 177u},
    {"times", 215u},   {"divide", 247u},  {"euro", 8364u},  {"pound", 163u},
    {"cent", 162u},    {"yen", 165u},     {"sect", 167u},   {"para", 182u},
    {"middot", 183u},  {"bull", 8226u},   {"dagger", 8224u},{"Dagger", 8225u},
    {"frac12", 189u},  {"frac14", 188u},  {"frac34", 190u}, {"agrave", 224u},
    {"aacute", 225u},  {"acirc", 226u},   {"atilde", 227u}, {"auml", 228u},
    {"aring", 229u},   {"ccedil", 231u},  {"egrave", 232u}, {"eacute", 233u},
    {"ecirc", 234u},   {"euml", 235u},    {"igrave", 236u}, {"iacute", 237u},
    {"ntilde", 241u},  {"ograve", 242u},  {"oacute", 243u}, {"ouml", 246u},
    {"otilde", 245u},  {"ugrave", 249u},  {"uacute", 250u}, {"uuml", 252u},
    {"szlig", 223u},   {"Aacute", 193u},  {"Eacute", 201u}, {"Ntilde", 209u},
    {"Ouml", 214u},    {"Uuml", 220u},    {"shy", 173u},
};

size_t capy_utf8_encode(uint32_t cp, char out[4]) {
  if (cp <= 0x7Fu) {
    out[0] = (char)cp;
    return 1;
  }
  if (cp <= 0x7FFu) {
    out[0] = (char)(0xC0u | (cp >> 6));
    out[1] = (char)(0x80u | (cp & 0x3Fu));
    return 2;
  }
  if (cp <= 0xFFFFu) {
    if (cp >= 0xD800u && cp <= 0xDFFFu) {
      return 0; /* surrogate halves are not scalar values */
    }
    out[0] = (char)(0xE0u | (cp >> 12));
    out[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
    out[2] = (char)(0x80u | (cp & 0x3Fu));
    return 3;
  }
  if (cp <= 0x10FFFFu) {
    out[0] = (char)(0xF0u | (cp >> 18));
    out[1] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
    out[2] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
    out[3] = (char)(0x80u | (cp & 0x3Fu));
    return 4;
  }
  return 0;
}

int capy_html_entity_lookup(const char *name, size_t len, uint32_t *cp) {
  size_t i;
  for (i = 0; i < sizeof(g_entities) / sizeof(g_entities[0]); i++) {
    if (strlen(g_entities[i].name) == len &&
        strncmp(g_entities[i].name, name, len) == 0) {
      *cp = g_entities[i].cp;
      return 1;
    }
  }
  return 0;
}

size_t capy_html_charref_at(const char *s, size_t len, uint32_t *cp) {
  if (len < 3u || s[0] != '&') {
    return 0;
  }

  if (s[1] == '#') {
    size_t i = 2;
    uint32_t v = 0;
    int digits = 0;
    int overflow = 0;
    if (i < len && (s[i] == 'x' || s[i] == 'X')) {
      i++;
      while (i < len && ent_is_hex((unsigned char)s[i])) {
        if (v <= 0x10FFFFu) {
          v = v * 16u + (uint32_t)ent_hex_val((unsigned char)s[i]);
        }
        if (v > 0x10FFFFu || digits >= 8) {
          overflow = 1;
        }
        digits++;
        i++;
      }
    } else {
      while (i < len && ent_is_digit((unsigned char)s[i])) {
        if (v <= 0x10FFFFu) {
          v = v * 10u + (uint32_t)(s[i] - '0');
        }
        if (v > 0x10FFFFu || digits >= 9) {
          overflow = 1;
        }
        digits++;
        i++;
      }
    }
    if (digits == 0 || i >= len || s[i] != ';') {
      return 0;
    }
    *cp = overflow ? CAPY_CP_INVALID : v;
    return i + 1;
  }

  {
    size_t i = 1;
    while (i < len && (i - 1) < 31u && ent_is_alnum((unsigned char)s[i])) {
      i++;
    }
    if (i == 1 || i >= len || s[i] != ';') {
      return 0;
    }
    if (capy_html_entity_lookup(s + 1, i - 1, cp)) {
      return i + 1;
    }
  }
  return 0;
}
