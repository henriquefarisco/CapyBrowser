/*
 * CapyBrowse Text - host fetch/read backends (outside the capy-browser-core ABI).
 *
 * Two always-available, dependency-free readers (local file, stdin) plus an
 * opt-in HTTPS backend (libcurl), compiled only when CAPY_HOST_HAVE_CURL is
 * defined. This is the only place in the reference front-end that performs I/O;
 * src/ never does. HTTPS-first is enforced here.
 */

#include "capy_host.h"
#include "url_parse.h"

#include <stdio.h>
#include <string.h>

/* Clear the response-metadata fields so every filled payload is well-defined. */
static void capy_host_reset_meta(struct capy_host_payload *out) {
  out->truncated = 0;
  out->http_status = 0;
  out->effective_url[0] = '\0';
  out->content_type[0] = '\0';
  out->content_disposition[0] = '\0';
}

int capy_host_payload_append(struct capy_host_payload *out,
                             const unsigned char *data, size_t len) {
  if (!out || !out->buf || (!data && len != 0) || out->len > out->cap) {
    return CAPY_HOST_ERR_ARGS;
  }
  if (len > out->cap - out->len) {
    out->truncated = 1;
    return CAPY_HOST_ERR_TOO_LARGE;
  }
  if (len != 0) {
    memcpy(out->buf + out->len, data, len);
    out->len += len;
  }
  return CAPY_HOST_OK;
}

int capy_host_read_file(const char *path, struct capy_host_payload *out) {
  FILE *f;
  if (!path || !out || !out->buf || out->cap == 0) {
    return CAPY_HOST_ERR_ARGS;
  }
  capy_host_reset_meta(out);
  f = fopen(path, "rb");
  if (!f) {
    return CAPY_HOST_ERR_OPEN;
  }
  out->len = fread(out->buf, 1, out->cap, f);
  if (ferror(f)) {
    fclose(f);
    out->len = 0;
    return CAPY_HOST_ERR_READ;
  }
  if (out->len == out->cap) {
    int extra = fgetc(f);
    if (extra != EOF) {
      out->truncated = 1;
      fclose(f);
      return CAPY_HOST_ERR_TOO_LARGE;
    }
    if (ferror(f)) {
      fclose(f);
      out->len = 0;
      return CAPY_HOST_ERR_READ;
    }
  }
  fclose(f);
  return CAPY_HOST_OK;
}

int capy_host_read_stdin(struct capy_host_payload *out) {
  if (!out || !out->buf || out->cap == 0) {
    return CAPY_HOST_ERR_ARGS;
  }
  capy_host_reset_meta(out);
  out->len = fread(out->buf, 1, out->cap, stdin);
  if (ferror(stdin)) {
    out->len = 0;
    return CAPY_HOST_ERR_READ;
  }
  if (out->len == out->cap) {
    int extra = fgetc(stdin);
    if (extra != EOF) {
      out->truncated = 1;
      return CAPY_HOST_ERR_TOO_LARGE;
    }
    if (ferror(stdin)) {
      out->len = 0;
      return CAPY_HOST_ERR_READ;
    }
  }
  return CAPY_HOST_OK;
}

int capy_host_prepare_url(const char *in, char *out, size_t cap) {
  struct capy_url url;
  char tmp[CAPY_URL_MAX_LEN + 16];
  const char *target;
  int rc;

  if (!in || !out || cap == 0) {
    return CAPY_HOST_ERR_ARGS;
  }
  target = in;
  if (strstr(in, "://") == NULL) {
    int n = snprintf(tmp, sizeof(tmp), "https://%s", in);
    if (n < 0 || (size_t)n >= sizeof(tmp)) {
      return CAPY_HOST_ERR_ARGS;
    }
    target = tmp;
  }
  rc = capy_url_parse(target, NULL, &url, NULL);
  if (rc != CAPY_URL_OK) {
    return CAPY_HOST_ERR_ARGS;
  }
  /* HTTPS-first: anything that is not https is refused at the adapter. */
  if (strcmp(url.scheme, "https") != 0) {
    return CAPY_HOST_ERR_SCHEME;
  }
  if (capy_url_serialize(&url, out, cap) < 0) {
    return CAPY_HOST_ERR_ARGS;
  }
  return CAPY_HOST_OK;
}

#ifdef CAPY_HOST_HAVE_CURL

#include <curl/curl.h>

/*
 * Append complete chunks only. Returning 0 makes libcurl abort immediately on
 * overflow instead of downloading and silently discarding the rest.
 */
static size_t capy_host_write_cb(char *ptr, size_t size, size_t nmemb,
                                 void *userp) {
  struct capy_host_payload *p = (struct capy_host_payload *)userp;
  size_t n = size * nmemb;
  if (size != 0 && n / size != nmemb) {
    p->truncated = 1;
    return 0;
  }
  return capy_host_payload_append(p, (const unsigned char *)ptr, n) ==
                 CAPY_HOST_OK
             ? n
             : 0;
}

