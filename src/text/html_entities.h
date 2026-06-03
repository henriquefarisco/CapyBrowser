#ifndef CAPY_HTML_ENTITIES_H
#define CAPY_HTML_ENTITIES_H

#include <stddef.h>
#include <stdint.h>

/* Sentinel code point used to flag an out-of-range / invalid numeric ref. */
#define CAPY_CP_INVALID 0x110000u

/* Encode a Unicode code point as UTF-8 into out[0..3]. Returns the byte count
 * (1..4), or 0 for an invalid code point (surrogate or > 0x10FFFF). */
size_t capy_utf8_encode(uint32_t cp, char out[4]);

/* Look up a named HTML entity spelled without '&'/';' (e.g. "amp"). Returns 1
 * and sets *cp on an exact match, 0 otherwise. */
int capy_html_entity_lookup(const char *name, size_t len, uint32_t *cp);

/* Try to decode a character reference starting at s[0]=='&' within s[0..len).
 * On success returns the number of input bytes consumed (including '&' and the
 * trailing ';') and sets *cp to the decoded code point (CAPY_CP_INVALID for an
 * out-of-range numeric ref; the caller still validates control points). Returns
 * 0 when s does not start a well-formed reference (caller emits '&' literally). */
size_t capy_html_charref_at(const char *s, size_t len, uint32_t *cp);

#endif /* CAPY_HTML_ENTITIES_H */
