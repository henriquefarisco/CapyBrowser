#include "url_internal.h"

#include <string.h>

int capy_url_pct_normalize(const char *src, char *dst, size_t dst_cap,
                           int *flag_case, int *flag_decoded,
                           int *flag_non_ascii) {
  size_t si = 0;
  size_t di = 0;

  if (!src || !dst || dst_cap == 0) {
    return CAPY_URL_ERR_NULL;
  }

  while (src[si] != '\0') {
    unsigned char c = (unsigned char)src[si];

    if (c == '%') {
      int hi = capy_url_hex_val((unsigned char)src[si + 1]);
      int lo = (hi >= 0) ? capy_url_hex_val((unsigned char)src[si + 2]) : -1;
      int val;
      if (hi < 0 || lo < 0) {
        return CAPY_URL_ERR_PERCENT;
      }
      val = (hi << 4) | lo;
      if (capy_url_is_unreserved(val)) {
        /* Decode unreserved octets so normalization is idempotent and so
         * encoded dot-segments (e.g. %2E) participate in path resolution. */
        if (di + 1 >= dst_cap) {
          return CAPY_URL_ERR_OVERFLOW;
        }
        dst[di++] = (char)val;
        if (flag_decoded) {
          *flag_decoded = 1;
        }
      } else {
        char u_hi = capy_url_hex_upper(hi);
        char u_lo = capy_url_hex_upper(lo);
        if (di + 3 >= dst_cap) {
          return CAPY_URL_ERR_OVERFLOW;
        }
        dst[di++] = '%';
        dst[di++] = u_hi;
        dst[di++] = u_lo;
        if ((char)src[si + 1] != u_hi || (char)src[si + 2] != u_lo) {
          if (flag_case) {
            *flag_case = 1;
          }
        }
      }
      si += 3;
    } else if (c >= 0x80) {
      /* Percent-encode non-ASCII bytes (IDNA/punycode is future work). */
      if (di + 3 >= dst_cap) {
        return CAPY_URL_ERR_OVERFLOW;
      }
      dst[di++] = '%';
      dst[di++] = capy_url_hex_upper((int)(c >> 4));
      dst[di++] = capy_url_hex_upper((int)(c & 0x0F));
      if (flag_non_ascii) {
        *flag_non_ascii = 1;
      }
      si += 1;
    } else {
      if (di + 1 >= dst_cap) {
        return CAPY_URL_ERR_OVERFLOW;
      }
      dst[di++] = (char)c;
      si += 1;
    }
  }

  dst[di] = '\0';
  return (int)di;
}

int capy_url_remove_dot_segments(char *path) {
  char in[CAPY_URL_PATH_MAX + 1];
  char out[CAPY_URL_PATH_MAX + 1];
  size_t in_len;
  size_t p = 0;
  size_t o = 0;

  if (!path) {
    return 0;
  }
  in_len = strlen(path);
  if (in_len > CAPY_URL_PATH_MAX) {
    in_len = CAPY_URL_PATH_MAX;
  }
  memcpy(in, path, in_len);
  in[in_len] = '\0';

  while (p < in_len) {
    size_t rem = in_len - p;
    const char *s = in + p;

    if (rem >= 3 && s[0] == '.' && s[1] == '.' && s[2] == '/') {
      p += 3; /* "../" */
    } else if (rem >= 2 && s[0] == '.' && s[1] == '/') {
      p += 2; /* "./" */
    } else if (rem >= 3 && s[0] == '/' && s[1] == '.' && s[2] == '/') {
      p += 2; /* "/./" -> leave the trailing '/' */
    } else if (rem == 2 && s[0] == '/' && s[1] == '.') {
      out[o++] = '/'; /* "/." -> "/" */
      p += 2;
    } else if (rem >= 4 && s[0] == '/' && s[1] == '.' && s[2] == '.' &&
               s[3] == '/') {
      p += 3; /* "/../" -> leave the trailing '/' and pop a segment */
      while (o > 0 && out[o - 1] != '/') {
        o--;
      }
      if (o > 0) {
        o--;
      }
    } else if (rem == 3 && s[0] == '/' && s[1] == '.' && s[2] == '.') {
      while (o > 0 && out[o - 1] != '/') { /* "/.." -> "/" and pop */
        o--;
      }
      if (o > 0) {
        o--;
      }
      out[o++] = '/';
      p += 3;
    } else if (rem == 1 && s[0] == '.') {
      p += 1; /* "." */
    } else if (rem == 2 && s[0] == '.' && s[1] == '.') {
      p += 2; /* ".." */
    } else {
      /* Move one path segment, including its leading '/', to the output. */
      size_t k = 0;
      if (s[0] == '/') {
        out[o++] = '/';
        k = 1;
      }
      while (k < rem && s[k] != '/') {
        out[o++] = s[k];
        k++;
      }
      p += k;
    }
  }

  out[o] = '\0';
  if (o == in_len && memcmp(out, in, o) == 0) {
    return 0;
  }
  memcpy(path, out, o + 1);
  return 1;
}

