/*    g_draw.cpp
 *
 *    Copyright (c) 2008, eFTE SF Group (see AUTHORS file)
 *    Copyright (c) 1994-1996, Marko Macek
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

#include "console.h"
#include "utf8.h"

#ifdef  NTCONSOLE
#   define  WIN32_LEAN_AND_MEAN 1
#   include <windows.h>
#endif

int CStrLen(const char *p) {
    int len = 0, was = 0;
    while (*p) {
        len++;
        if (*p == '&' && !was) {
            len--;
            was = 1;
        }
        p++;
        was = 0;
    }
    return len;
}

#ifndef NTCONSOLE

void MoveCh(PCell B, char CCh, TAttr Attr, int Count) {
    TCell cell = TCELL_MAKE1((unsigned char)CCh, Attr);
    while (Count-- > 0)
        *B++ = cell;
}

void MoveChar(PCell B, int Pos, int Width, const char CCh, TAttr Attr, int Count) {
    if (Pos < 0) { Count += Pos; Pos = 0; }
    if (Pos >= Width) return;
    if (Pos + Count > Width) Count = Width - Pos;
    if (Count <= 0) return;
    TCell cell = TCELL_MAKE1((unsigned char)CCh, Attr);
    B += Pos;
    while (Count-- > 0)
        *B++ = cell;
}

int MoveMem(PCell B, int Pos, int Width, const char *Ch, TAttr Attr, int Count) {
    if (Pos < 0) { Count += Pos; Ch -= Pos; Pos = 0; }
    if (Pos >= Width || Count <= 0) return 0;

    B += Pos;
    const char *src = Ch;
    int remaining = Count;
    int col = Pos;

    while (remaining > 0 && col < Width) {
        unsigned char c0 = (unsigned char)*src;

        if (c0 < 0x80) {
            *B++ = TCELL_MAKE1(c0, Attr);
            src++; remaining--; col++;
        } else if (utf8_is_cont(c0)) {
            /* lone continuation byte — skip, don't allocate a column */
            src++; remaining--;
        } else {
            int consumed;
            unsigned long cp = utf8_decode(src, &consumed);
            int w = utf8_codepoint_width(cp);
            if (consumed > remaining) consumed = remaining;

            unsigned char b1 = (consumed >= 2) ? (unsigned char)src[1] : 0;
            unsigned char b2 = (consumed >= 3) ? (unsigned char)src[2] : 0;
            *B++ = TCELL_MAKE(c0, b1, b2, Attr);
            src += consumed; remaining -= consumed; col++;

            if (w == 2 && col < Width) {
                *B++ = TCELL_MAKE1(0, Attr);
                col++;
            }
        }
    }
    return col - Pos; /* columns written */
}

void MoveStr(PCell B, int Pos, int Width, const char *Ch, TAttr Attr, int MaxCount) {
    if (Pos < 0) { MaxCount += Pos; Ch -= Pos; Pos = 0; }
    if (Pos >= Width) return;
    if (MaxCount <= 0) return;

    B += Pos;
    int col = Pos;

    while (*Ch && col < Width) {
        unsigned char c0 = (unsigned char)*Ch;

        if (c0 < 0x80) {
            /* ASCII */
            *B++ = TCELL_MAKE1(c0, Attr);
            Ch++;
            col++;
        } else {
            /* UTF-8 multi-byte */
            int consumed;
            unsigned long cp = utf8_decode(Ch, &consumed);
            int w = utf8_codepoint_width(cp);

            unsigned char b0 = c0;
            unsigned char b1 = (consumed >= 2) ? (unsigned char)Ch[1] : 0;
            unsigned char b2 = (consumed >= 3) ? (unsigned char)Ch[2] : 0;
            /* 4-byte sequences: store first 3 bytes, output layer uses utf8_col_buf */

            *B++ = TCELL_MAKE(b0, b1, b2, Attr);
            Ch += consumed;
            col++;

            /* double-width: write a zero-char placeholder */
            if (w == 2 && col < Width) {
                *B++ = TCELL_MAKE1(0, Attr);
                col++;
            }
        }
    }
}

void MoveCStr(PCell B, int Pos, int Width, const char *Ch, TAttr A0, TAttr A1, int MaxCount) {
    if (Pos < 0) { MaxCount += Pos; Ch -= Pos; Pos = 0; }
    if (Pos >= Width) return;
    if (Pos + MaxCount > Width) MaxCount = Width - Pos;
    if (MaxCount <= 0) return;
    B += Pos;
    int was = 0;
    while (MaxCount > 0 && *Ch) {
        if (*Ch == '&' && !was) { Ch++; MaxCount++; was = 1; continue; }
        TAttr a = was ? A1 : A0;
        *B++ = TCELL_MAKE1((unsigned char)*Ch++, a);
        was = 0;
        MaxCount--;
    }
}

