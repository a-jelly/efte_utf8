/*    utf8.h
 *
 *    UTF-8 support for eFTE
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 */

#ifndef UTF8_H
#define UTF8_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Is this byte a UTF-8 continuation byte? (10xxxxxx)
 */
static inline int utf8_is_cont(unsigned char c) {
    return (c & 0xC0) == 0x80;
}

/*
 * Is this byte the start of a multi-byte UTF-8 sequence?
 */
static inline int utf8_is_lead(unsigned char c) {
    return (c >= 0x80) && !utf8_is_cont(c);
}

/*
 * How many bytes does the UTF-8 sequence starting with this byte occupy?
 * Returns 1 for ASCII and any invalid/continuation bytes (safe fallback).
 */
static inline int utf8_seqlen(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1; /* continuation or invalid - treat as 1 byte */
}

/*
 * Decode one UTF-32 codepoint from a UTF-8 string.
 * *bytes_consumed is set to the number of bytes consumed (1..4).
 * On invalid input, returns the raw byte value and consumes 1 byte.
 */
unsigned long utf8_decode(const char *s, int *bytes_consumed);

/*
 * How many terminal columns does a Unicode codepoint occupy?
 * Uses wcwidth(); returns 1 for unknown/control characters.
 */
int utf8_codepoint_width(unsigned long codepoint);

/*
 * How many terminal columns does a single UTF-8 sequence starting at *s
 * occupy? Also advances *s past the sequence and decrements *len
 * by the number of bytes consumed.
 */
int utf8_char_width(const char **s, int *len);

/*
 * Convert a byte offset in a UTF-8 line to a screen column.
 * Tabs are expanded using TabSize; tab stops are at multiples of TabSize.
 * If ExpandTabs is false, column == byte offset (legacy behaviour).
 */
int utf8_byte_to_col(const char *chars, int char_count,
                     int byte_offset,
                     int expand_tabs, int tab_size);

/*
 * Convert a screen column to the byte offset in a UTF-8 line.
 * If ExpandTabs is false, byte offset == column (legacy behaviour).
 */
int utf8_col_to_byte(const char *chars, int char_count,
                     int col,
                     int expand_tabs, int tab_size);

/*
 * Per-column UTF-8 sequence buffer.
 * For each screen column we store the full UTF-8 byte sequence of the
 * character that occupies that column. Written by the hilit layer via
 * utf8_cell_set(), read by ConPutBox via utf8_col_buf[].
 * len == 0 means "use the byte from TCell directly" (ASCII path).
 */
#define UTF8_COL_BUF_MAX 256

struct Utf8Cell {
    unsigned char bytes[4];
    int           len;
};

extern Utf8Cell utf8_col_buf[UTF8_COL_BUF_MAX];

void utf8_cell_set(int col, const char *seq, int len);
void utf8_cell_clear(int col);

#ifdef __cplusplus
}
#endif

#endif /* UTF8_H */
