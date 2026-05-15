#ifndef UTF8_UTILS_H
#define UTF8_UTILS_H
#include <stddef.h>
#include <string.h>
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline int utf8_seqlen(unsigned char b) {
    if (b < 0x80) return 1;
    if (b < 0xC0) return 1;
    if (b < 0xE0) return 2;
    if (b < 0xF0) return 3;
    return 4;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline size_t utf8_strlen(const char *s) {
    if (!s) return 0;
    size_t n = 0;
    while (*s) { s += utf8_seqlen((unsigned char)*s); n++; }
    return n;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline size_t utf8_char_offset(const char *s, size_t slen, size_t ch) {
    size_t off = 0, i = 1;
    while (off < slen && i < ch) { off += (size_t)utf8_seqlen((unsigned char)s[off]); i++; }
    return off;
}
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline size_t utf8_char_bytes(const char *s, size_t slen, size_t off, size_t n) {
    size_t start = off, i = 0;
    while (off < slen && i < n) { off += (size_t)utf8_seqlen((unsigned char)s[off]); i++; }
    return off - start;
}
#endif