/* Capture the Content-Disposition header value into the payload (case-insens). */
static size_t capy_host_header_cb(char *buffer, size_t size, size_t nitems,
                                  void *userp) {
  struct capy_host_payload *p = (struct capy_host_payload *)userp;
  const char *pfx = "content-disposition:";
  size_t plen = strlen(pfx);
  size_t n = size * nitems;
  size_t i;
  size_t j = 0;
  if (p == NULL || n < plen) {
    return n;
  }
  for (i = 0; i < plen; i++) {
    char c = buffer[i];
    if (c >= 'A' && c <= 'Z') {
      c = (char)(c - 'A' + 'a');
    }
    if (c != pfx[i]) {
      return n;
    }
  }
  i = plen;
  while (i < n && (buffer[i] == ' ' || buffer[i] == '\t')) {
    i++;
  }
  while (i < n && buffer[i] != '\r' && buffer[i] != '\n' &&
         j + 1 < sizeof(p->content_disposition)) {
    p->content_disposition[j++] = buffer[i++];
  }
  p->content_disposition[j] = '\0';
  return n;
}

int capy_host_fetch_https(const char *url, const char *user_agent,
                          const char *referer, struct capy_host_payload *out) {
  CURL *curl;
  CURLcode res;
  long code = 0;

  if (!url || !out || !out->buf || out->cap == 0) {
    return CAPY_HOST_ERR_ARGS;
  }
  out->len = 0;
  capy_host_reset_meta(out);
  /* HTTPS-first, enforced at the adapter before any I/O. */
  if (strncmp(url, "https://", 8) != 0) {
    return CAPY_HOST_ERR_SCHEME;
  }

  curl = curl_easy_init();
  if (!curl) {
    return CAPY_HOST_ERR_NETWORK;
  }

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, capy_host_write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)out);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, capy_host_header_cb);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void *)out);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
  /* Never auto-follow a non-HTTPS redirect; restrict every hop to https. */
#if defined(CURL_AT_LEAST_VERSION) && CURL_AT_LEAST_VERSION(7, 85, 0)
  curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
  curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https");
#else
  curl_easy_setopt(curl, CURLOPT_PROTOCOLS, (long)CURLPROTO_HTTPS);
  curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, (long)CURLPROTO_HTTPS);
#endif
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
  /* Minimal, static identity (aligns with the future private-session UA). */
  curl_easy_setopt(curl, CURLOPT_USERAGENT,
                   (user_agent && user_agent[0]) ? user_agent : "CapyBrowse");
  if (referer != NULL && referer[0] != '\0') {
    curl_easy_setopt(curl, CURLOPT_REFERER, referer);
  }
  curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

  res = curl_easy_perform(curl);
  if (res == CURLE_OK) {
    char *ct = NULL;
    char *effective = NULL;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    out->http_status = code;
    if (curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective) == CURLE_OK &&
        effective != NULL) {
      size_t k = strlen(effective);
      if (k > CAPY_HOST_URL_MAX) {
        curl_easy_cleanup(curl);
        out->len = 0;
        return CAPY_HOST_ERR_TOO_LARGE;
      }
      memcpy(out->effective_url, effective, k + 1);
    }
    if (curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ct) == CURLE_OK &&
        ct != NULL) {
      size_t k = 0;
      while (ct[k] != '\0' && k + 1 < sizeof(out->content_type)) {
        out->content_type[k] = ct[k];
        k++;
      }
      out->content_type[k] = '\0';
    }
  }
  curl_easy_cleanup(curl);

  if (res != CURLE_OK) {
    if (out->truncated && res == CURLE_WRITE_ERROR) {
      out->len = 0;
      return CAPY_HOST_ERR_TOO_LARGE;
    }
    out->len = 0;
    return CAPY_HOST_ERR_NETWORK;
  }
  if (code < 200 || code >= 300) {
    out->len = 0;
    return CAPY_HOST_ERR_HTTP;
  }
  if (out->effective_url[0] == '\0' ||
      strncmp(out->effective_url, "https://", 8) != 0) {
    out->len = 0;
    return CAPY_HOST_ERR_SCHEME;
  }
  return CAPY_HOST_OK;
}

#else /* !CAPY_HOST_HAVE_CURL */

int capy_host_fetch_https(const char *url, const char *user_agent,
                          const char *referer, struct capy_host_payload *out) {
  (void)url;
  (void)user_agent;
  (void)referer;
  if (out) {
    out->len = 0;
    capy_host_reset_meta(out);
  }
  return CAPY_HOST_ERR_DISABLED;
}

#endif /* CAPY_HOST_HAVE_CURL */

const char *capy_host_status_name(int status) {
  switch (status) {
  case CAPY_HOST_OK:
    return "OK";
  case CAPY_HOST_ERR_ARGS:
    return "ARGS";
  case CAPY_HOST_ERR_SCHEME:
    return "SCHEME";
  case CAPY_HOST_ERR_OPEN:
    return "OPEN";
  case CAPY_HOST_ERR_READ:
    return "READ";
  case CAPY_HOST_ERR_TOO_LARGE:
    return "TOO_LARGE";
  case CAPY_HOST_ERR_NETWORK:
    return "NETWORK";
  case CAPY_HOST_ERR_HTTP:
    return "HTTP";
  case CAPY_HOST_ERR_DISABLED:
    return "DISABLED";
  default:
    return "UNKNOWN";
  }
}
