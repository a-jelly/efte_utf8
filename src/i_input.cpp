/*    i_input.cpp
 *
 *    Copyright (c) 2008, eFTE SF Group (see AUTHORS file)
 *    Copyright (c) 1994-1996, Marko Macek
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

#include "fte.h"

ExInput::ExInput(const char *APrompt, char *ALine, unsigned int ABufLen, Completer AComp, int Select, int AHistId): ExView() {
    assert(ABufLen > 0);
    MaxLen = ABufLen - 1;
    Comp = AComp;
    SelStart = SelEnd = 0;
    Prompt = strdup(APrompt);
    Line = (char *) malloc(MaxLen + 1);
    MatchStr = (char *) malloc(MaxLen + 1);
    CurStr = (char *) malloc(MaxLen + 1);
    if (Line) {
        Line[MaxLen] = 0;
        strncpy(Line, ALine, MaxLen);
        Pos = strlen(Line);
        LPos = 0;
    }
    if (Select)
        SelEnd = Pos;
    TabCount = 0;
    HistId = AHistId;
    CurItem = 0;
}

ExInput::~ExInput() {
    if (Prompt)
        free(Prompt);
    if (Line)
        free(Line);
    if (MatchStr)
        free(MatchStr);
    if (CurStr)
        free(CurStr);
    Prompt = 0;
    Line = 0;
}

void ExInput::Activate(int gotfocus) {
    ExView::Activate(gotfocus);
}

ExView * ExInput::GetViewContext() {
    return Next;
}

int ExInput::BeginMacro() {
    return 1;
}

void ExInput::HandleEvent(TEvent &Event) {
    switch (Event.What) {
    case evKeyDown:
        switch (kbCode(Event.Key.Code)) {
        case kbLeft:
            if (Pos > 0) {
                Pos--;
                // UTF-8 фикс: Прыгаем назад через байты продолжения (0x80 - 0xBF)
                while (Pos > 0 && (Line[Pos] & 0xC0) == 0x80) Pos--;
            }
            SelStart = SelEnd = 0;
            TabCount = 0;
            Event.What = evNone;
            break;
        case kbRight:
            if (Pos < strlen(Line)) {
                Pos++;
                // UTF-8 фикс: Прыгаем вперед через байты продолжения
                while (Pos < strlen(Line) && (Line[Pos] & 0xC0) == 0x80) Pos++;
            }
            SelStart = SelEnd = 0;
            TabCount = 0;
            Event.What = evNone;
            break;
        case kbLeft | kfCtrl:
            if (Pos > 0) {
                Pos--;
                while (Pos > 0) {
                    if (isalnum(Line[Pos]) && !isalnum(Line[Pos - 1]))
                        break;
                    Pos--;
                }
            }
            SelStart = SelEnd = 0;
            TabCount = 0;
            Event.What = evNone;
            break;
        case kbRight | kfCtrl: {
            unsigned int len = strlen(Line);
            if (Pos < len) {
                Pos++;
                while (Pos < len) {
                    if (isalnum(Line[Pos]) && !isalnum(Line[Pos - 1]))
                        break;
                    Pos++;
                }
            }
        }
        SelStart = SelEnd = 0;
        TabCount = 0;
        Event.What = evNone;
        break;
        case kbHome:
            Pos = 0;
            SelStart = SelEnd = 0;
            TabCount = 0;
            Event.What = evNone;
            break;
        case kbEnd:
            Pos = strlen(Line);
            SelStart = SelEnd = 0;
            TabCount = 0;
            Event.What = evNone;
            break;
        case kbEsc:
            EndExec(0);
            Event.What = evNone;
            break;
        case kbEnter:
            AddInputHistory(HistId, Line);
            EndExec(1);
            Event.What = evNone;
            break;
        case kbBackSp | kfCtrl | kfShift:
            SelStart = SelEnd = 0;
            Pos = 0;
            Line[0] = 0;
            TabCount = 0;
            break;
        case kbBackSp | kfCtrl:
            if (Pos > 0) {
                if (Pos > strlen(Line)) {
                    Pos = strlen(Line);
                } else {
                    char Ch;

                    if (Pos > 0) do {
                            Pos--;
                            memmove(Line + Pos, Line + Pos + 1, strlen(Line + Pos + 1) + 1);
                            if (Pos == 0) break;
                            Ch = Line[Pos - 1];
                        } while (Pos > 0 && Ch != '\\' && Ch != '/' && Ch != '.' && isalnum(Ch));
                }
            }
            SelStart = SelEnd = 0;
            TabCount = 0;
            Event.What = evNone;
            break;
        case kbBackSp:
        case kbBackSp | kfShift:
            if (SelStart < SelEnd) {
                memmove(Line + SelStart, Line + SelEnd, strlen(Line + SelEnd) + 1);
                Pos = SelStart;
                SelStart = SelEnd = 0;
                break;
            }
            if (Pos <= 0) break;
            {
                // UTF-8 фикс: Вычисляем, сколько байт занимает удаляемый символ
                int dec = 1;
                while (Pos - dec > 0 && (Line[Pos - dec] & 0xC0) == 0x80) dec++;
                Pos -= dec;
                memmove(Line + Pos, Line + Pos + dec, strlen(Line + Pos + dec) + 1);
            }
            TabCount = 0;
            Event.What = evNone;
            break;
        case kbDel:
            if (SelStart < SelEnd) {
                memmove(Line + SelStart, Line + SelEnd, strlen(Line + SelEnd) + 1);
                Pos = SelStart;
                SelStart = SelEnd = 0;
                break;
            }
            if (Pos < strlen(Line)) {
                // UTF-8 фикс: Удаляем все байты текущего символа
                int inc = 1;
                while (Pos + inc < strlen(Line) && (Line[Pos + inc] & 0xC0) == 0x80) inc++;
                memmove(Line + Pos, Line + Pos + inc, strlen(Line + Pos + inc) + 1);
            }
            TabCount = 0;
            Event.What = evNone;
            break;
        case kbDel | kfCtrl:
            if (SelStart < SelEnd) {
                memmove(Line + SelStart, Line + SelEnd, strlen(Line + SelEnd) + 1);
                Pos = SelStart;
                SelStart = SelEnd = 0;
                break;
            }
            SelStart = SelEnd = 0;
            Line[Pos] = 0;
            TabCount = 0;
            Event.What = evNone;
            break;
        case kbIns | kfShift:
        case 'V'   | kfCtrl: {
            int len;

            if (SystemClipboard)
                GetPMClip(0);

            if (SSBuffer == 0) break;
            if (SSBuffer->RCount == 0) break;

            if (SelStart < SelEnd) {
                memmove(Line + SelStart, Line + SelEnd, strlen(Line + SelEnd) + 1);
                Pos = SelStart;
                SelStart = SelEnd = 0;
            }

            len = SSBuffer->LineChars(0);
            if (strlen(Line) + len < MaxLen) {
                memmove(Line + Pos + len, Line + Pos, strlen(Line + Pos) + 1);
                memcpy(Line + Pos, SSBuffer->RLine(0)->Chars, len);
                TabCount = 0;
                Event.What = evNone;
                Pos += len;
            }
        }
        break;
        case kbUp:
            SelStart = SelEnd = 0;
            CurItem++;
            {
                int cnt = CountInputHistory(HistId);
                if (CurItem > cnt) {
                    CurItem = cnt;
                }

                GetInputHistory(HistId, Line, MaxLen, CurItem);
                Pos = strlen(Line);
                SelStart = SelEnd = 0;
            }
            Event.What = evNone;
            break;
        case kbDown:
            SelStart = SelEnd = 0;
            if (CurItem > 0) {
                CurItem--;
            }
            {
                int cnt = CountInputHistory(HistId);
                if (CurItem > cnt) {
                    CurItem = cnt;
                }

                GetInputHistory(HistId, Line, MaxLen, CurItem);
                Pos = strlen(Line);
                SelStart = SelEnd = 0;
            }
            Event.What = evNone;
            break;
        case kbTab | kfShift:
            TabCount -= 2;
        case kbTab:
            if (Comp) {
                char *Str2 = (char *) malloc(MaxLen + 1);
                int n;

                assert(Str2);
                TabCount++;
                if (TabCount < 1) TabCount = 1;
                if ((TabCount == 1) && (kbCode(Event.Key.Code) == kbTab)) {
                    strcpy(MatchStr, Line);
                }
                n = Comp(MatchStr, Str2, TabCount);
                if ((n > 0) && (TabCount <= n)) {
                    strcpy(Line, Str2);
                    Pos = strlen(Line);
                } else if (TabCount > n) TabCount = n;
                free(Str2);
            }
            SelStart = SelEnd = 0;
            Event.What = evNone;
            break;
        case 'Q' | kfCtrl:
            Event.What = evKeyDown;
            Event.Key.Code = Win->GetChar(0);
        default: {
            // ==============================================================
            // UTF-8 ФИКС: РУЧНОЙ ВВОД МНОГОБАЙТОВЫХ СИМВОЛОВ (КИРИЛЛИЦА)
            // ==============================================================
            unsigned long code = Event.Key.Code;
            
            // Если нажат системный хоткей (Alt/Ctrl) - игнорируем ввод текста
            if (code & (kfAlt | kfCtrl)) break;

            // Извлекаем чистый символ (UCS-4)
            uint32_t cp = code & 0x00FFFFFF;

            // Игнорируем непечатные управляющие символы (кроме табуляции)
            if (cp < 32 && cp != '\t') break;

            char seq[5];
            int slen = 0;

            // На лету кодируем Unicode в правильный UTF-8 массив
            if (cp <= 0x7F) {
                seq[0] = (char)cp;
                slen = 1;
            } else if (cp <= 0x7FF) {
                seq[0] = (char)(0xC0 | (cp >> 6));
                seq[1] = (char)(0x80 | (cp & 0x3F));
                slen = 2;
            } else if (cp <= 0xFFFF) {
                seq[0] = (char)(0xE0 | (cp >> 12));
                seq[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
                seq[2] = (char)(0x80 | (cp & 0x3F));
                slen = 3;
            } else if (cp <= 0x10FFFF) {
                seq[0] = (char)(0xF0 | (cp >> 18));
                seq[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
                seq[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
                seq[3] = (char)(0x80 | (cp & 0x3F));
                slen = 4;
            } else {
                break; // Битый символ
            }

            // Вставляем все получившиеся байты в буфер
            if (strlen(Line) + slen <= MaxLen) {
                if (SelStart < SelEnd) {
                    memmove(Line + SelStart, Line + SelEnd, strlen(Line + SelEnd) + 1);
                    Pos = SelStart;
                    SelStart = SelEnd = 0;
                }
                // Раздвигаем буфер под длину символа
                memmove(Line + Pos + slen, Line + Pos, strlen(Line + Pos) + 1);
                
                // Записываем байты UTF-8
                for (int i = 0; i < slen; i++) {
                    Line[Pos++] = seq[i];
                }
                TabCount = 0;
                Event.What = evNone;
            }
        }
        break;
        }
        Event.What = evNone;
        break;
    }
}

/*
void ExInput::HandleEvent(TEvent &Event) {
    switch (Event.What) {
    case evKeyDown:
        switch (kbCode(Event.Key.Code)) {
        case kbLeft:
            if (Pos > 0) Pos--;
            SelStart = SelEnd = 0;
            TabCount = 0;
            Event.What = evNone;
            break;
        case kbRight:
            Pos++;
            SelStart = SelEnd = 0;
            TabCount = 0;
            Event.What = evNone;
            break;
        case kbLeft | kfCtrl:
            if (Pos > 0) {
                Pos--;
                while (Pos > 0) {
                    if (isalnum(Line[Pos]) && !isalnum(Line[Pos - 1]))
                        break;
                    Pos--;
                }
            }
            SelStart = SelEnd = 0;
            TabCount = 0;
            Event.What = evNone;
            break;
        case kbRight | kfCtrl: {
            unsigned int len = strlen(Line);
            if (Pos < len) {
                Pos++;
                while (Pos < len) {
                    if (isalnum(Line[Pos]) && !isalnum(Line[Pos - 1]))
                        break;
                    Pos++;
                }
            }
        }
        SelStart = SelEnd = 0;
        TabCount = 0;
        Event.What = evNone;
        break;
        case kbHome:
            Pos = 0;
            SelStart = SelEnd = 0;
            TabCount = 0;
            Event.What = evNone;
            break;
        case kbEnd:
            Pos = strlen(Line);
            SelStart = SelEnd = 0;
            TabCount = 0;
            Event.What = evNone;
            break;
        case kbEsc:
            EndExec(0);
            Event.What = evNone;
            break;
        case kbEnter:
            AddInputHistory(HistId, Line);
            EndExec(1);
            Event.What = evNone;
            break;
        case kbBackSp | kfCtrl | kfShift:
            SelStart = SelEnd = 0;
            Pos = 0;
            Line[0] = 0;
            TabCount = 0;
            break;
        case kbBackSp | kfCtrl:
            if (Pos > 0) {
                if (Pos > strlen(Line)) {
                    Pos = strlen(Line);
                } else {
                    char Ch;

                    if (Pos > 0) do {
                            Pos--;
                            memmove(Line + Pos, Line + Pos + 1, strlen(Line + Pos + 1) + 1);
                            if (Pos == 0) break;
                            Ch = Line[Pos - 1];
                        } while (Pos > 0 && Ch != '\\' && Ch != '/' && Ch != '.' && isalnum(Ch));
                }
            }
            SelStart = SelEnd = 0;
            TabCount = 0;
            Event.What = evNone;
            break;
        case kbBackSp:
        case kbBackSp | kfShift:
            if (SelStart < SelEnd) {
                memmove(Line + SelStart, Line + SelEnd, strlen(Line + SelEnd) + 1);
                Pos = SelStart;
                SelStart = SelEnd = 0;
                break;
            }
            if (Pos <= 0) break;
            Pos--;
            if (Pos < strlen(Line))
                memmove(Line + Pos, Line + Pos + 1, strlen(Line + Pos + 1) + 1);
            TabCount = 0;
            Event.What = evNone;
            break;
        case kbDel:
            if (SelStart < SelEnd) {
                memmove(Line + SelStart, Line + SelEnd, strlen(Line + SelEnd) + 1);
                Pos = SelStart;
                SelStart = SelEnd = 0;
                break;
            }
            if (Pos < strlen(Line))
                memmove(Line + Pos, Line + Pos + 1, strlen(Line + Pos + 1) + 1);
            TabCount = 0;
            Event.What = evNone;
            break;
        case kbDel | kfCtrl:
            if (SelStart < SelEnd) {
                memmove(Line + SelStart, Line + SelEnd, strlen(Line + SelEnd) + 1);
                Pos = SelStart;
                SelStart = SelEnd = 0;
                break;
            }
            SelStart = SelEnd = 0;
            Line[Pos] = 0;
            TabCount = 0;
            Event.What = evNone;
            break;
        case kbIns | kfShift:
        case 'V'   | kfCtrl: {
            int len;

            if (SystemClipboard)
                GetPMClip(0);

            if (SSBuffer == 0) break;
            if (SSBuffer->RCount == 0) break;

            if (SelStart < SelEnd) {
                memmove(Line + SelStart, Line + SelEnd, strlen(Line + SelEnd) + 1);
                Pos = SelStart;
                SelStart = SelEnd = 0;
            }

            len = SSBuffer->LineChars(0);
            if (strlen(Line) + len < MaxLen) {
                memmove(Line + Pos + len, Line + Pos, strlen(Line + Pos) + 1);
                memcpy(Line + Pos, SSBuffer->RLine(0)->Chars, len);
                TabCount = 0;
                Event.What = evNone;
                Pos += len;
            }
        }
        break;
        case kbUp:
            SelStart = SelEnd = 0;
            CurItem++;
            // printf("Up: Cnt: %d, CurItem: %d, CurStr: %s, Line: %s\n", CountInputHistory(HistId), CurItem, CurStr, Line);
            {
                int cnt = CountInputHistory(HistId);
                if (CurItem > cnt) {
                    CurItem = cnt;
                }

                GetInputHistory(HistId, Line, MaxLen, CurItem);
                Pos = strlen(Line);
                SelStart = SelEnd = 0;
            }
            Event.What = evNone;
            break;
        case kbDown:
            SelStart = SelEnd = 0;
            if (CurItem > 0) {
                CurItem--;
            }
            // printf("Dn: Cnt: %d, CurItem: %d, CurStr: %s, Line: %s\n", CountInputHistory(HistId), CurItem, CurStr, Line);

            {
                int cnt = CountInputHistory(HistId);
                if (CurItem > cnt) {
                    CurItem = cnt;
                }

                GetInputHistory(HistId, Line, MaxLen, CurItem);
                Pos = strlen(Line);
                SelStart = SelEnd = 0;
            }
            Event.What = evNone;
            break;
        case kbTab | kfShift:
            TabCount -= 2;
        case kbTab:
            if (Comp) {
                char *Str2 = (char *) malloc(MaxLen + 1);
                int n;

                assert(Str2);
                TabCount++;
                if (TabCount < 1) TabCount = 1;
                if ((TabCount == 1) && (kbCode(Event.Key.Code) == kbTab)) {
                    strcpy(MatchStr, Line);
                }
                n = Comp(MatchStr, Str2, TabCount);
                if ((n > 0) && (TabCount <= n)) {
                    strcpy(Line, Str2);
                    Pos = strlen(Line);
                } else if (TabCount > n) TabCount = n;
                free(Str2);
            }
            SelStart = SelEnd = 0;
            Event.What = evNone;
            break;
        case 'Q' | kfCtrl:
            Event.What = evKeyDown;
            Event.Key.Code = Win->GetChar(0);
        default: {
            char Ch;

            if (GetCharFromEvent(Event, &Ch) && (strlen(Line) < MaxLen)) {
                if (SelStart < SelEnd) {
                    memmove(Line + SelStart, Line + SelEnd, strlen(Line + SelEnd) + 1);
                    Pos = SelStart;
                    SelStart = SelEnd = 0;
                }
                memmove(Line + Pos + 1, Line + Pos, strlen(Line + Pos) + 1);
                Line[Pos++] = Ch;
                TabCount = 0;
                Event.What = evNone;
            }
        }
        break;
        }
        Event.What = evNone;
        break;
    }
}
*/
void ExInput::UpdateView() {
    if (Next) {
        Next->UpdateView();
    }
}

