/*
 * capy-browser-core: private/anonymous session base (Fase M4, part b).
 *
 * Pure computation of the per-navigation request identity. No I/O. Uses the
 * Fase C1 URL core for origin comparison and serialization. Deterministic.
 */

#include "session.h"

#include <string.h>

#define CAPY_REQUEST_UA "CapyBrowse"

static void set_ua(struct capy_request_identity *o) {
  /* minimal, static, no version leak */
  memcpy(o->user_agent, CAPY_REQUEST_UA, sizeof(CAPY_REQUEST_UA));
}

int capy_request_identity(enum capy_session_mode mode, const char *current_url,
                          const char *target_url,
                          struct capy_request_identity *out) {
  struct capy_url cur;
  struct capy_url tgt;
  struct capy_url_origin oc;
  struct capy_url_origin ot;

  if (out == NULL) {
    return CAPY_SESSION_ERR_NULL;
  }
  out->ephemeral = (mode == CAPY_SESSION_PRIVATE) ? 1 : 0;
  out->allow_third_party = (mode == CAPY_SESSION_PRIVATE) ? 0 : 1;
  out->send_referer = 0;
  out->referer[0] = '\0';
  set_ua(out);

  /* A private session never emits a Referer. */
  if (mode == CAPY_SESSION_PRIVATE) {
    return CAPY_SESSION_OK;
  }
  /* No referring page or no target: nothing to compute. */
  if (current_url == NULL || target_url == NULL) {
    return CAPY_SESSION_OK;
  }
  if (capy_url_parse(current_url, NULL, &cur, NULL) != CAPY_URL_OK) {
    return CAPY_SESSION_OK;
  }
  if (capy_url_parse(target_url, NULL, &tgt, NULL) != CAPY_URL_OK) {
    return CAPY_SESSION_OK;
  }
  /* Never leak an HTTPS referrer to a non-HTTPS target. */
  if (strcmp(cur.scheme, "https") == 0 && strcmp(tgt.scheme, "https") != 0) {
    return CAPY_SESSION_OK;
  }
  if (capy_url_origin(&cur, &oc) != CAPY_URL_OK ||
      capy_url_origin(&tgt, &ot) != CAPY_URL_OK) {
    return CAPY_SESSION_OK;
  }

  /* Drop the fragment in every case; cross-origin also drops path/query. */
  cur.has_fragment = 0;
  cur.fragment[0] = '\0';
  if (!capy_url_origin_equal(&oc, &ot)) {
    cur.path[0] = '/';
    cur.path[1] = '\0';
    cur.has_query = 0;
    cur.query[0] = '\0';
  }
  if (capy_url_serialize(&cur, out->referer, sizeof(out->referer)) < 0) {
    out->referer[0] = '\0';
    return CAPY_SESSION_OK;
  }
  out->send_referer = 1;
  return CAPY_SESSION_OK;
}

const char *capy_session_mode_name(enum capy_session_mode m) {
  switch (m) {
    case CAPY_SESSION_NORMAL:
      return "normal";
    case CAPY_SESSION_PRIVATE:
      return "private";
  }
  return "unknown";
}
