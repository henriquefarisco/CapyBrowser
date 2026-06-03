#include "url_parse.h"

#include <string.h>

int capy_url_origin(const struct capy_url *url, struct capy_url_origin *out) {
  if (!url || !out) {
    return CAPY_URL_ERR_NULL;
  }
  strcpy(out->scheme, url->scheme);
  strcpy(out->host, url->host);
  out->port = url->port; /* effective port (explicit or scheme default) */
  return CAPY_URL_OK;
}

int capy_url_origin_equal(const struct capy_url_origin *a,
                          const struct capy_url_origin *b) {
  if (!a || !b) {
    return 0;
  }
  if (a->port != b->port) {
    return 0;
  }
  if (strcmp(a->scheme, b->scheme) != 0) {
    return 0;
  }
  if (strcmp(a->host, b->host) != 0) {
    return 0;
  }
  return 1;
}
