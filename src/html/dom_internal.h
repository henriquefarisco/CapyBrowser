#ifndef CAPY_DOM_INTERNAL_H
#define CAPY_DOM_INTERNAL_H

/*
 * Internal arena helpers shared between dom.c and html_parse.c. NOT part of the
 * public capy-browser-core ABI; consumers include only dom.h.
 */

#include "dom.h"

/* Append one byte to the string arena. Returns 1 on success, 0 if full (sets
 * the STRING_BUDGET warning + truncated). */
int capy_dom_arena_put(struct capy_dom_doc *doc, char c);

/*
 * Intern bytes into the string arena:
 *   - lower-cases ASCII letters when `lower` is set (for tag/attr names);
 *   - decodes HTML character references to UTF-8 when `decode` is set (for text
 *     and attribute values);
 *   - drops control bytes other than HTML whitespace (tab/LF/FF/CR), preserving
 *     whitespace (the DOM keeps it; collapsing is a layout concern).
 * Sets off/out_len to the interned range. Returns 1 if everything fit, 0 if
 * the arena filled (the partial range is still set).
 */
int capy_dom_intern(struct capy_dom_doc *doc, const char *src, size_t len,
                    int lower, int decode, size_t *off, size_t *out_len);

/* Allocate a node of `type`, returning its index or CAPY_DOM_NONE when the pool
 * is full (sets NODE_BUDGET + truncated). Tree links are initialized empty. */
size_t capy_dom_new_node(struct capy_dom_doc *doc,
                         enum capy_dom_node_type type);

/* Append `child` as the last child of `parent` (both valid node indices). */
void capy_dom_append_child(struct capy_dom_doc *doc, size_t parent,
                           size_t child);

/* Record a warning once, in fixed canonical order, saturating at the cap. */
void capy_dom_warn(struct capy_dom_doc *doc, enum capy_dom_warning code);

#endif /* CAPY_DOM_INTERNAL_H */
