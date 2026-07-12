/*
 * capy-browser-core: download preparation (Fase M4, part a).
 *
 * Pure decision core: validate the URL (HTTPS-first via Fase C1), derive a safe
 * filename from Content-Disposition (filename / RFC 5987 filename*) or the URL
 * path, and enforce the size budget. No I/O. Deterministic and fail-closed.
 */

#include "download.h"

#include <string.h>

#define CD_BUF 512u

static int ci_eq(char a, char b) {
  if (a >= 'A' && a <= 'Z') {
    a = (char)(a - 'A' + 'a');
  }
  if (b >= 'A' && b <= 'Z') {
    b = (char)(b - 'A' + 'a');
  }
  return a == b;
}

static int cd_is_ows(char c) { return c == ' ' || c == '\t'; }

/* Return the value of an exact Content-Disposition parameter. Segments are
 * separated only by semicolons outside quoted strings, preventing a lookalike
 * such as `notfilename=` (or text inside a quoted value) from being accepted as
 * the security-sensitive filename parameter. */
static const char *cd_find_param(const char *cd, const char *name) {
  const char *segment = cd;
  size_t name_len = strlen(name);
  while (*segment != '\0') {
    const char *p = segment;
    int in_quote = 0;
    int escaped = 0;
    size_t i = 0;
    while (cd_is_ows(*p)) {
      p++;
    }
    while (i < name_len && p[i] != '\0' && ci_eq(p[i], name[i])) {
      i++;
    }
    if (i == name_len && p[i] == '=') {
      return p + i + 1;
    }

    for (; *p != '\0'; p++) {
      if (escaped) {
        escaped = 0;
      } else if (in_quote && *p == '\\') {
        escaped = 1;
      } else if (*p == '"') {
        in_quote = !in_quote;
      } else if (!in_quote && *p == ';') {
        segment = p + 1;
        break;
      }
    }
    if (*p == '\0') {
      break;
    }
  }
  return NULL;
}

static int ishex(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
         (c >= 'A' && c <= 'F');
}

static int hexval(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  return c - 'A' + 10;
}

/*
 * Extract a raw filename value from a Content-Disposition header into dst
 * (NUL-terminated). Prefers RFC 5987 `filename*=` (percent-decoded) ove
 * `filename=` (quoted or token). Returns 1 if a non-empty value was found.
 */
static int cd_extract_filename(const char *cd, char *dst, size_t cap,
                               size_t *out_len) {
  const char *p;
  size_t n = 0;

  p = cd_find_param(cd, "filename*");
  if (p != NULL) {
    const char *q1 = NULL;
    const char *s;
    /* skip the charset'lang' prefix: advance past the second quote */
    for (s = p; *s != '\0' && *s != ';'; s++) {
      if (*s == '\'') {
        if (q1 == NULL) {
          q1 = s;
        } else {
          p = s + 1;
          break;
        }
      }
    }
    while (*p != '\0' && *p != ';' && n + 1 < cap) {
      if (*p == '%' && p[1] != '\0' && p[2] != '\0' && ishex(p[1]) &&
          ishex(p[2])) {
        dst[n++] = (char)((hexval(p[1]) << 4) | hexval(p[2]));
        p += 3;
      } else {
        dst[n++] = *p;
        p++;
      }
    }
    dst[n] = '\0';
    *out_len = n;
    return n > 0;
  }

  p = cd_find_param(cd, "filename");
  if (p != NULL) {
    if (*p == '"') {
      p++;
      while (*p != '\0' && *p != '"' && n + 1 < cap) {
        dst[n++] = *p++;
      }
    } else {
      while (*p != '\0' && *p != ';' && *p != ' ' && *p != '\t' &&
             n + 1 < cap) {
        dst[n++] = *p++;
      }
    }
    dst[n] = '\0';
    *out_len = n;
    return n > 0;
  }

  dst[0] = '\0';
  *out_len = 0;
  return 0;
}