void capy_url_lower_inplace(char *s, int skip_pct) {
  size_t i = 0;
  if (!s) {
    return;
  }
  while (s[i] != '\0') {
    if (skip_pct && s[i] == '%' && capy_url_is_hex((unsigned char)s[i + 1]) &&
        capy_url_is_hex((unsigned char)s[i + 2])) {
      i += 3; /* keep an already-normalized %XX triplet untouched */
      continue;
    }
    s[i] = capy_url_to_lower((unsigned char)s[i]);
    i += 1;
  }
}

uint32_t capy_url_default_port(const char *scheme) {
  if (!scheme) {
    return 0;
  }
  if (strcmp(scheme, "https") == 0) {
    return 443;
  }
  if (strcmp(scheme, "http") == 0) {
    return 80;
  }
  if (strcmp(scheme, "wss") == 0) {
    return 443;
  }
  if (strcmp(scheme, "ws") == 0) {
    return 80;
  }
  if (strcmp(scheme, "ftp") == 0) {
    return 21;
  }
  return 0;
}

void capy_url_warn_push(struct capy_url_warnings *w,
                        enum capy_url_warning code) {
  if (!w) {
    return;
  }
  if (w->count < CAPY_URL_WARN_MAX) {
    w->codes[w->count] = code;
    w->count += 1;
  }
}

const char *capy_url_warning_name(enum capy_url_warning w) {
  switch (w) {
    case CAPY_URL_WARN_PERCENT_CASE_NORMALIZED:
      return "PERCENT_CASE_NORMALIZED";
    case CAPY_URL_WARN_PERCENT_UNRESERVED_DECODED:
      return "PERCENT_UNRESERVED_DECODED";
    case CAPY_URL_WARN_NON_ASCII_PCT_ENCODED:
      return "NON_ASCII_PCT_ENCODED";
    case CAPY_URL_WARN_DEFAULT_PORT_DROPPED:
      return "DEFAULT_PORT_DROPPED";
    case CAPY_URL_WARN_DOT_SEGMENTS_RESOLVED:
      return "DOT_SEGMENTS_RESOLVED";
  }
  return "UNKNOWN";
}

static int url_append(char *buf, size_t cap, size_t *o, const char *s) {
  size_t i = 0;
  while (s[i] != '\0') {
    if (*o + 1 >= cap) {
      return -1;
    }
    buf[*o] = s[i];
    *o += 1;
    i += 1;
  }
  return 0;
}

static int url_append_ch(char *buf, size_t cap, size_t *o, char c) {
  if (*o + 1 >= cap) {
    return -1;
  }
  buf[*o] = c;
  *o += 1;
  return 0;
}

static int url_append_u32(char *buf, size_t cap, size_t *o, uint32_t v) {
  char tmp[10];
  size_t n = 0;
  if (v == 0) {
    return url_append_ch(buf, cap, o, '0');
  }
  while (v > 0) {
    tmp[n++] = (char)('0' + (int)(v % 10u));
    v /= 10u;
  }
  while (n > 0) {
    n--;
    if (url_append_ch(buf, cap, o, tmp[n]) != 0) {
      return -1;
    }
  }
  return 0;
}

int capy_url_serialize(const struct capy_url *url, char *out_buf,
                       size_t out_cap) {
  size_t o = 0;

  if (!url || !out_buf || out_cap == 0) {
    return CAPY_URL_ERR_NULL;
  }

  if (url_append(out_buf, out_cap, &o, url->scheme) != 0 ||
      url_append_ch(out_buf, out_cap, &o, ':') != 0) {
    return CAPY_URL_ERR_OVERFLOW;
  }
  if (url->has_authority) {
    if (url_append(out_buf, out_cap, &o, "//") != 0 ||
        url_append(out_buf, out_cap, &o, url->host) != 0) {
      return CAPY_URL_ERR_OVERFLOW;
    }
    if (url->has_port) {
      if (url_append_ch(out_buf, out_cap, &o, ':') != 0 ||
          url_append_u32(out_buf, out_cap, &o, url->port) != 0) {
        return CAPY_URL_ERR_OVERFLOW;
      }
    }
  }
  if (url_append(out_buf, out_cap, &o, url->path) != 0) {
    return CAPY_URL_ERR_OVERFLOW;
  }
  if (url->has_query) {
    if (url_append_ch(out_buf, out_cap, &o, '?') != 0 ||
        url_append(out_buf, out_cap, &o, url->query) != 0) {
      return CAPY_URL_ERR_OVERFLOW;
    }
  }
  if (url->has_fragment) {
    if (url_append_ch(out_buf, out_cap, &o, '#') != 0 ||
        url_append(out_buf, out_cap, &o, url->fragment) != 0) {
      return CAPY_URL_ERR_OVERFLOW;
    }
  }

  out_buf[o] = '\0';
  return (int)o;
}
