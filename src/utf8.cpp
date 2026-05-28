/*    utf8.cpp
 *
 *    UTF-8 support for eFTE
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 */

#include <wchar.h>
#include <string.h>
#include "utf8.h"

/* ------------------------------------------------------------------ */
/* Decode one codepoint                                                */
/* ------------------------------------------------------------------ */

unsigned long utf8_decode(const char *s, int *bytes_consumed) {
    unsigned char c = (unsigned char)*s;

    if (c < 0x80) {
        *bytes_consumed = 1;
        return c;
    }
    if ((c & 0xE0) == 0xC0 && (unsigned char)s[1] != 0) {
        unsigned char c2 = (unsigned char)s[1];
        if ((c2 & 0xC0) == 0x80) {
            *bytes_consumed = 2;
            return ((unsigned long)(c & 0x1F) << 6) | (c2 & 0x3F);
        }
    }
    if ((c & 0xF0) == 0xE0 && s[1] && s[2]) {
        unsigned char c2 = (unsigned char)s[1];
        unsigned char c3 = (unsigned char)s[2];
        if (((c2 & 0xC0) == 0x80) && ((c3 & 0xC0) == 0x80)) {
            *bytes_consumed = 3;
            return ((unsigned long)(c & 0x0F) << 12) |
                   ((unsigned long)(c2 & 0x3F) << 6) |
                   (c3 & 0x3F);
        }
    }
    if ((c & 0xF8) == 0xF0 && s[1] && s[2] && s[3]) {
        unsigned char c2 = (unsigned char)s[1];
        unsigned char c3 = (unsigned char)s[2];
        unsigned char c4 = (unsigned char)s[3];
        if (((c2 & 0xC0) == 0x80) && ((c3 & 0xC0) == 0x80) &&
            ((c4 & 0xC0) == 0x80)) {
            *bytes_consumed = 4;
            return ((unsigned long)(c & 0x07) << 18) |
                   ((unsigned long)(c2 & 0x3F) << 12) |
                   ((unsigned long)(c3 & 0x3F) << 6) |
                   (c4 & 0x3F);
        }
    }
    /* Invalid or lone continuation byte — consume 1 byte, pass it through */
    *bytes_consumed = 1;
    return c;
}

/* ------------------------------------------------------------------ */
/* Column width of a codepoint                                         */
/* ------------------------------------------------------------------ */

int utf8_codepoint_width(unsigned long cp) {
    wchar_t wc = (wchar_t)cp;
    int w = wcwidth(wc);
    if (w < 0) w = 1;   /* control chars, unknowns */
    if (w == 0) w = 1;  /* combining marks: treat as 1 for safety */
    return w;
}

/* ------------------------------------------------------------------ */
/* Width of one UTF-8 character (advances pointer)                    */
/* ------------------------------------------------------------------ */

int utf8_char_width(const char **s, int *len) {
    int consumed;
    unsigned long cp;

    if (*len <= 0) return 0;

    cp = utf8_decode(*s, &consumed);
    if (consumed > *len) consumed = *len;

    *s   += consumed;
    *len -= consumed;

    return utf8_codepoint_width(cp);
}

/* ------------------------------------------------------------------ */
/* byte offset → screen column                                         */
/* ------------------------------------------------------------------ */

int utf8_byte_to_col(const char *chars, int char_count,
                     int byte_offset,
                     int expand_tabs, int tab_size) {
    if (!expand_tabs)
        return byte_offset;

    const char *p = chars;
    int remaining = char_count;
    int col = 0;
    int byte_pos = 0;

    while (remaining > 0 && byte_pos < byte_offset) {
        unsigned char c = (unsigned char)*p;

        if (c == '\t') {
            /* next tab stop */
            int next_tab = (col / tab_size + 1) * tab_size;
            col = next_tab;
            p++;
            remaining--;
            byte_pos++;
        } else {
            int consumed;
            unsigned long cp = utf8_decode(p, &consumed);
            if (consumed > remaining) consumed = remaining;

            /* only count column for the lead byte */
            col += utf8_codepoint_width(cp);

            p         += consumed;
            remaining -= consumed;
            byte_pos  += consumed;
        }
    }

    /* cursor past end of line */
    if (byte_pos < byte_offset)
        col += byte_offset - byte_pos;

    return col;
}

/* ------------------------------------------------------------------ */
/* screen column → byte offset                                         */
/* ------------------------------------------------------------------ */

int utf8_col_to_byte(const char *chars, int char_count,
                     int col,
                     int expand_tabs, int tab_size) {
    if (!expand_tabs)
        return col;

    const char *p = chars;
    int remaining = char_count;
    int cur_col = 0;
    int byte_pos = 0;

    while (remaining > 0) {
        unsigned char c = (unsigned char)*p;

        if (c == '\t') {
            int next_tab = (cur_col / tab_size + 1) * tab_size;
            if (next_tab > col) return byte_pos;
            cur_col = next_tab;
            p++;
            remaining--;
            byte_pos++;
        } else {
            int consumed;
            unsigned long cp = utf8_decode(p, &consumed);
            if (consumed > remaining) consumed = remaining;

            int w = utf8_codepoint_width(cp);
            if (cur_col + w > col) return byte_pos;
            cur_col += w;

            p         += consumed;
            remaining -= consumed;
            byte_pos  += consumed;
        }
    }

    /* col past end of line */
    return byte_pos + (col - cur_col);
}

/* ------------------------------------------------------------------ */
/* Per-column UTF-8 parallel buffer                                    */
/* ------------------------------------------------------------------ */

Utf8Cell utf8_col_buf[UTF8_COL_BUF_MAX];

void utf8_cell_set(int col, const char *seq, int len) {
    if (col < 0 || col >= UTF8_COL_BUF_MAX) return;
    if (len < 1 || len > 4) len = 1;
    utf8_col_buf[col].len = len;
    for (int i = 0; i < len; i++)
        utf8_col_buf[col].bytes[i] = (unsigned char)seq[i];
}

void utf8_cell_clear(int col) {
    if (col >= 0 && col < UTF8_COL_BUF_MAX)
        utf8_col_buf[col].len = 0;
}
