#include "url_internal.h"

#include <string.h>

/*
 * Raw, syntactically-split components (before percent / dot-segment
 * normalization). All buffers are NUL-terminated.
 */
struct capy_url_raw {
  int has_scheme;
  char scheme[CAPY_URL_SCHEME_MAX + 1];
  int has_authority;
  char host[CAPY_URL_HOST_MAX + 1];
  int has_port;
  uint32_t port;
  char path[CAPY_URL_PATH_MAX + 1];
  int has_query;
  char query[CAPY_URL_QUERY_MAX + 1];
  int has_fragment;
  char fragment[CAPY_URL_FRAGMENT_MAX + 1];
};

/* Reject control bytes / raw spaces; enforce length; report length. */
static int validate_input(const char *s, size_t *out_len) {
  size_t i;
  for (i = 0; s[i] != '\0'; i++) {
    unsigned char c = (unsigned char)s[i];
    if (i >= CAPY_URL_MAX_LEN) {
      return CAPY_URL_ERR_TOO_LONG;
    }
    if (c == 0x20) {
      return CAPY_URL_ERR_SPACE;
    }
    if (c < 0x20 || c == 0x7F) {
      return CAPY_URL_ERR_CONTROL;
    }
  }
  if (i == 0) {
    return CAPY_URL_ERR_EMPTY;
  }
  *out_len = i;
  return CAPY_URL_OK;
}

/* Split a reference into raw components (RFC 3986 3 / appendix B). */
static int parse_raw(const char *s, size_t len, struct capy_url_raw *r) {
  size_t cursor = 0;

  memset(r, 0, sizeof(*r));

  /* scheme */
  if (capy_url_is_alpha((unsigned char)s[0])) {
    size_t i = 1;
    while (i < len && capy_url_is_scheme_char((unsigned char)s[i])) {
      i++;
    }
    if (i < len && s[i] == ':') {
      if (i > CAPY_URL_SCHEME_MAX) {
        return CAPY_URL_ERR_SCHEME;
      }
      memcpy(r->scheme, s, i);
      r->scheme[i] = '\0';
      r->has_scheme = 1;
      cursor = i + 1;
    }
  }

  /* authority */
  if (s[cursor] == '/' && s[cursor + 1] == '/') {
    size_t astart;
    size_t aend;
    size_t hstart;
    size_t hend;
    size_t pstart = 0;
    size_t j;
    int has_port_sep = 0;

    r->has_authority = 1;
    cursor += 2;
    astart = cursor;
    aend = cursor;
    while (aend < len && s[aend] != '/' && s[aend] != '?' && s[aend] != '#') {
      aend++;
    }
    for (j = astart; j < aend; j++) {
      if (s[j] == '@') {
        return CAPY_URL_ERR_HOST; /* userinfo is rejected */
      }
    }

    hstart = astart;
    if (astart < aend && s[astart] == '[') {
      size_t close = astart;
      while (close < aend && s[close] != ']') {
        close++;
      }
      if (close >= aend) {
        return CAPY_URL_ERR_HOST; /* unterminated IPv6 literal */
      }
      hend = close + 1;
      if (hend < aend) {
        if (s[hend] != ':') {
          return CAPY_URL_ERR_HOST; /* junk after ']' */
        }
        has_port_sep = 1;
        pstart = hend + 1;
      }
    } else {
      hend = astart;
      while (hend < aend && s[hend] != ':') {
        hend++;
      }
      if (hend < aend) {
        has_port_sep = 1;
        pstart = hend + 1;
      }
    }

    if (hend == hstart) {
      return CAPY_URL_ERR_HOST; /* empty host */
    }
    if (hend - hstart > CAPY_URL_HOST_MAX) {
      return CAPY_URL_ERR_OVERFLOW;
    }
    memcpy(r->host, s + hstart, hend - hstart);
    r->host[hend - hstart] = '\0';

    if (has_port_sep && pstart < aend) {
      uint32_t pv = 0;
      size_t k;
      for (k = pstart; k < aend; k++) {
        if (!capy_url_is_digit((unsigned char)s[k])) {
          return CAPY_URL_ERR_PORT;
        }
        pv = pv * 10u + (uint32_t)(s[k] - '0');
        if (pv > 65535u) {
          return CAPY_URL_ERR_PORT;
        }
      }
      r->has_port = 1;
      r->port = pv;
    }
    cursor = aend;
  }

  /* path */
  {
    size_t pe = cursor;
    while (pe < len && s[pe] != '?' && s[pe] != '#') {
      pe++;
    }
    if (pe - cursor > CAPY_URL_PATH_MAX) {
      return CAPY_URL_ERR_OVERFLOW;
    }
    memcpy(r->path, s + cursor, pe - cursor);
    r->path[pe - cursor] = '\0';
    cursor = pe;
  }

  /* query */
  if (cursor < len && s[cursor] == '?') {
    size_t qs = cursor + 1;
    size_t qe = qs;
    while (qe < len && s[qe] != '#') {
      qe++;
    }
    if (qe - qs > CAPY_URL_QUERY_MAX) {
      return CAPY_URL_ERR_OVERFLOW;
    }
    memcpy(r->query, s + qs, qe - qs);
    r->query[qe - qs] = '\0';
    r->has_query = 1;
    cursor = qe;
  }

  /* fragment */
  if (cursor < len && s[cursor] == '#') {
    size_t fs = cursor + 1;
    if (len - fs > CAPY_URL_FRAGMENT_MAX) {
      return CAPY_URL_ERR_OVERFLOW;
    }
    memcpy(r->fragment, s + fs, len - fs);
    r->fragment[len - fs] = '\0';
    r->has_fragment = 1;
  }

  return CAPY_URL_OK;
}

