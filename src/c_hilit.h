/*    c_hilit.h
 *
 *    Copyright (c) 2008, eFTE SF Group (see AUTHORS file)
 *    Copyright (c) 1994-1996, Marko Macek
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

#ifndef HILIT_H_
#define HILIT_H_

#include "console.h"
#include "c_mode.h"
#include "e_regex.h"
#include "utf8.h"

#include <sys/types.h>

class EBuffer;
class ELine;

typedef unsigned short hlState;
typedef unsigned char hsState;

#define HILIT_P(proc) \
    int proc(EBuffer *BF, int LN, PCell B, int Pos, int Width, ELine *Line, hlState &State, hsState *StateMap, int *ECol)

//typedef int (*SyntaxProc)(EBuffer *BF, int LN, PCell B, int Pos, int Width, ELine *Line, hlState &State, hsState *StateMap);
typedef HILIT_P((*SyntaxProc));


int Indent_Plain(EBuffer *B, int Line, int PosCursor);
int Indent_Continue(EBuffer *B, int Line, int PosCursor);

HILIT_P(Hilit_Plain);

/* highlighting state */

HILIT_P(Hilit_C);
HILIT_P(Hilit_PERL);
HILIT_P(Hilit_MAKE);
HILIT_P(Hilit_REXX);
HILIT_P(Hilit_IPF);
HILIT_P(Hilit_ADA);
HILIT_P(Hilit_MSG);
HILIT_P(Hilit_SH);
HILIT_P(Hilit_PASCAL);
HILIT_P(Hilit_TEX);
HILIT_P(Hilit_FTE);
HILIT_P(Hilit_CATBS);
HILIT_P(Hilit_SIMPLE);

int Indent_C(EBuffer *B, int Line, int PosCursor);
int Indent_REXX(EBuffer *B, int Line, int PosCursor);
int Indent_SIMPLE(EBuffer *B, int Line, int PosCursor);

/*
 * NT has 2-byte charcode and attribute... Following is not portable to non-
 * intel; should be replaced by formal TCell definition' usage instead of
 * assumed array.. (Jal)
 */
#ifdef NTCONSOLE
#    define PCLI unsigned short
#else
#    define PCLI unsigned char
#endif

#define HILIT_CLRD() \
    ((Color < COUNT_CLR) ? Colors[Color] : Color - COUNT_CLR)

#ifndef X_MASK
#define X_MASK 0xFF
#endif

/*
 * UTF-8 aware rendering macros.
 *
 * i   - byte index into line (always advances by 1 byte)
 * p   - byte pointer (always advances by 1 byte)
 * len - bytes remaining (always decrements by 1)
 * C   - screen column (advances by wcwidth for lead byte, 0 for continuation)
 *
 * For a multi-byte UTF-8 character the lead byte occupies a TCell at
 * column C; continuation bytes are stored in *subsequent* TCell slots
 * (so ConPutBox can reassemble the sequence) but do NOT advance C.
 * A double-width lead byte causes C to advance by 2 (placeholder cell
 * is written automatically).
 */

/*
 * UTF-8 aware rendering macros for 32-bit TCell.
 *
 * TCell layout (Unix): bits 7..0 = b0, 15..8 = b1, 23..16 = b2, 31..24 = attr
 *
 * i, p, len  — byte index / pointer / remaining bytes in the source line
 * C           — screen column (advances by wcwidth, not by byte count)
 * B           — TDrawBuffer (PCell, one TCell per screen column)
 */

/* Write current character starting at *p into TCell at column C.
 * For multi-byte sequences we collect all bytes and pack into one TCell. */
#define ColorChar() \
    do { \
        BPos = C - Pos; \
        if (B && BPos >= 0 && BPos < Width) { \
            unsigned char _c0 = (unsigned char)*p; \
            TCell _cell; \
            if (_c0 < 0x80 || utf8_is_cont(_c0)) { \
                /* ASCII or lone continuation (shouldn't happen) */ \
                _cell = TCELL_MAKE1(_c0, HILIT_CLRD()); \
            } else { \
                int _bc; \
                utf8_decode(p, &_bc); \
                unsigned char _b1 = (_bc >= 2) ? (unsigned char)p[1] : 0; \
                unsigned char _b2 = (_bc >= 3) ? (unsigned char)p[2] : 0; \
                _cell = TCELL_MAKE(_c0, _b1, _b2, HILIT_CLRD()); \
            } \
            B[BPos] = _cell; \
        } \
        if (StateMap) StateMap[i] = (hsState)(State & X_MASK); \
    } while (0)

/* Advance past the current character.
 * For multi-byte sequences: advance i/p/len by the full byte count,
 * advance C by wcwidth. */
