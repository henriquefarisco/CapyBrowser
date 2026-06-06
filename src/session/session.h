#ifndef CAPY_SESSION_H
#define CAPY_SESSION_H

/*
 * capy-browser-core: private/anonymous session base (Fase M4, part b).
 *
 * The pure, deterministic application-layer half of the private-session
 * surface: given a session mode and the (current page, target) URLs, it
 * computes the request identity CapyBrowser recommends to the host adapter --
 * the ephemeral-storage flag, whether automatic third-party loads are allowed,
 * a minimal static User-Agent with no version leak, and the Referer value unde
 * a strict-origin-when-cross-origin policy (zeroed entirely in private mode).
 *
 * CapyBrowser owns only this application layer. Transport anonymity (proxy /
 * onion routing / private DNS), the cookie/cache storage backends the ephemeral
 * flag toggles, and certificate policy are CapyOS responsibilities. This
 * surface performs NO I/O and never changes parse/display determinism.
 */

#include <stddef.h>

#include "url_parse.h" /* CAPY_URL_MAX_LEN, capy_url_parse / origin */

#define CAPY_REQUEST_UA_MAX 128u

enum capy_session_status {
  CAPY_SESSION_OK = 0,
  CAPY_SESSION_ERR_NULL = -1
};

enum capy_session_mode {
  CAPY_SESSION_NORMAL = 0,
  CAPY_SESSION_PRIVATE = 1
};

/*
 * The request identity for one navigation. In a private session: ephemeral is
 * 1 (host must not persist cookies/cache), third-party loads are disallowed and
 * no Referer is sent. The User-Agent is always the minimal static identity.
 */
struct capy_request_identity {
  int ephemeral;         /* 1 = host must not persist cookies/cache */
  int allow_third_party; /* 0 = no automatic third-party/external loads */
  int send_referer;      /* 1 = send the referer field below */
  char user_agent[CAPY_REQUEST_UA_MAX + 1];
  char referer[CAPY_URL_MAX_LEN + 1]; /* empty unless send_referer */
};

/*
 * Compute the request identity. current_url (the referring page) and target_url
 * may be NULL (then no Referer is computed). Always resets *out. Returns
 * CAPY_SESSION_OK or CAPY_SESSION_ERR_NULL on a NULL out.
 *
 * Referer policy (normal mode only): same-origin -> the full current URL minus
 * fragment; cross-origin -> the current origin only; an HTTPS->non-HTTPS
 * downgrade or any unparseable URL -> no Referer. Private mode never sends one.
 */
int capy_request_identity(enum capy_session_mode mode, const char *current_url,
                          const char *target_url,
                          struct capy_request_identity *out);

const char *capy_session_mode_name(enum capy_session_mode m);

#endif /* CAPY_SESSION_H */