/* Percent-normalize one component in place (uses a scratch buffer). */
static int normalize_one(char *comp, size_t cap, int lower_host, int *fc,
                         int *fd, int *fn) {
  char tmp[CAPY_URL_PATH_MAX + 1];
  int n = capy_url_pct_normalize(comp, tmp, cap + 1, fc, fd, fn);
  if (n < 0) {
    return n;
  }
  if (lower_host) {
    capy_url_lower_inplace(tmp, 1);
  }
  memcpy(comp, tmp, (size_t)n + 1);
  return CAPY_URL_OK;
}

/* Lowercase scheme/host and percent-normalize host/path/query/fragment. */
static int normalize_raw(struct capy_url_raw *r, int *fc, int *fd, int *fn) {
  int rc;
  capy_url_lower_inplace(r->scheme, 0);
  if (r->has_authority) {
    rc = normalize_one(r->host, CAPY_URL_HOST_MAX, 1, fc, fd, fn);
    if (rc != CAPY_URL_OK) {
      return rc;
    }
  }
  rc = normalize_one(r->path, CAPY_URL_PATH_MAX, 0, fc, fd, fn);
  if (rc != CAPY_URL_OK) {
    return rc;
  }
  if (r->has_query) {
    rc = normalize_one(r->query, CAPY_URL_QUERY_MAX, 0, fc, fd, fn);
    if (rc != CAPY_URL_OK) {
      return rc;
    }
  }
  if (r->has_fragment) {
    rc = normalize_one(r->fragment, CAPY_URL_FRAGMENT_MAX, 0, fc, fd, fn);
    if (rc != CAPY_URL_OK) {
      return rc;
    }
  }
  return CAPY_URL_OK;
}

/* RFC 3986 5.2.3 merge: combine base path with a relative reference path. */
static int merge_path(const struct capy_url_raw *base, const char *ref_path,
                      char *out, size_t cap) {
  size_t reflen = strlen(ref_path);

  if (base->has_authority && base->path[0] == '\0') {
    if (1 + reflen > cap) {
      return CAPY_URL_ERR_OVERFLOW;
    }
    out[0] = '/';
    memcpy(out + 1, ref_path, reflen);
    out[1 + reflen] = '\0';
    return CAPY_URL_OK;
  }
  {
    size_t blen = strlen(base->path);
    size_t last_slash = blen; /* sentinel: "no slash" */
    size_t prefix_len;
    size_t i;
    for (i = 0; i < blen; i++) {
      if (base->path[i] == '/') {
        last_slash = i;
      }
    }
    prefix_len = (last_slash == blen) ? 0 : (last_slash + 1);
    if (prefix_len + reflen > cap) {
      return CAPY_URL_ERR_OVERFLOW;
    }
    memcpy(out, base->path, prefix_len);
    memcpy(out + prefix_len, ref_path, reflen);
    out[prefix_len + reflen] = '\0';
  }
  return CAPY_URL_OK;
}

