/*
 * CapyBrowse Text - reference host front-end (CLI text browser).
 *
 * Wires the pure browser-core surfaces into a usable command-line text browser:
 *   Fase C1 (URL parse/normalize) validates and normalizes the address;
 *   Fase C2 (HTML-to-text) renders the page to the deterministic text view.
 * All side effects (HTTPS fetch, file read) live in the host layer (host/),
 * never in src/, preserving the decoupling contract. No JavaScript is executed.
 *
 * Modes:
 *   capybrowse <https-url> [-i]        fetch + render (needs the network build)
 *   capybrowse --file <path> [--base <url>]   render a local HTML file
 *   capybrowse - [--base <url>]        render HTML from stdin
 */

#include "capy_host.h"
#include "download.h"
#include "html_text.h"
#include "session.h"
#include "url_parse.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum capy_cli_mode { CLI_MODE_URL = 0, CLI_MODE_FILE = 1, CLI_MODE_STDIN = 2 };

#define CAPY_HISTORY_MAX 64u

/* Fixed buffers (no allocation). Input is C2's budget + 1 so an over-budget
 * page is seen by C2 as too long and reported with INPUT_TRUNCATED. */
static unsigned char g_input[CAPY_TEXT_MAX_INPUT + 1];
static char g_body[CAPY_TEXT_MAX_INPUT + 1];
static struct capy_text_doc g_doc;
static char g_base[CAPY_URL_MAX_LEN + 1];
static char g_current[CAPY_URL_MAX_LEN + 1]; /* current page (Referer source) */
static enum capy_session_mode g_mode = CAPY_SESSION_NORMAL;
static size_t g_max_input = 0;    /* 0 = use the full buffer (C2 input budget) */
static unsigned g_page_lines = 0; /* 0 = no pagination */

/* Back-navigation stack of visited (normalized) URLs; top = current page. */
static char g_history[CAPY_HISTORY_MAX][CAPY_URL_MAX_LEN + 1];
static size_t g_history_len = 0;

/* Effective input cap: the buffer size, optionally reduced by --max-bytes. */
static size_t input_cap(void) {
  size_t cap = sizeof(g_input);
  if (g_max_input != 0 && g_max_input < cap) {
    cap = g_max_input;
  }
  return cap;
}

/* Push a normalized URL onto the history stack (dropping the oldest if full). */
static void history_push(const char *url) {
  size_t n = strlen(url);
  size_t i;
  if (n > CAPY_URL_MAX_LEN) {
    n = CAPY_URL_MAX_LEN;
  }
  if (g_history_len == CAPY_HISTORY_MAX) {
    size_t k;
    for (k = 1; k < CAPY_HISTORY_MAX; k++) {
      memcpy(g_history[k - 1], g_history[k], CAPY_URL_MAX_LEN + 1);
    }
    g_history_len--;
  }
  i = g_history_len;
  memcpy(g_history[i], url, n);
  g_history[i][n] = '\0';
  g_history_len++;
}

static void usage(const char *argv0) {
  fprintf(stderr,
          "CapyBrowse Text - reference host text browser (capy-browser-core)\n"
          "\n"
          "usage:\n"
          "  %s <https-url> [-i]               fetch + render (network build)\n"
          "  %s --file <path> [--base <url>]   render a local HTML file\n"
          "  %s - [--base <url>]               render HTML from stdin\n"
          "\n"
          "options:\n"
          "  --base <url>        absolute base URL to resolve relative links\n"
          "  --max-bytes <n>     cap the bytes read for a page\n"
          "  --page <n>          paginate the body every <n> lines (0=off)\n"
          "  -i, --interactive   follow numbered links / 'b' back (URL mode)\n"
          "  --private           ephemeral session: minimal UA, no Referer\n"
          "  -h, --help          show this help\n"
          "\n"
          "notes: HTTPS-first (non-HTTPS is refused); no JavaScript runs.\n",
          argv0, argv0, argv0);
}