void ExInput::RepaintView() {
    if (Next) {
        Next->RepaintView();
    }
}

void ExInput::UpdateStatus() {
    RepaintStatus();
}


void ExInput::RepaintStatus() {
    TDrawBuffer B;
    int W, H, FLen, FPos;

    ConQuerySize(&W, &H);

    FPos = strlen(Prompt) + 2;
    FLen = W - FPos;

    if (Pos > strlen(Line))
        Pos = strlen(Line);

    // ====================================================================
    // UTF-8 ФИКС: ВЫЧИСЛЕНИЕ ВИЗУАЛЬНОЙ ШИРИНЫ (СИМВОЛОВ, А НЕ БАЙТ)
    // ====================================================================
    int vPos = 0, vLPos = 0, vSelStart = 0, vSelEnd = 0;
    
    // Считаем реальные символы (игнорируя байты продолжения UTF-8)
    for (unsigned int i = 0; i < Pos; i++)      
        if (((unsigned char)Line[i] & 0xC0) != 0x80) vPos++;
        
    for (unsigned int i = 0; i < LPos; i++)     
        if (((unsigned char)Line[i] & 0xC0) != 0x80) vLPos++;
        
    for (unsigned int i = 0; i < SelStart; i++) 
        if (((unsigned char)Line[i] & 0xC0) != 0x80) vSelStart++;
        
    for (unsigned int i = 0; i < SelEnd; i++)   
        if (((unsigned char)Line[i] & 0xC0) != 0x80) vSelEnd++;

    // Скроллинг строки ввода теперь работает по визуальным колонкам
    if (vLPos + FLen <= vPos) vLPos = vPos - FLen + 1;
    if (vPos < vLPos) vLPos = vPos;

    // Конвертируем новую визуальную позицию (vLPos) обратно в байтовую (LPos) 
    // для корректной передачи в функцию отрисовки текста.
    LPos = 0;
    int chars = 0;
    while (Line[LPos]) {
        if (((unsigned char)Line[LPos] & 0xC0) != 0x80) {
            if (chars == vLPos) break;
            chars++;
        }
        LPos++;
    }
    // ====================================================================

    MoveChar(B, 0, W, ' ', hcEntry_Field, W);
    MoveStr(B, 0, W, Prompt, hcEntry_Prompt, FPos);
    MoveChar(B, FPos - 2, W, ':', hcEntry_Prompt, 1);
    
    // Отрисовываем сам текст (MoveStr уже умеет декодировать UTF-8)
    MoveStr(B, FPos, W, Line + LPos, hcEntry_Field, FLen);
    
    // Рисуем выделение (Selection) по визуальным колонкам
    if (vSelEnd > vSelStart) {
        MoveAttr(B, FPos + vSelStart - vLPos, W, hcEntry_Selection, vSelEnd - vSelStart);
    }
    
    // Ставим курсор на правильную визуальную позицию (vPos)
    ConSetCursorPos(FPos + vPos - vLPos, H - 1);
    ConPutBox(0, H - 1, W, 1, B);
    ConSetInsertState(true);
    ConShowCursor();
}
/*
void ExInput::RepaintStatus() {
    TDrawBuffer B;
    int W, H, FLen, FPos;

    ConQuerySize(&W, &H);

    FPos = strlen(Prompt) + 2;
    FLen = W - FPos;

    if (Pos > strlen(Line))
        Pos = strlen(Line);
    //if (Pos < 0) Pos = 0;
    if (LPos + FLen <= Pos) LPos = Pos - FLen + 1;
    if (Pos < LPos) LPos = Pos;

    MoveChar(B, 0, W, ' ', hcEntry_Field, W);
    MoveStr(B, 0, W, Prompt, hcEntry_Prompt, FPos);
    MoveChar(B, FPos - 2, W, ':', hcEntry_Prompt, 1);
    MoveStr(B, FPos, W, Line + LPos, hcEntry_Field, FLen);
    MoveAttr(B, FPos + SelStart - LPos, W, hcEntry_Selection, SelEnd - SelStart);
    ConSetCursorPos(FPos + Pos - LPos, H - 1);
    ConPutBox(0, H - 1, W, 1, B);
    ConSetInsertState(true);
    ConShowCursor();
}
*/