void MoveAttr(PCell B, int Pos, int Width, TAttr Attr, int Count) {
    if (Pos < 0) { Count += Pos; Pos = 0; }
    if (Pos >= Width) return;
    if (Pos + Count > Width) Count = Width - Pos;
    if (Count <= 0) return;
    B += Pos;
    while (Count-- > 0) {
        /* preserve character bytes, replace attr */
        *B = (*B & 0x00FFFFFF) | ((unsigned int)Attr << 24);
        B++;
    }
}

void MoveBgAttr(PCell B, int Pos, int Width, TAttr Attr, int Count) {
    if (Pos < 0) { Count += Pos; Pos = 0; }
    if (Pos >= Width) return;
    if (Pos + Count > Width) Count = Width - Pos;
    if (Count <= 0) return;
    B += Pos;
    while (Count-- > 0) {
        unsigned char a = TCELL_ATTR(*B);
        a = (a & 0x0F) | (Attr & 0xF0);
        *B = (*B & 0x00FFFFFF) | ((unsigned int)a << 24);
        B++;
    }
}

#else

void MoveCh(PCell B, char Ch, TAttr Attr, int Count) {
    PCHAR_INFO p = (PCHAR_INFO) B;
    while (Count > 0) {
        p->Char.AsciiChar = Ch;
        p->Attributes = Attr;
        p++;
        Count--;
    }
}

void MoveChar(PCell B, int Pos, int Width, const char Ch, TAttr Attr, int Count) {
    PCHAR_INFO p = (PCHAR_INFO) B;
    if (Pos < 0) {
        Count += Pos;
        Pos = 0;
    }
    if (Pos >= Width) return;
    if (Pos + Count > Width) Count = Width - Pos;
    if (Count <= 0) return;
    for (p += Pos; Count > 0; Count--) {
        p->Char.AsciiChar = Ch;
        p->Attributes = Attr;
        p++;
    }
}

void MoveMem(PCell B, int Pos, int Width, const char* Ch, TAttr Attr, int Count) {
    PCHAR_INFO p = (PCHAR_INFO) B;

    if (Pos < 0) {
        Count += Pos;
        Ch -= Pos;
        Pos = 0;
    }
    if (Pos >= Width) return;
    if (Pos + Count > Width) Count = Width - Pos;
    if (Count <= 0) return;
    for (p += Pos; Count > 0; Count--) {
        p->Char.AsciiChar = *Ch++;
        p->Attributes = Attr;
        p++;
    }
}

void MoveStr(PCell B, int Pos, int Width, const char* Ch, TAttr Attr, int MaxCount) {
    PCHAR_INFO p = (PCHAR_INFO) B;

    if (Pos < 0) {
        MaxCount += Pos;
        Ch -= Pos;
        Pos = 0;
    }
    if (Pos >= Width) return;
    if (Pos + MaxCount > Width) MaxCount = Width - Pos;
    if (MaxCount <= 0) return;
    for (p += Pos; MaxCount > 0 && (*Ch != 0); MaxCount--) {
        p->Char.AsciiChar = *Ch++;
        p->Attributes = Attr;
        p++;
    }
}

void MoveCStr(PCell B, int Pos, int Width, const char* Ch, TAttr A0, TAttr A1, int MaxCount) {
    PCHAR_INFO p = (PCHAR_INFO) B;
    char was;
    //TAttr A;

    if (Pos < 0) {
        MaxCount += Pos;
        Ch -= Pos;
        Pos = 0;
    }
    if (Pos >= Width) return;
    if (Pos + MaxCount > Width) MaxCount = Width - Pos;
    if (MaxCount <= 0) return;
    was = 0;
    for (p += Pos; MaxCount > 0 && (*Ch != 0); MaxCount--) {
        if (*Ch == '&' && !was) {
            Ch++;
            MaxCount++;
            was = 1;
            continue;
        }
        p->Char.AsciiChar = (unsigned char)(*Ch++);
        if (was) {
            p->Attributes = A1;
            was = 0;
        } else
            p->Attributes = A0;
        p++;
    }
}

void MoveAttr(PCell B, int Pos, int Width, TAttr Attr, int Count) {
    PCHAR_INFO p = (PCHAR_INFO) B;

    if (Pos < 0) {
        Count += Pos;
        Pos = 0;
    }
    if (Pos >= Width) return;
    if (Pos + Count > Width) Count = Width - Pos;
    if (Count <= 0) return;
    for (p += Pos; Count > 0; Count--, p++)
        p->Attributes = Attr;
}

void MoveBgAttr(PCell B, int Pos, int Width, TAttr Attr, int Count) {
    PCHAR_INFO p = (PCHAR_INFO) B;

    if (Pos < 0) {
        Count += Pos;
        Pos = 0;
    }
    if (Pos >= Width) return;
    if (Pos + Count > Width) Count = Width - Pos;
    if (Count <= 0) return;
    for (p += Pos; Count > 0; Count--) {
        p->Attributes =
            ((unsigned char)(p->Attributes & 0xf)) |
            ((unsigned char) Attr);
        p++;
    }
}

#endif
