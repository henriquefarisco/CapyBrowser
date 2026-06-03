#ifndef CAPY_URL_INTERNAL_H
#define CAPY_URL_INTERNAL_H

/*
 * Internal helpers shared between url_parse.c, url_normalize.c and origin.c.
 * NOT part of the public capy-browser-core ABI; consumers include only
 * url_parse.h. All ASCII classification is locale-independent so results are
 * deterministic regardless of the host C locale.
 */

#include "url_parse.h"

static inline int capy_url_is_alpha(int c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static inline int capy_url_is_digit(int c) { return c >= '0' && c <= '9'; }

/* scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )  (first char checked
 * separately by the caller). */
static inline int capy_url_is_scheme_char(int c) {
  return capy_url_is_alpha(c) || capy_url_is_digit(c) || c == '+' || c == '-' ||
         c == '.';
}

static inline int capy_url_is_hex(int c) {
  return capy_url_is_digit(c) || (c >= 'a' && c <= 'f') ||
         (c >= 'A' && c <= 'F');
}

/* Value of a hex digit, or -1 if not hex. */
static inline int capy_url_hex_val(int c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return 10 + (c - 'a');
  }
  if (c >= 'A' && c <= 'F') {
    return 10 + (c - 'A');
  }
  return -1;
}

/* Upper-case hex digit for a nibble value 0..15. */
static inline char capy_url_hex_upper(int v) {
  static const char digits[] = "0123456789ABCDEF";
  return digits[v & 0x0F];
}

/* unreserved = ALPHA / DIGIT / "-" / "." / "_" / "~"  (RFC 3986 2.3). */
static inline int capy_url_is_unreserved(int c) {
  return capy_url_is_alpha(c) || capy_url_is_digit(c) || c == '-' || c == '.' ||
         c == '_' || c == '~';
}

static inline char capy_url_to_lower(int c) {
  if (c >= 'A' && c <= 'Z') {
    return (char)(c - 'A' + 'a');
  }
  return (char)c;
}

/* --- defined in url_normalize.c --- */

/*
 * Copy src -> dst applying percent-encoding normalization:
 *   - "%XX" validated (else CAPY_URL_ERR_PERCENT); hex upper-cased;
 *     if the triplet encodes an unreserved byte it is decoded to that byte;
 *   - bytes >= 0x80 are percent-encoded as "%XX" (upper-case hex);
 *   - all other bytes are copied verbatim.
 * The three flag pointers (any may be NULL) are OR-ed with 1 when the matching
 * transformation happens. Returns the dst length (excluding NUL) or a negative
 * enum capy_url_status (CAPY_URL_ERR_PERCENT / CAPY_URL_ERR_OVERFLOW).
 */
int capy_url_pct_normalize(const char *src, char *dst, size_t dst_cap,
                           int *flag_case, int *flag_decoded,
                           int *flag_non_ascii);

/* RFC 3986 5.2.4 remove_dot_segments, in place. The buffer must be
 * NUL-terminated with length <= CAPY_URL_PATH_MAX. Returns 1 if the path
 * changed, 0 otherwise. */
int capy_url_remove_dot_segments(char *path);

/* Lower-case ASCII letters in place. When skip_pct is non-zero, the two hex
 * digits following a '%' are left untouched (they were already upper-cased by
 * percent normalization). */
void capy_url_lower_inplace(char *s, int skip_pct);

/* Default port for a (lower-case) scheme, or 0 when unknown. */
uint32_t capy_url_default_port(const char *scheme);

/* Append a warning, saturating at CAPY_URL_WARN_MAX. NULL sink is ignored. */
void capy_url_warn_push(struct capy_url_warnings *w, enum capy_url_warning code);

#endif /* CAPY_URL_INTERNAL_H */
