/* utf8_utils.h — UTF-8 character-aware helpers for P3C
 * All functions operate on byte strings; "character" = Unicode code point.
 * Authors: Lon Jones Cherryholmes · Claude Sonnet 4.6  2026-04-05 */
#ifndef UTF8_UTILS_H
#define UTF8_UTILS_H
#include <stddef.h>
#include <string.h>

/* Number of bytes in the UTF-8 sequence starting at byte b (lead byte). */
static inline int utf8_seqlen(unsigned char b) {
    if (b < 0x80) return 1;
    if (b < 0xC0) return 1; /* bare continuation — treat as 1 to avoid loops */
    if (b < 0xE0) return 2;
    if (b < 0xF0) return 3;
    return 4;
}

/* Count UTF-8 code points in NUL-terminated string s. */
static inline size_t utf8_strlen(const char *s) {
    if (!s) return 0;
    size_t n = 0;
    while (*s) { s += utf8_seqlen((unsigned char)*s); n++; }
    return n;
}

/* Byte offset of the start of the ch-th code point (1-based) in s.
 * s has byte length slen.  Returns slen if ch > string length. */
static inline size_t utf8_char_offset(const char *s, size_t slen, size_t ch) {
    size_t off = 0, i = 1;
    while (off < slen && i < ch) { off += (size_t)utf8_seqlen((unsigned char)s[off]); i++; }
    return off;
}

/* Byte length of n code points starting at byte offset off in s (byte len slen). */
static inline size_t utf8_char_bytes(const char *s, size_t slen, size_t off, size_t n) {
    size_t start = off, i = 0;
    while (off < slen && i < n) { off += (size_t)utf8_seqlen((unsigned char)s[off]); i++; }
    return off - start;
}

#endif /* UTF8_UTILS_H */