/* RFC 3986 5.2.2 reference transform for a reference without a scheme. */
static int transform_no_scheme(const struct capy_url_raw *base,
                               const struct capy_url_raw *ref,
                               struct capy_url_raw *t, int *dot_changed) {
  memset(t, 0, sizeof(*t));
  t->has_scheme = 1;
  strcpy(t->scheme, base->scheme);
  *dot_changed = 0;

  if (ref->has_authority) {
    t->has_authority = 1;
    strcpy(t->host, ref->host);
    t->has_port = ref->has_port;
    t->port = ref->port;
    strcpy(t->path, ref->path);
    *dot_changed = capy_url_remove_dot_segments(t->path);
    t->has_query = ref->has_query;
    strcpy(t->query, ref->query);
  } else {
    t->has_authority = base->has_authority;
    strcpy(t->host, base->host);
    t->has_port = base->has_port;
    t->port = base->port;
    if (ref->path[0] == '\0') {
      strcpy(t->path, base->path);
      if (ref->has_query) {
        t->has_query = 1;
        strcpy(t->query, ref->query);
      } else {
        t->has_query = base->has_query;
        strcpy(t->query, base->query);
      }
    } else {
      if (ref->path[0] == '/') {
        strcpy(t->path, ref->path);
      } else {
        int rc = merge_path(base, ref->path, t->path, CAPY_URL_PATH_MAX);
        if (rc != CAPY_URL_OK) {
          return rc;
        }
      }
      *dot_changed = capy_url_remove_dot_segments(t->path);
      t->has_query = ref->has_query;
      strcpy(t->query, ref->query);
    }
  }
  t->has_fragment = ref->has_fragment;
  strcpy(t->fragment, ref->fragment);
  return CAPY_URL_OK;
}

int capy_url_parse(const char *input, const char *base, struct capy_url *out,
                   struct capy_url_warnings *warnings) {
  struct capy_url_raw ref;
  struct capy_url_raw target;
  uint32_t scheme_default;
  size_t len;
  int rc;
  int f_case = 0;
  int f_dec = 0;
  int f_non = 0;
  int f_dot = 0;
  int f_port = 0;

  if (warnings) {
    warnings->count = 0;
  }
  if (!input || !out) {
    return CAPY_URL_ERR_NULL;
  }

  rc = validate_input(input, &len);
  if (rc != CAPY_URL_OK) {
    return rc;
  }
  rc = parse_raw(input, len, &ref);
  if (rc != CAPY_URL_OK) {
    return rc;
  }
  rc = normalize_raw(&ref, &f_case, &f_dec, &f_non);
  if (rc != CAPY_URL_OK) {
    return rc;
  }

  if (ref.has_scheme) {
    target = ref;
    f_dot = capy_url_remove_dot_segments(target.path);
  } else {
    struct capy_url_raw base_raw;
    size_t blen;
    if (!base) {
      return CAPY_URL_ERR_BASE;
    }
    rc = validate_input(base, &blen);
    if (rc != CAPY_URL_OK) {
      return rc;
    }
    rc = parse_raw(base, blen, &base_raw);
    if (rc != CAPY_URL_OK) {
      return rc;
    }
    if (!base_raw.has_scheme) {
      return CAPY_URL_ERR_BASE;
    }
    rc = normalize_raw(&base_raw, NULL, NULL, NULL);
    if (rc != CAPY_URL_OK) {
      return rc;
    }
    (void)capy_url_remove_dot_segments(base_raw.path);
    rc = transform_no_scheme(&base_raw, &ref, &target, &f_dot);
    if (rc != CAPY_URL_OK) {
      return rc;
    }
  }

  scheme_default = capy_url_default_port(target.scheme);
  if (target.has_port && scheme_default != 0 && target.port == scheme_default) {
    f_port = 1;
    out->has_port = 0;
    out->port = scheme_default;
  } else if (target.has_port) {
    out->has_port = 1;
    out->port = target.port;
  } else {
    out->has_port = 0;
    out->port = scheme_default;
  }

  if (target.has_authority && target.path[0] == '\0') {
    target.path[0] = '/';
    target.path[1] = '\0';
  }

  strcpy(out->scheme, target.scheme);
  strcpy(out->host, target.host);
  strcpy(out->path, target.path);
  strcpy(out->query, target.query);
  strcpy(out->fragment, target.fragment);
  out->has_authority = target.has_authority;
  out->has_query = target.has_query;
  out->has_fragment = target.has_fragment;

  if (f_case) {
    capy_url_warn_push(warnings, CAPY_URL_WARN_PERCENT_CASE_NORMALIZED);
  }
  if (f_dec) {
    capy_url_warn_push(warnings, CAPY_URL_WARN_PERCENT_UNRESERVED_DECODED);
  }
  if (f_non) {
    capy_url_warn_push(warnings, CAPY_URL_WARN_NON_ASCII_PCT_ENCODED);
  }
  if (f_port) {
    capy_url_warn_push(warnings, CAPY_URL_WARN_DEFAULT_PORT_DROPPED);
  }
  if (f_dot) {
    capy_url_warn_push(warnings, CAPY_URL_WARN_DOT_SEGMENTS_RESOLVED);
  }

  return CAPY_URL_OK;
}