/* Copy the URL's last path segment into dst (NUL-terminated); returns length. */
static size_t url_last_segment(const struct capy_url *u, char *dst,
                               size_t cap) {
  const char *seg = u->path;
  const char *s;
  size_t n = 0;
  for (s = u->path; *s != '\0'; s++) {
    if (*s == '/') {
      seg = s + 1;
    }
  }
  while (seg[n] != '\0' && n + 1 < cap) {
    dst[n] = seg[n];
    n++;
  }
  dst[n] = '\0';
  return n;
}

/*
 * Reduce src to its basename, drop control bytes and path separators, bound to
 * cap, into dst (NUL-terminated). Returns 1 if the result is a usable name, 0
 * if it is empty, "." or "..".
 */
static int sanitize_filename(const char *src, char *dst, size_t cap) {
  const char *base = src;
  const char *s;
  size_t n = 0;
  for (s = src; *s != '\0'; s++) {
    if (*s == '/' || *s == '\\') {
      base = s + 1;
    }
  }
  for (s = base; *s != '\0' && n + 1 < cap; s++) {
    unsigned char c = (unsigned char)*s;
    if (c < 0x20u || c == 0x7Fu || c == '/' || c == '\\') {
      continue;
    }
    dst[n++] = (char)c;
  }
  dst[n] = '\0';
  if (n == 0) {
    return 0;
  }
  if (strcmp(dst, ".") == 0 || strcmp(dst, "..") == 0) {
    return 0;
  }
  return 1;
}

int capy_download_prepare(const char *url, const char *base_url,
                          const char *content_disposition, long content_length,
                          long max_size, struct capy_download *out) {
  struct capy_url u;
  char cand[CD_BUF];
  size_t clen = 0;
  int from_cd = 0;

  if (url == NULL || out == NULL) {
    return CAPY_DOWNLOAD_ERR_NULL;
  }
  out->verdict = CAPY_DOWNLOAD_ACCEPT;
  out->url[0] = '\0';
  out->filename[0] = '\0';

  if (capy_url_parse(url, base_url, &u, NULL) != CAPY_URL_OK) {
    out->verdict = CAPY_DOWNLOAD_REJECT_URL;
    return CAPY_DOWNLOAD_OK;
  }
  if (strcmp(u.scheme, "https") != 0) {
    out->verdict = CAPY_DOWNLOAD_REJECT_SCHEME;
    return CAPY_DOWNLOAD_OK;
  }
  if (capy_url_serialize(&u, out->url, sizeof(out->url)) < 0) {
    out->url[0] = '\0';
    out->verdict = CAPY_DOWNLOAD_REJECT_URL;
    return CAPY_DOWNLOAD_OK;
  }
  if (max_size > 0 && content_length >= 0 && content_length > max_size) {
    out->verdict = CAPY_DOWNLOAD_REJECT_TOO_LARGE;
    return CAPY_DOWNLOAD_OK;
  }

  if (content_disposition != NULL &&
      cd_extract_filename(content_disposition, cand, sizeof(cand), &clen)) {
    from_cd = 1;
  } else {
    url_last_segment(&u, cand, sizeof(cand));
  }

  if (!sanitize_filename(cand, out->filename, sizeof(out->filename))) {
    if (from_cd) {
      out->filename[0] = '\0';
      out->verdict = CAPY_DOWNLOAD_REJECT_FILENAME;
      return CAPY_DOWNLOAD_OK;
    }
    memcpy(out->filename, "download", sizeof("download"));
  }

  out->verdict = CAPY_DOWNLOAD_ACCEPT;
  return CAPY_DOWNLOAD_OK;
}

const char *capy_download_verdict_name(enum capy_download_verdict v) {
  switch (v) {
    case CAPY_DOWNLOAD_ACCEPT:
      return "ACCEPT";
    case CAPY_DOWNLOAD_REJECT_URL:
      return "REJECT_URL";
    case CAPY_DOWNLOAD_REJECT_SCHEME:
      return "REJECT_SCHEME";
    case CAPY_DOWNLOAD_REJECT_TOO_LARGE:
      return "REJECT_TOO_LARGE";
    case CAPY_DOWNLOAD_REJECT_FILENAME:
      return "REJECT_FILENAME";
  }
  return "UNKNOWN";
}