#define NextChar() \
    do { \
        unsigned char _nc = (unsigned char)*p; \
        if (_nc < 0x80) { \
            /* ASCII */ \
            i++; p++; len--; C++; \
        } else if (utf8_is_cont(_nc)) { \
            /* lone continuation byte — skip without advancing C */ \
            i++; p++; len--; \
        } else { \
            /* lead byte of multi-byte sequence */ \
            int _bc; \
            unsigned long _cp = utf8_decode(p, &_bc); \
            int _w = utf8_codepoint_width(_cp); \
            if (_bc > len) _bc = len; \
            i += _bc; p += _bc; len -= _bc; \
            C += _w; \
            /* double-width: write placeholder TCell */ \
            if (_w == 2 && B) { \
                int _ph = (C - 1) - Pos; \
                if (_ph >= 0 && _ph < Width) \
                    B[_ph] = TCELL_MAKE1(0, HILIT_CLRD()); \
            } \
        } \
    } while (0)

#define ColorNext() do { ColorChar(); NextChar(); } while (0)

#define ColorNext() do { ColorChar(); NextChar(); } while (0)

#define UntilMatchBrace(first, cmd) \
    do { \
        int Count[] = { 0, 0, 0, }; \
        switch (first) \
        { \
            case '{': ++Count[0]; break; \
            case '[': ++Count[1]; break; \
            case '(': ++Count[2]; break; \
        } \
\
        while (len > 0)        \
        {                      \
            switch (*p) {      \
            case '{':          \
                ++Count[0];    \
                break;         \
            case '}':          \
                --Count[0];    \
                break;         \
            case '[':          \
                ++Count[1];    \
                break;         \
            case ']':          \
                --Count[1];    \
                break;         \
            case '(':          \
                ++Count[2];    \
                break;         \
            case ')':          \
                --Count[2];    \
                break;         \
            }                  \
            cmd;               \
            if (TEST_ZERO)     \
                break;         \
        } \
    } while (0)

#define HILIT_VARS(ColorTable, Line) \
    int BPos; \
    ChColor *Colors = ColorTable; \
    ChColor Color = CLR_Normal; \
    int i; \
    int len = Line->Count; \
    char *p = Line->Chars; \
    int NC = 0, C = 0; \
    int TabSize = BFI(BF, BFI_TabSize); \
    int ExpandTabs = BFI(BF, BFI_ExpandTabs);

//#define HILIT_VARS2()
//    int len1 = len;
//    char *last = p + len1 - 1;

#define IF_TAB() \
    if (*p == '\t' && ExpandTabs) { \
    NC = NextTab(C, TabSize); \
    if (StateMap) StateMap[i] = hsState(State);\
    if (B) MoveChar(B, C - Pos, Width, ' ', HILIT_CLRD(), NC - C);\
    if (BFI(BF, BFI_ShowTabs)) ColorChar();\
    i++,len--,p++;\
    C = NC;\
    continue;\
    }

#define CK_MAXLEN 64

inline bool isZeroArray(int* Count, size_t len) {
    for (size_t i = 0; i < len; ++i)
        if (Count[i] != 0)
            return 0;
    return 1;
}

#define TEST_ZERO isZeroArray(Count, sizeof(Count)/sizeof(Count[0]))

typedef struct {
    int TotalCount;
    int count[CK_MAXLEN];
    char *key[CK_MAXLEN];
} ColorKeywords;

struct HTrans {
    char *match;
    int matchLen;
    long matchFlags;
    int nextState;
    int color;
    RxNode *regexp;

    void InitTrans();
};

struct HState {
    int transCount;
    int firstTrans;
    int color;

    ColorKeywords keywords;
    char *wordChars;
    long options;
    int nextKwdMatchedState;
    int nextKwdNotMatchedState;
    int nextKwdNoCharState;

    void InitState();
    int GetHilitWord(int len, char *str, ChColor &clr);
};

class HMachine {
public:
    int stateCount;
    int transCount;
    HState *state;
    HTrans *trans;

    HMachine();
    ~HMachine();
    void AddState(HState &aState);
    void AddTrans(HTrans &aTrans);

    HState *LastState() {
        return state + stateCount - 1;
    }
};

class EColorize {
public:
    char *Name;
    EColorize *Next;
    EColorize *Parent;
    int SyntaxParser;
    ColorKeywords Keywords; // keywords to highlight
    HMachine *hm;
    ChColor Colors[COUNT_CLR];

    EColorize(const char *AName, const char *AParent);
    ~EColorize();

    int SetColor(int clr, const char *value);
};

extern EColorize *Colorizers;
EColorize *FindColorizer(const char *AName);

SyntaxProc GetHilitProc(int id);

int IsState(hsState *Buf, hsState State, int Len);
int LookAt(EBuffer *B, int Row, unsigned int Pos, const char *What, hsState State, int NoWord = 1, int CaseInsensitive = 0);
int LookAtNoCase(EBuffer *B, int Row, unsigned int Pos, const char *What, hsState State, int NoWord = 1);

#endif /* __HILIT_H_ */
