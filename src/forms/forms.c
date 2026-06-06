/*
 * capy-browser-core: static form submission (Fase M4, part c).
 *
 * Pure application/x-www-form-urlencoded encoding + request building over the
 * Fase C1 URL core. No I/O, no JavaScript. Deterministic and fail-closed.
 */

#include "forms.h"

#include <string.h>

static const char FORM_CONTENT_TYPE[] = "application/x-www-form-urlencoded";

static char form_hex_upper(int nibble) {
  return (nibble < 10) ? (char)('0' + nibble) : (char)('A' + nibble - 10);
}

/* The application/x-www-form-urlencoded "unreserved" set kept literal. */
static int form_is_literal(unsigned char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '*' || c == '-' || c == '.' ||
         c == '_';
}

/* Append the encoded form of src to buf[*len..]; returns 1 ok, 0 on overflow. */
static int form_encode(const char *src, char *buf, size_t cap, size_t *len) {
  size_t i;
  for (i = 0; src[i] != '\0'; i++) {
    unsigned char c = (unsigned char)src[i];
    if (c == ' ') {
      if (*len + 1 >= cap) {
        return 0;
      }
      buf[(*len)++] = '+';
    } else if (form_is_literal(c)) {
      if (*len + 1 >= cap) {
        return 0;
      }
      buf[(*len)++] = (char)c;
    } else {
      if (*len + 3 >= cap) {
        return 0;
      }
      buf[(*len)++] = '%';
      buf[(*len)++] = form_hex_upper((c >> 4) & 0xf);
      buf[(*len)++] = form_hex_upper(c & 0xf);
    }
  }
  return 1;
}

int capy_form_submit(enum capy_form_method method, const char *action,
                     const char *base_url, const struct capy_form_field *fields,
                     size_t field_count, struct capy_form_request *out) {
  char enc[CAPY_FORM_BODY_MAX + 1];
  size_t elen = 0;
  size_t i;
  struct capy_url u;

  if (action == NULL || out == NULL || (fields == NULL && field_count > 0)) {
    return CAPY_FORM_ERR_NULL;
  }
  out->method = method;
  out->url[0] = '\0';
  out->body[0] = '\0';
  out->body_len = 0;
  out->content_type = "";

  for (i = 0; i < field_count; i++) {
    if (fields[i].name == NULL || fields[i].value == NULL) {
      return CAPY_FORM_ERR_NULL;
    }
    if (i > 0) {
      if (elen + 1 >= sizeof(enc)) {
        return CAPY_FORM_ERR_OVERFLOW;
      }
      enc[elen++] = '&';
    }
    if (!form_encode(fields[i].name, enc, sizeof(enc), &elen)) {
      return CAPY_FORM_ERR_OVERFLOW;
    }
    if (elen + 1 >= sizeof(enc)) {
      return CAPY_FORM_ERR_OVERFLOW;
    }
    enc[elen++] = '=';
    if (!form_encode(fields[i].value, enc, sizeof(enc), &elen)) {
      return CAPY_FORM_ERR_OVERFLOW;
    }
  }
  enc[elen] = '\0';

  if (capy_url_parse(action, base_url, &u, NULL) != CAPY_URL_OK) {
    return CAPY_FORM_ERR_URL;
  }
  if (strcmp(u.scheme, "https") != 0) {
    return CAPY_FORM_ERR_SCHEME;
  }

  if (method == CAPY_FORM_GET) {
    if (elen > CAPY_URL_QUERY_MAX) {
      return CAPY_FORM_ERR_OVERFLOW;
    }
    memcpy(u.query, enc, elen);
    u.query[elen] = '\0';
    u.has_query = 1;
    if (capy_url_serialize(&u, out->url, sizeof(out->url)) < 0) {
      out->url[0] = '\0';
      return CAPY_FORM_ERR_OVERFLOW;
    }
  } else {
    if (capy_url_serialize(&u, out->url, sizeof(out->url)) < 0) {
      out->url[0] = '\0';
      return CAPY_FORM_ERR_OVERFLOW;
    }
    memcpy(out->body, enc, elen);
    out->body[elen] = '\0';
    out->body_len = elen;
    out->content_type = FORM_CONTENT_TYPE;
  }
  return CAPY_FORM_OK;
}

const char *capy_form_method_name(enum capy_form_method m) {
  switch (m) {
    case CAPY_FORM_GET:
      return "GET";
    case CAPY_FORM_POST:
      return "POST";
  }
  return "UNKNOWN";
}