/* Thin diagnostic wrapper over the pure host URL-prep (enforces HTTPS-first). */
static int prepare_url(const char *in, char *out, size_t cap) {
  int rc = capy_host_prepare_url(in, out, cap);
  if (rc == CAPY_HOST_ERR_SCHEME) {
    fprintf(stderr, "error: HTTPS-first - refusing non-HTTPS URL\n");
  } else if (rc != CAPY_HOST_OK) {
    fprintf(stderr, "error: invalid URL\n");
  }
  return rc;
}

/* Print the body, optionally paging every page_lines blocks (reads stdin). */
static void print_body(const char *body, unsigned page_lines) {
  size_t i;
  unsigned lines = 0;
  if (page_lines == 0) {
    printf("%s\n", body);
    return;
  }
  for (i = 0; body[i] != '\0'; i++) {
    putchar((unsigned char)body[i]);
    if (body[i] == '\n') {
      lines++;
      if (lines % page_lines == 0 && body[i + 1] != '\0') {
        char tmp[16];
        printf("-- more -- (enter=continue, q=stop): ");
        fflush(stdout);
        if (!fgets(tmp, sizeof(tmp), stdin)) {
          return;
        }
        if (tmp[0] == 'q' || tmp[0] == 'Q') {
          return;
        }
      }
    }
  }
  putchar('\n');
}

static void render(const char *page_url, const struct capy_text_doc *doc,
                   const char *body, unsigned page_lines) {
  size_t i;
  if (page_url && page_url[0] != '\0') {
    printf("URL: %s\n", page_url);
  }
  if (doc->has_title) {
    printf("Title: %s\n", doc->title);
  }
  printf("\n");
  print_body(body, page_lines);
  if (doc->link_count > 0) {
    printf("\nLinks:\n");
    for (i = 0; i < doc->link_count; i++) {
      if (doc->links[i].text[0] != '\0') {
        printf("  [%zu] %s\n      %s\n", i + 1, doc->links[i].text,
               doc->links[i].url);
      } else {
        printf("  [%zu] %s\n", i + 1, doc->links[i].url);
      }
    }
  }
  if (doc->warnings.count > 0) {
    fprintf(stderr, "warnings:");
    for (i = 0; i < doc->warnings.count; i++) {
      fprintf(stderr, " %s", capy_text_warning_name(doc->warnings.codes[i]));
    }
    fprintf(stderr, "\n");
  }
  if (doc->truncated) {
    fprintf(stderr, "note: output truncated (a resource limit was reached)\n");
  }
}

/* True if the response should be rendered as text (HTML/plain) rather than
 * saved as a download. An empty/unknown type defaults to text (prior behavior). */
static int ct_is_textual(const char *ct) {
  static const char pfx[] = "text/";
  size_t i;
  if (ct == NULL || ct[0] == '\0') {
    return 1;
  }
  for (i = 0; pfx[i] != '\0'; i++) {
    char c = ct[i];
    if (c >= 'A' && c <= 'Z') {
      c = (char)(c - 'A' + 'a');
    }
    if (c != pfx[i]) {
      return 0;
    }
  }
  return 1;
}

/* Save a non-HTML response under a filename derived by the M4a download core.
 * Fail-closed: a truncated transfer or a rejected verdict is not written. The
 * sanitized filename has no path component, so the write stays in the cwd. */
