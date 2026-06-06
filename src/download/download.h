#ifndef CAPY_DOWNLOAD_H
#define CAPY_DOWNLOAD_H

/*
 * capy-browser-core: download preparation (Fase M4, part a).
 *
 * The pure, deterministic decision core of the download surface: given a target
 * URL (+ optional base), the response's Content-Disposition and declared size,
 * and the declared limits, it validates the URL (HTTPS-first via Fase C1),
 * derives and sanitizes a safe filename, and enforces the size budget -- all
 * fail-closed. It performs NO I/O: opening a socket, fetching bytes and writing
 * the file are the host's job (the streaming sink callbacks arrive in part b).
 *
 * Filename safety: a server-supplied Content-Disposition name is reduced to its
 * basename (path separators stripped), control bytes dropped and length bounded;
 * a name that reduces to empty / "." / ".." is rejected fail-closed. When there
 * is no Content-Disposition, the name is derived from the URL's last path
 * segment, falling back to "download" when that is unusable.
 */

#include <stddef.h>

#include "url_parse.h" /* CAPY_URL_MAX_LEN, capy_url_parse */

/* Maximum derived filename length (alpha target; see docs/compatibility.md). */
#define CAPY_DOWNLOAD_FILENAME_MAX 255u

enum capy_download_status {
  CAPY_DOWNLOAD_OK = 0,
  CAPY_DOWNLOAD_ERR_NULL = -1
};

/* The deterministic verdict. ACCEPT fills url + filename; a reject leaves them
 * empty. Additive: append new reject reasons; never renumber. */
enum capy_download_verdict {
  CAPY_DOWNLOAD_ACCEPT = 0,
  CAPY_DOWNLOAD_REJECT_URL = 1,       /* URL invalid / unresolvable */
  CAPY_DOWNLOAD_REJECT_SCHEME = 2,    /* non-HTTPS (HTTPS-first) */
  CAPY_DOWNLOAD_REJECT_TOO_LARGE = 3, /* declared size over the budget */
  CAPY_DOWNLOAD_REJECT_FILENAME = 4   /* server-supplied name unusable/unsafe */
};

struct capy_download {
  enum capy_download_verdict verdict;
  char url[CAPY_URL_MAX_LEN + 1];                /* resolved, normalized */
  char filename[CAPY_DOWNLOAD_FILENAME_MAX + 1]; /* sanitized basename */
};

/*
 * Prepare a download decision. Pure, deterministic, fail-closed.
 *
 *   url / base_url      : target reference + optional absolute base (Fase C1).
 *   content_disposition : raw header value, or NULL.
 *   content_length      : declared length in bytes, or negative if unknown.
 *   max_size            : maximum accepted size in bytes; 0 means no limit.
 *   out                 : receives the verdict and (on ACCEPT) url + filename.
 *
 * Returns CAPY_DOWNLOAD_OK (the decision is in out->verdict) o
 * CAPY_DOWNLOAD_ERR_NULL on a NULL url/out. Always resets *out first.
 */
int capy_download_prepare(const char *url, const char *base_url,
                          const char *content_disposition, long content_length,
                          long max_size, struct capy_download *out);

const char *capy_download_verdict_name(enum capy_download_verdict v);

#endif /* CAPY_DOWNLOAD_H */
