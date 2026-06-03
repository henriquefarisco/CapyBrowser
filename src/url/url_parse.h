#ifndef CAPY_URL_PARSE_H
#define CAPY_URL_PARSE_H

/*
 * capy-browser-core: URL parser / normalizer / origin (Fase C1).
 *
 * Pure, deterministic, allocation-free and decoupled from CapyOS:
 *   - same (input, base) always yield the same normalized URL, the same
 *     origin and the same warning sequence;
 *   - malformed input fails closed with a deterministic negative status;
 *   - no network, no filesystem, no clock, no randomness, no globals.
 *
 * HTTPS-first policy lives in the host adapter: non-HTTPS schemes still
 * parse here; the adapter is what rejects the fetch.
 */

#include <stddef.h>
#include <stdint.h>

/* Resource limits (alpha target). Input bound aligned with CapyOS
 * HTTP_MAX_URL = 2048. Per-component capacities fail closed on overflow. */
#define CAPY_URL_MAX_LEN 2048u
#define CAPY_URL_SCHEME_MAX 32u
#define CAPY_URL_HOST_MAX 256u
#define CAPY_URL_PATH_MAX 2048u
#define CAPY_URL_QUERY_MAX 2048u
#define CAPY_URL_FRAGMENT_MAX 2048u

/* Return codes. CAPY_URL_OK (0) on success; negative is a fail-closed reject. */
enum capy_url_status {
  CAPY_URL_OK = 0,
  CAPY_URL_ERR_NULL = -1,     /* NULL input or output pointer */
  CAPY_URL_ERR_EMPTY = -2,    /* empty reference with no usable base */
  CAPY_URL_ERR_TOO_LONG = -3, /* input exceeds CAPY_URL_MAX_LEN */
  CAPY_URL_ERR_CONTROL = -4,  /* control byte (< 0x20 or 0x7F) in input */
  CAPY_URL_ERR_SPACE = -5,    /* raw space (0x20) in input */
  CAPY_URL_ERR_PERCENT = -6,  /* '%' not followed by two hex digits */
  CAPY_URL_ERR_SCHEME = -7,   /* malformed scheme */
  CAPY_URL_ERR_HOST = -8,     /* authority present but host empty/invalid */
  CAPY_URL_ERR_PORT = -9,     /* invalid or out-of-range port */
  CAPY_URL_ERR_BASE = -10,    /* relative reference with missing/relative base */
  CAPY_URL_ERR_OVERFLOW = -11 /* a normalized component exceeds its capacity */
};

/*
 * Normalization / tolerant-recovery warnings. Emitted in a fixed canonical
 * order (the enum order below), independent of where in the input they occur,
 * so the warning sequence is deterministic for identical inputs.
 */
enum capy_url_warning {
  CAPY_URL_WARN_PERCENT_CASE_NORMALIZED = 1, /* %xx hex digits upper-cased */
  CAPY_URL_WARN_PERCENT_UNRESERVED_DECODED = 2, /* %xx of unreserved decoded */
  CAPY_URL_WARN_NON_ASCII_PCT_ENCODED = 3,      /* byte >= 0x80 encoded */
  CAPY_URL_WARN_DEFAULT_PORT_DROPPED = 4,       /* explicit default port removed */
  CAPY_URL_WARN_DOT_SEGMENTS_RESOLVED = 5       /* '.'/'..' segments resolved */
};

#define CAPY_URL_WARN_MAX 8u

struct capy_url_warnings {
  enum capy_url_warning codes[CAPY_URL_WARN_MAX];
  size_t count; /* number stored (saturates at CAPY_URL_WARN_MAX) */
};

/* Parsed, normalized, absolute URL. All component strings are NUL-terminated. */
struct capy_url {
  char scheme[CAPY_URL_SCHEME_MAX + 1];     /* lower-case, no trailing ':' */
  char host[CAPY_URL_HOST_MAX + 1];         /* lower-case, may be empty */
  char path[CAPY_URL_PATH_MAX + 1];         /* dot-segments resolved */
  char query[CAPY_URL_QUERY_MAX + 1];       /* without leading '?' */
  char fragment[CAPY_URL_FRAGMENT_MAX + 1]; /* without leading '#' */
  uint32_t port;     /* effective port: explicit value, else scheme default,
                      * else 0 when the scheme has no known default */
  int has_authority; /* a "//" authority component was present */
  int has_port;      /* an explicit, non-default port is kept in serialization */
  int has_query;     /* a '?' was present (query may still be empty) */
  int has_fragment;  /* a '#' was present (fragment may still be empty) */
};

/*
 * Parse + resolve (against an optional absolute base) + normalize, fail-closed.
 *
 *   input    : URL reference (absolute or relative), NUL-terminated.
 *   base     : optional absolute base for relative resolution; may be NULL.
 *   out      : receives the normalized, absolute URL on success.
 *   warnings : optional; receives the deterministic warning sequence; may be
 *              NULL. When non-NULL it is always reset before use.
 *
 * The result is always absolute: a relative reference requires an absolute
 * base, otherwise CAPY_URL_ERR_BASE is returned.
 *
 * Returns CAPY_URL_OK (0) or a negative enum capy_url_status.
 */
int capy_url_parse(const char *input, const char *base, struct capy_url *out,
                   struct capy_url_warnings *warnings);

/*
 * Recompose the normalized URL into out_buf (NUL-terminated).
 * Returns the written length (excluding NUL) on success, CAPY_URL_ERR_OVERFLOW
 * if out_cap is too small, or CAPY_URL_ERR_NULL on a NULL argument.
 */
int capy_url_serialize(const struct capy_url *url, char *out_buf,
                       size_t out_cap);

/* Origin tuple (scheme, host, effective port). */
struct capy_url_origin {
  char scheme[CAPY_URL_SCHEME_MAX + 1];
  char host[CAPY_URL_HOST_MAX + 1];
  uint32_t port;
};

/* Extract the origin of a parsed URL. Returns CAPY_URL_OK or a negative code. */
int capy_url_origin(const struct capy_url *url, struct capy_url_origin *out);

/* 1 if both origins are identical (scheme, host, port), 0 otherwise. */
int capy_url_origin_equal(const struct capy_url_origin *a,
                          const struct capy_url_origin *b);

/* Stable name for a warning code (fixtures/logs); "UNKNOWN" if unrecognized. */
const char *capy_url_warning_name(enum capy_url_warning w);

#endif /* CAPY_URL_PARSE_H */
