#ifndef CAPY_HOST_H
#define CAPY_HOST_H

/*
 * CapyBrowse Text - reference host front-end (outside the capy-browser-core ABI).
 *
 * This layer is the decoupled "host adapter" the contract describes: it lives
 * outside src/ and supplies the side effects (filesystem read, HTTPS fetch) the
 * pure browser-core (Fase C1 URL + Fase C2 HTML-to-text) is forbidden from
 * doing itself. The core stays pure; only this layer touches the outside world.
 *
 * HTTPS-first is enforced here, at the adapter, exactly as the contract states:
 * a non-HTTPS fetch is rejected; the core only parses the URL.
 *
 * Nothing in this header is part of the capy-browser-core ABI. It adds no erro
 * code, warning or display-list node to the owned ABI.
 */

#include <stddef.h>

/* Host-layer status (not a browser-core error code). 0 on success. */
enum capy_host_status {
  CAPY_HOST_OK = 0,
  CAPY_HOST_ERR_ARGS = -1,      /* NULL/invalid argument */
  CAPY_HOST_ERR_SCHEME = -2,    /* non-HTTPS rejected (HTTPS-first) */
  CAPY_HOST_ERR_OPEN = -3,      /* could not open the source */
  CAPY_HOST_ERR_READ = -4,      /* read failure */
  CAPY_HOST_ERR_TOO_LARGE = -5, /* payload exceeded the buffer budget */
  CAPY_HOST_ERR_NETWORK = -6,   /* transport failure */
  CAPY_HOST_ERR_HTTP = -7,      /* non-success HTTP status */
  CAPY_HOST_ERR_DISABLED = -8   /* built without a network backend */
};

/*
 * A payload read into a caller-owned fixed buffer. The host layer neve
 * allocates: the caller provides buf/cap and the layer fills len (<= cap).
 * A source larger than cap fails with CAPY_HOST_ERR_TOO_LARGE; network
 * transfers are aborted at the first overflowing chunk instead of silently
 * discarding bytes or rendering an incomplete response.
 */
#define CAPY_HOST_CT_MAX 127u
#define CAPY_HOST_CD_MAX 511u
#define CAPY_HOST_URL_MAX 2048u

struct capy_host_payload {
  unsigned char *buf;
  size_t cap;
  size_t len;
  int truncated;                                  /* response bytes were dropped */
  long http_status;                               /* final HTTP status, or 0 */
  char effective_url[CAPY_HOST_URL_MAX + 1];      /* final URL after redirects */
  char content_type[CAPY_HOST_CT_MAX + 1];        /* response metadata, filled */
  char content_disposition[CAPY_HOST_CD_MAX + 1]; /* by fetch; empty if unknown */
};

/* Bounded append used by streaming adapters. It never partially accepts a
 * chunk: overflow sets truncated and returns CAPY_HOST_ERR_TOO_LARGE. */
int capy_host_payload_append(struct capy_host_payload *out,
                             const unsigned char *data, size_t len);

/* Read up to cap bytes of a local file into the payload (always available). */
int capy_host_read_file(const char *path, struct capy_host_payload *out);

/* Read up to cap bytes of stdin into the payload (always available). */
int capy_host_read_stdin(struct capy_host_payload *out);

/*
 * Fetch an absolute HTTPS URL into the payload buffer.
 *
 * HTTPS-first: a URL whose scheme is not "https" is rejected with
 * CAPY_HOST_ERR_SCHEME before any I/O. Redirects, when followed, are restricted
 * to HTTPS. Built without a network backend this returns CAPY_HOST_ERR_DISABLED
 * (use a local file or stdin instead). user_agent (NULL/empty = a built-in
 * default) and referer (NULL/empty = none) carry the session request identity.
 * On success, out->effective_url is the final HTTPS URL after redirects and
 * out->http_status is the final 2xx status.
 */
int capy_host_fetch_https(const char *url, const char *user_agent,
                          const char *referer, struct capy_host_payload *out);

/*
 * Validate + normalize a user-typed address into out (serialized, NUL-
 * terminated). A reference with no scheme is assumed to be https. Enforces
 * HTTPS-first: a non-"https" scheme is rejected with CAPY_HOST_ERR_SCHEME and a
 * malformed/over-long reference with CAPY_HOST_ERR_ARGS. Pure (no I/O); the same
 * input always yields the same verdict and bytes. Implemented over the Fase C1
 * URL core.
 */
int capy_host_prepare_url(const char *in, char *out, size_t cap);

/* Stable, human-readable name for a status code (diagnostics). */
const char *capy_host_status_name(int status);

#endif /* CAPY_HOST_H */
