/*    dch_unicode.h
 *
 *    Mapping from eFTE internal draw codes (the DCH_* indices in console.h,
 *    as emitted by ConGetDrawChar()) to the Unicode code points used by the
 *    Unicode-aware display backends (Xft).
 *
 *    This is the single source of truth. Every place that renders a cell to
 *    Unicode — the line renderer and the cursor renderer — must go through
 *    DchToUnicode(). A second copy of this table is exactly how the cell
 *    under the cursor ends up showing a different glyph than the rest of the
 *    line.
 *
 *    The values are indexed by (draw code + 1), i.e. the 1..21 range produced
 *    by ConGetDrawChar(); index 0 is unused. Kept as plain unsigned int so the
 *    header carries no dependency on Xft/fontconfig (FcChar32 is unsigned int).
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 */

#ifndef DCH_UNICODE_H
#define DCH_UNICODE_H

static inline unsigned int DchToUnicode(unsigned int ucs4, int slen) {
    static const unsigned int DchUnicodeTable[] = {
        0,
        0x250C, /*  1: DCH_C1     ┌ */
        0x2510, /*  2: DCH_C2     ┐ */
        0x2514, /*  3: DCH_C3     └ */
        0x2518, /*  4: DCH_C4     ┘ */
        0x2500, /*  5: DCH_H      ─ */
        0x2502, /*  6: DCH_V      │ */
        0x251C, /*  7: DCH_M1     ├ */
        0x2524, /*  8: DCH_M2     ┤ */
        0x2192, /*  9: DCH_M3     → */
        0x2534, /* 10: DCH_M4     ┴ */
        0x253C, /* 11: DCH_X      ┼ */
        0x25B6, /* 12: DCH_RPTR   ▶ */
        0x00B7, /* 13: DCH_EOL    · (end of line)   */
        0x2666, /* 14: DCH_EOF    ♦ (end of file)   */
        0x2500, /* 15: DCH_END    ─ (end of buffer) */
        0x25B2, /* 16: DCH_AUP    ▲ */
        0x25BC, /* 17: DCH_ADOWN  ▼ */
        0x2592, /* 18: DCH_HFORE  ▒ */
        0x2591, /* 19: DCH_HBACK  ░ */
        0x25C0, /* 20: DCH_ALEFT  ◀ */
        0x25B6  /* 21: DCH_ARIGHT ▶ */
    };

    if (slen == 1 && ucs4 >= 1 && ucs4 <= 21)
        return DchUnicodeTable[ucs4];
    if (ucs4 < 32)        /* other non-printables -> blank, avoid .notdef boxes */
        return ' ';
    return ucs4;
}

#endif /* DCH_UNICODE_H */