static int save_download(const char *url, const struct capy_host_payload *pay) {
  struct capy_download dl;
  const char *cd;
  FILE *f;
  if (pay->truncated) {
    fprintf(stderr,
            "download exceeds the %lu-byte buffer; not saved "
            "(streaming arrives with the Etapa 7 adapter)\n",
            (unsigned long)pay->cap);
    return CAPY_HOST_ERR_TOO_LARGE;
  }
  cd = (pay->content_disposition[0] != '\0') ? pay->content_disposition : NULL;
  capy_download_prepare(url, NULL, cd, (long)pay->len, 0, &dl);
  if (dl.verdict != CAPY_DOWNLOAD_ACCEPT) {
    fprintf(stderr, "download rejected: %s\n",
            capy_download_verdict_name(dl.verdict));
    return CAPY_HOST_ERR_READ;
  }
  f = fopen(dl.filename, "wb");
  if (f == NULL) {
    fprintf(stderr, "cannot open %s for writing\n", dl.filename);
    return CAPY_HOST_ERR_OPEN;
  }
  if (fwrite(pay->buf, 1, pay->len, f) != pay->len) {
    fclose(f);
    fprintf(stderr, "short write to %s\n", dl.filename);
    return CAPY_HOST_ERR_READ;
  }
  fclose(f);
  printf("saved %lu bytes to %s\n", (unsigned long)pay->len, dl.filename);
  return CAPY_HOST_OK;
}

/* Resolve+fetch+render a URL into g_doc/g_body. Returns capy_host_status. */
static int load_url(const char *url) {
  struct capy_host_payload pay;
  int rc;

  pay.buf = g_input;
  pay.cap = input_cap();
  pay.len = 0;

  rc = prepare_url(url, g_base, sizeof(g_base));
  if (rc != CAPY_HOST_OK) {
    return rc;
  }
  {
    struct capy_request_identity id;
    const char *referer;
    (void)capy_request_identity(
        g_mode, (g_current[0] != '\0') ? g_current : NULL, g_base, &id);
    referer = id.send_referer ? id.referer : NULL;
    rc = capy_host_fetch_https(g_base, id.user_agent, referer, &pay);
  }
  if (rc != CAPY_HOST_OK) {
    fprintf(stderr, "fetch failed: %s\n", capy_host_status_name(rc));
    if (rc == CAPY_HOST_ERR_DISABLED) {
      fprintf(stderr, "hint: rebuild with 'make capybrowse-net' for HTTPS, "
                      "or use --file / - for offline input\n");
    }
    return rc;
  }
  if (!ct_is_textual(pay.content_type)) {
    return save_download(g_base, &pay);
  }
  if (capy_html_to_text((const uint8_t *)pay.buf, pay.len, g_base, g_body,
                        sizeof(g_body), &g_doc) != CAPY_TEXT_OK) {
    fprintf(stderr, "render failed\n");
    return CAPY_HOST_ERR_READ;
  }
  render(g_base, &g_doc, g_body, g_page_lines);
  memcpy(g_current, g_base, sizeof(g_current));
  return CAPY_HOST_OK;
}

/* Navigate to a URL; on success optionally push it onto the history stack. */
static int navigate(const char *url, int push) {
  int rc = load_url(url);
  if (rc == CAPY_HOST_OK && push) {
    history_push(g_base);
  }
  return rc;
}

/* Read+render a local file or stdin into g_doc/g_body. */
static int load_local(enum capy_cli_mode mode, const char *source,
                      const char *base) {
  struct capy_host_payload pay;
  int rc;

  pay.buf = g_input;
  pay.cap = input_cap();
  pay.len = 0;

  rc = (mode == CLI_MODE_FILE) ? capy_host_read_file(source, &pay)
                               : capy_host_read_stdin(&pay);
  if (rc != CAPY_HOST_OK) {
    fprintf(stderr, "read failed: %s\n", capy_host_status_name(rc));
    return rc;
  }
  if (capy_html_to_text((const uint8_t *)pay.buf, pay.len, base, g_body,
                        sizeof(g_body), &g_doc) != CAPY_TEXT_OK) {
    fprintf(stderr, "render failed\n");
    return CAPY_HOST_ERR_READ;
  }
  /* Pagination prompts read stdin, so disable it when HTML came from stdin. */
  render(base ? base : source, &g_doc, g_body,
         (mode == CLI_MODE_FILE) ? g_page_lines : 0);
  return CAPY_HOST_OK;
}

