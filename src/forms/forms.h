#ifndef CAPY_FORMS_H
#define CAPY_FORMS_H

/*
 * capy-browser-core: static form submission (Fase M4, part c).
 *
 * The pure, deterministic core of (non-scripted) form submit: given a method,
 * an action URL (+ optional base) and a set of name/value fields, it
 * `application/x-www-form-urlencoded`-encodes the fields and builds the request
 * descriptor -- for GET the action URL with its query replaced by the encoded
 * data, for POST the resolved action URL plus the encoded body and content
 * type. HTTPS-first; fail-closed on overflow. It performs NO I/O: the actual
 * fetch is the host adapter's job. No JavaScript is involved.
 */

#include <stddef.h>

#include "url_parse.h" /* CAPY_URL_MAX_LEN, capy_url_parse / serialize */

/* Maximum encoded body/query length (alpha; configurable per stage). */
#define CAPY_FORM_BODY_MAX 4096u

enum capy_form_status {
  CAPY_FORM_OK = 0,
  CAPY_FORM_ERR_NULL = -1,
  CAPY_FORM_ERR_URL = -2,     /* action invalid / unresolvable */
  CAPY_FORM_ERR_SCHEME = -3,  /* non-HTTPS action (HTTPS-first) */
  CAPY_FORM_ERR_OVERFLOW = -4, /* encoded data exceeds a budget */
  CAPY_FORM_ERR_METHOD = -5    /* method is neither GET nor POST */
};

enum capy_form_method {
  CAPY_FORM_GET = 0,
  CAPY_FORM_POST = 1
};

/* One form field. Both pointers must be NUL-terminated and non-NULL. */
struct capy_form_field {
  const char *name;
  const char *value;
};

struct capy_form_request {
  enum capy_form_method method;
  char url[CAPY_URL_MAX_LEN + 1];   /* resolved target; GET includes ?query */
  char body[CAPY_FORM_BODY_MAX + 1]; /* POST: encoded body; GET: empty */
  size_t body_len;
  const char *content_type; /* POST: the urlencoded type; GET: "" */
};

/*
 * Build a form-submission request. Pure, deterministic, fail-closed.
 *
 *   method      : GET or POST.
 *   action      : the form action reference (absolute or relative).
 *   base_url     : optional absolute base for resolving a relative action.
 *   fields/field_count : the name/value pairs (may be NULL only if count 0).
 *   out         : receives the method, resolved URL, body and content type.
 *
 * Returns CAPY_FORM_OK, or a negative enum capy_form_status. A method other
 * than GET/POST fails closed with CAPY_FORM_ERR_METHOD. When `out` is non-NULL,
 * it is always reset before any other argument is validated, so an error never
 * leaves a stale URL/body from a previous request.
 */
int capy_form_submit(enum capy_form_method method, const char *action,
                     const char *base_url, const struct capy_form_field *fields,
                     size_t field_count, struct capy_form_request *out);

const char *capy_form_method_name(enum capy_form_method m);

#endif /* CAPY_FORMS_H */
