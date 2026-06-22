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
    {"Agrave", 192u},  {"Acirc", 194u},   {"Atilde", 195u}, {"Auml", 196u},
    {"Aring", 197u},   {"AElig", 198u},   {"Ccedil", 199u}, {"Egrave", 200u},
    {"Ecirc", 202u},   {"Euml", 203u},    {"Igrave", 204u}, {"Iacute", 205u},
    {"Icirc", 206u},   {"Iuml", 207u},    {"ETH", 208u},    {"Ograve", 210u},
    {"Oacute", 211u},  {"Ocirc", 212u},   {"Otilde", 213u}, {"Oslash", 216u},
    {"Ugrave", 217u},  {"Uacute", 218u},  {"Ucirc", 219u},  {"Yacute", 221u},
    {"THORN", 222u},   {"aelig", 230u},   {"eth", 240u},    {"icirc", 238u},
    {"iuml", 239u},    {"ocirc", 244u},   {"oslash", 248u}, {"ucirc", 251u},
    {"yacute", 253u},  {"thorn", 254u},   {"yuml", 255u},
    /* Remaining ISO-8859-1 symbol/punctuation entities (U+00A1-U+00BF). */
    {"iexcl", 161u},   {"curren", 164u},  {"brvbar", 166u}, {"uml", 168u},
    {"ordf", 170u},    {"not", 172u},     {"macr", 175u},   {"sup2", 178u},
    {"sup3", 179u},    {"acute", 180u},   {"micro", 181u},  {"cedil", 184u},
    {"sup1", 185u},    {"ordm", 186u},    {"iquest", 191u},
    /* HTML4 'symbols' (Greek letters, mathematical operators, arrows, card
     * suits) and 'special' (typographic punctuation, ligatures, spacing)
     * named entities -- completes the HTML 4.01 named entity set. Code
     * points follow the canonical WHATWG/HTML4 name->code-point mapping. */
    {"Alpha", 913u}, {"Beta", 914u}, {"Chi", 935u}, {"Delta", 916u},
    {"Epsilon", 917u}, {"Eta", 919u}, {"Gamma", 915u}, {"Iota", 921u},
    {"Kappa", 922u}, {"Lambda", 923u}, {"Mu", 924u}, {"Nu", 925u},
    {"OElig", 338u}, {"Omega", 937u}, {"Omicron", 927u}, {"Phi", 934u},
    {"Pi", 928u}, {"Prime", 8243u}, {"Psi", 936u}, {"Rho", 929u},
    {"Scaron", 352u}, {"Sigma", 931u}, {"Tau", 932u}, {"Theta", 920u},
    {"Upsilon", 933u}, {"Xi", 926u}, {"Yuml", 376u}, {"Zeta", 918u},
    {"alefsym", 8501u}, {"alpha", 945u}, {"and", 8743u}, {"ang", 8736u},
    {"asymp", 8776u}, {"bdquo", 8222u}, {"beta", 946u}, {"cap", 8745u},
    {"chi", 967u}, {"circ", 710u}, {"clubs", 9827u}, {"cong", 8773u},
    {"crarr", 8629u}, {"cup", 8746u}, {"dArr", 8659u}, {"darr", 8595u},
    {"delta", 948u}, {"diams", 9830u}, {"empty", 8709u}, {"emsp", 8195u},
    {"ensp", 8194u}, {"epsilon", 949u}, {"equiv", 8801u}, {"eta", 951u},
    {"exist", 8707u}, {"fnof", 402u}, {"forall", 8704u}, {"frasl", 8260u},
    {"gamma", 947u}, {"ge", 8805u}, {"hArr", 8660u}, {"harr", 8596u},
    {"hearts", 9829u}, {"image", 8465u}, {"infin", 8734u}, {"int", 8747u},
    {"iota", 953u}, {"isin", 8712u}, {"kappa", 954u}, {"lArr", 8656u},
    {"lambda", 955u}, {"lang", 9001u}, {"larr", 8592u}, {"lceil", 8968u},
    {"le", 8804u}, {"lfloor", 8970u}, {"lowast", 8727u}, {"loz", 9674u},
    {"lrm", 8206u}, {"lsaquo", 8249u}, {"minus", 8722u}, {"mu", 956u},
    {"nabla", 8711u}, {"ne", 8800u}, {"ni", 8715u}, {"notin", 8713u},
    {"nsub", 8836u}, {"nu", 957u}, {"oelig", 339u}, {"oline", 8254u},
    {"omega", 969u}, {"omicron", 959u}, {"oplus", 8853u}, {"or", 8744u},
    {"otimes", 8855u}, {"part", 8706u}, {"permil", 8240u}, {"perp", 8869u},
    {"phi", 966u}, {"pi", 960u}, {"piv", 982u}, {"prime", 8242u},
    {"prod", 8719u}, {"prop", 8733u}, {"psi", 968u}, {"rArr", 8658u},
    {"radic", 8730u}, {"rang", 9002u}, {"rarr", 8594u}, {"rceil", 8969u},
    {"real", 8476u}, {"rfloor", 8971u}, {"rho", 961u}, {"rlm", 8207u},
    {"rsaquo", 8250u}, {"sbquo", 8218u}, {"scaron", 353u}, {"sdot", 8901u},
    {"sigma", 963u}, {"sigmaf", 962u}, {"sim", 8764u}, {"spades", 9824u},
    {"sub", 8834u}, {"sube", 8838u}, {"sum", 8721u}, {"sup", 8835u},
    {"supe", 8839u}, {"tau", 964u}, {"there4", 8756u}, {"theta", 952u},
    {"thetasym", 977u}, {"thinsp", 8201u}, {"tilde", 732u}, {"uArr", 8657u},
    {"uarr", 8593u}, {"upsih", 978u}, {"upsilon", 965u}, {"weierp", 8472u},
    {"xi", 958u}, {"zeta", 950u}, {"zwj", 8205u}, {"zwnj", 8204u},
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