/* Follow numbered links (and 'b' back) until the user quits. */
static void interactive_loop(void) {
  char line[80];
  char target[CAPY_URL_MAX_LEN + 1];
  for (;;) {
    size_t n = g_doc.link_count;
    long sel;
    printf("\n[capybrowse] link number");
    if (n > 0) {
      printf(" (1-%zu)", n);
    }
    printf(", 'b' back, 'q' quit: ");
    fflush(stdout);
    if (!fgets(line, sizeof(line), stdin)) {
      return;
    }
    if (line[0] == 'q' || line[0] == 'Q' || line[0] == '\n' ||
        line[0] == '\0') {
      return;
    }
    if (line[0] == 'b' || line[0] == 'B') {
      if (g_history_len < 2) {
        fprintf(stderr, "(no page to go back to)\n");
        continue;
      }
      memcpy(target, g_history[g_history_len - 2], sizeof(target));
      target[sizeof(target) - 1] = '\0';
      if (navigate(target, 0) == CAPY_HOST_OK) {
        g_history_len--; /* drop the page we left */
      } else {
        fprintf(stderr, "back failed\n");
      }
      continue;
    }
    if (n == 0) {
      fprintf(stderr, "(no links on this page)\n");
      continue;
    }
    sel = strtol(line, NULL, 10);
    if (sel < 1 || (size_t)sel > n) {
      fprintf(stderr, "invalid selection\n");
      continue;
    }
    /* Copy before navigate overwrites g_doc with the next page. */
    memcpy(target, g_doc.links[(size_t)sel - 1].url, sizeof(target));
    target[sizeof(target) - 1] = '\0';
    (void)navigate(target, 1); /* on failure, keep the current page */
  }
}

int main(int argc, char **argv) {
  const char *source = NULL;
  const char *base_override = NULL;
  enum capy_cli_mode mode = CLI_MODE_URL;
  int interactive = 0;
  int i;

  for (i = 1; i < argc; i++) {
    const char *a = argv[i];
    if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
      usage(argv[0]);
      return 0;
    } else if (strcmp(a, "-i") == 0 || strcmp(a, "--interactive") == 0) {
      interactive = 1;
    } else if (strcmp(a, "--private") == 0) {
      g_mode = CAPY_SESSION_PRIVATE;
    } else if (strcmp(a, "--file") == 0) {
      if (++i >= argc) {
        usage(argv[0]);
        return 2;
      }
      mode = CLI_MODE_FILE;
      source = argv[i];
    } else if (strcmp(a, "--base") == 0) {
      if (++i >= argc) {
        usage(argv[0]);
        return 2;
      }
      base_override = argv[i];
    } else if (strcmp(a, "--max-bytes") == 0) {
      if (++i >= argc) {
        usage(argv[0]);
        return 2;
      }
      g_max_input = (size_t)strtoul(argv[i], NULL, 10);
    } else if (strcmp(a, "--page") == 0) {
      if (++i >= argc) {
        usage(argv[0]);
        return 2;
      }
      g_page_lines = (unsigned)strtoul(argv[i], NULL, 10);
    } else if (strcmp(a, "-") == 0) {
      mode = CLI_MODE_STDIN;
      source = "-";
    } else if (a[0] == '-') {
      fprintf(stderr, "unknown option: %s\n", a);
      usage(argv[0]);
      return 2;
    } else {
      mode = CLI_MODE_URL;
      source = a;
    }
  }

  if (mode == CLI_MODE_URL && !source) {
    usage(argv[0]);
    return 2;
  }

  if (mode == CLI_MODE_URL) {
    if (navigate(source, 1) != CAPY_HOST_OK) {
      return 1;
    }
    if (interactive) {
      interactive_loop();
    }
    return 0;
  }

  return (load_local(mode, source, base_override) == CAPY_HOST_OK) ? 0 : 1;
}