/* WHATWG HTML "numeric character reference end state": a numeric reference in
 * the C1 range 0x80..0x9F is a near-universal authoring error for the matching
 * Windows-1252 byte, so the standard maps it to the intended Unicode code
 * point (e.g. &#151; -> U+2014 EM DASH, &#128; -> U+20AC EURO SIGN). The five
 * values with no Windows-1252 assignment (0x81, 0x8D, 0x8F, 0x90, 0x9D) are
 * returned unchanged; the caller then treats them as invalid C1 controls.
 * Applies to numeric references only, never to named ones. */
static uint32_t ent_win1252_remap(uint32_t v) {
  switch (v) {
    case 0x80u: return 0x20ACu; /* EURO SIGN */
    case 0x82u: return 0x201Au; /* SINGLE LOW-9 QUOTATION MARK */
    case 0x83u: return 0x0192u; /* LATIN SMALL LETTER F WITH HOOK */
    case 0x84u: return 0x201Eu; /* DOUBLE LOW-9 QUOTATION MARK */
    case 0x85u: return 0x2026u; /* HORIZONTAL ELLIPSIS */
    case 0x86u: return 0x2020u; /* DAGGER */
    case 0x87u: return 0x2021u; /* DOUBLE DAGGER */
    case 0x88u: return 0x02C6u; /* MODIFIER LETTER CIRCUMFLEX ACCENT */
    case 0x89u: return 0x2030u; /* PER MILLE SIGN */
    case 0x8Au: return 0x0160u; /* LATIN CAPITAL LETTER S WITH CARON */
    case 0x8Bu: return 0x2039u; /* SINGLE LEFT-POINTING ANGLE QUOTATION MARK */
    case 0x8Cu: return 0x0152u; /* LATIN CAPITAL LIGATURE OE */
    case 0x8Eu: return 0x017Du; /* LATIN CAPITAL LETTER Z WITH CARON */
    case 0x91u: return 0x2018u; /* LEFT SINGLE QUOTATION MARK */
    case 0x92u: return 0x2019u; /* RIGHT SINGLE QUOTATION MARK */
    case 0x93u: return 0x201Cu; /* LEFT DOUBLE QUOTATION MARK */
    case 0x94u: return 0x201Du; /* RIGHT DOUBLE QUOTATION MARK */
    case 0x95u: return 0x2022u; /* BULLET */
    case 0x96u: return 0x2013u; /* EN DASH */
    case 0x97u: return 0x2014u; /* EM DASH */
    case 0x98u: return 0x02DCu; /* SMALL TILDE */
    case 0x99u: return 0x2122u; /* TRADE MARK SIGN */
    case 0x9Au: return 0x0161u; /* LATIN SMALL LETTER S WITH CARON */
    case 0x9Bu: return 0x203Au; /* SINGLE RIGHT-POINTING ANGLE QUOTATION MARK */
    case 0x9Cu: return 0x0153u; /* LATIN SMALL LIGATURE OE */
    case 0x9Eu: return 0x017Eu; /* LATIN SMALL LETTER Z WITH CARON */
    case 0x9Fu: return 0x0178u; /* LATIN CAPITAL LETTER Y WITH DIAERESIS */
    default: return v;
  }
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
    /* WHATWG numeric character reference end state: NULL (&#0;), surrogate
       halves (U+D800..U+DFFF) and out-of-range values (> U+10FFFF) are parse
       errors that resolve to U+FFFD. They share the CAPY_CP_INVALID sentinel,
       which the text emitter renders as U+FFFD REPLACEMENT CHARACTER while
       flagging ENTITY_INVALID. The C1 block (0x80..0x9F) is the Windows-1252
       authoring-error remap and is unaffected. */
    if (overflow || v == 0u || (v >= 0xD800u && v <= 0xDFFFu)) {
      *cp = CAPY_CP_INVALID;
    } else {
      if (v >= 0x80u && v <= 0x9Fu) {
        v = ent_win1252_remap(v);
      }
      *cp = v;
    }
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
