/*
 * o_p4.cpp
 *
 * Copyright (c) 2008, eFTE SF Group (see AUTHORS file)
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 * P4 (Perforce) support, modelled after SVN/CVS implementation.
 * Class providing access to most P4 commands.
 */

#include "fte.h"

static int SameDir(const char *D1, const char *D2) {
    if (!D1 || !D2) return 0;
    int l1 = strlen(D1);
    int l2 = strlen(D2);
    if (l1 < l2) return strncmp(D1, D2, l1) == 0 && strcmp(D2 + l1, SSLASH) == 0;
    else if (l1 == l2) return !strcmp(D1, D2);
    else return strncmp(D1, D2, l2) == 0 && strcmp(D1 + l1, SSLASH) == 0;
}

EP4 *P4View = 0;

EP4::EP4(int createFlags, EModel **ARoot, const char *ADir, const char *ACommand, const char *AOnFiles): EP4Base(createFlags, ARoot, "P4") {
    P4View = this;
    LogFile = 0;
    Commiting = 0;
    RunPipe(ADir, ACommand, AOnFiles);
}

EP4::EP4(int createFlags, EModel **ARoot): EP4Base(createFlags, ARoot, "P4") {
    P4View = this;
    LogFile = 0;
}

EP4::~EP4() {
    P4View = 0;
    RemoveLogFile();
}

void EP4::RemoveLogFile() {
    if (LogFile) {
        unlink(LogFile);
        free(LogFile);
        LogFile = 0;
    }
}

char *EP4::MarkedAsList() {
    int i;
    int len = 0;
    for (i = 0; i < LineCount; i++)
        if (Lines[i]->Status & 2) len += strlen(Lines[i]->File) + 1;
    if (len == 0) {
        if (Lines[Row]->Status & 4) return strdup(Lines[Row]->File);
        else return NULL;
    }
    char *s = (char *)malloc(len + 1);
    s[0] = 0;
    for (i = 0; i < LineCount; i++)
        if (Lines[i]->Status & 2) strcat(strcat(s, Lines[i]->File), " ");
    s[strlen(s) - 1] = 0;
    return s;
}

char EP4::GetFileStatus(const char *file) {
    for (int i = LineCount - 1; i >= 0; i--)
        if (Lines[i]->File && filecmp(Lines[i]->File, file) == 0) return Lines[i]->Msg[0];
    return 0;
}

/*
 * Parse output of P4 commands.
 *
 * 'p4 opened' format:
 *   //depot/path/file.cpp#3 - edit default change (text)
 *   //depot/path/file.h#1 - add default change (text)
 *
 * 'p4 diff' header lines:
 *   ==== //depot/path/file.cpp#3 - /home/user/ws/file.cpp ====
 *
 * 'p4 edit/add/delete' confirmation:
 *   //depot/path/file.cpp#3 - opened for edit
 *   //depot/path/file.cpp#1 - opened for add
 *
 * Lines starting with '//' with ' - ' in them are actionable.
 */
void EP4::ParseLine(const char *line, int len) {
    if (len > 4 && line[0] == '/' && line[1] == '/') {
        /* Find ' - ' separator */
        const char *sep = strstr(line, " - ");
        if (sep) {
            /* Extract depot path (up to #rev or the separator) */
            const char *hash = (const char *)memchr(line, '#', sep - line);
            int pathlen = (int)((hash ? hash : sep) - line);
            char *path = (char *)malloc(pathlen + 1);
            memcpy(path, line, pathlen);
            path[pathlen] = 0;

            AddLine(path, -1, line, 5); /* hilit=1 + markable=4 */
            free(path);
            return;
        }
    }
    AddLine(0, -1, line);
}

int EP4::RunPipe(const char *ADir, const char *ACommand, const char *AOnFiles) {
    Commiting = 0;
    if (!SameDir(Directory, ADir)) FreeLines();
    return EP4Base::RunPipe(ADir, ACommand, AOnFiles);
}

void EP4::ClosePipe() {
    EP4Base::ClosePipe();
    Commiting = 0;
}

int EP4::RunCommit(const char *ADir, const char *ACommand, const char *AOnFiles) {
    if (!SameDir(Directory, ADir)) FreeLines();

    free(Command);
    free(Directory);
    free(OnFiles);

    Command = strdup(ACommand);
    Directory = strdup(ADir);
    OnFiles = strdup(AOnFiles);

    RemoveLogFile();
    Running = 1;

    EP4Log *p4log = new EP4Log(0, &ActiveModel, Directory, OnFiles);
    LogFile = strdup(p4log->FileName);
    View->SwitchToModel(p4log);

    AddLine(LogFile, -1, "P4 submit start - enter description", 1);

    return 0;
}

extern BufferView *BufferList;

int EP4::DoneCommit(int commit) {
    Running = 0;
    free(Lines[LineCount - 1]->File);
    free(Lines[LineCount - 1]->Msg);
    LineCount--;
    UpdateList();

    if (commit) {
        /*
         * Read the description from the log file and build:
         *   p4 submit -d "description" [files]
         *
         * For simplicity we read the first non-empty line as description.
         * A more sophisticated approach would read the whole log file.
         */
        FILE *f = fopen(LogFile, "r");
        char desc[1024] = "no description";
        if (f) {
            char buf[1024];
            while (fgets(buf, sizeof(buf), f)) {
                int blen = strlen(buf);
                while (blen > 0 && (buf[blen-1] == '\n' || buf[blen-1] == '\r')) blen--;
                buf[blen] = 0;
                if (blen > 0) { strncpy(desc, buf, sizeof(desc) - 1); break; }
            }
            fclose(f);
        }

        /* Escape double quotes in description */
        char edesc[2048];
        int di = 0;
        for (int si = 0; desc[si] && di < (int)sizeof(edesc) - 2; si++) {
            if (desc[si] == '"') edesc[di++] = '\\';
            edesc[di++] = desc[si];
        }
        edesc[di] = 0;

        char *ACommand = (char *)malloc(strlen(Command) + sizeof(edesc) + 32);
        char *ADirectory = strdup(Directory);
        char *AOnFiles = strdup(OnFiles);
        sprintf(ACommand, "%s -d \"%s\"", Command, edesc);
        int ret = RunPipe(ADirectory, ACommand, AOnFiles);
        free(ACommand);
        free(ADirectory);
        free(AOnFiles);
        Commiting = 1;

        if (ActiveView->Model == P4LogView) {
            if (P4LogView->Next != P4View) {
                P4View->Prev->Next = P4View->Next;
                P4View->Next->Prev = P4View->Prev;
                P4View->Next = P4LogView->Next;
                P4LogView->Next->Prev = P4View;
                P4LogView->Next = P4View;
                P4View->Prev = P4LogView;
            }
        }

        return ret;
    } else {
        RemoveLogFile();
        UpdateList();
        return 0;
    }
}

int EP4::CanQuit() const {
    if (Running) return 0;
    else return 1;
}

int EP4::ConfQuit(GxView *V, int multiFile) {
    if (P4LogView) {
        if (P4LogView->ConfQuit(V, multiFile)) {
            ActiveView->DeleteModel(P4LogView);
        } else return 0;
    }
    if (Running) {
        switch (V->Choice(GPC_ERROR, "P4 command is running", 2, "&Kill", "&Cancel", "")) {
        case 0: return 1;
        case 1:
        default: return 0;
        }
    } else return 1;
}

int EP4::GetContext() const {
    return CONTEXT_P4;
}

EEventMap *EP4::GetEventMap() {
    return FindEventMap("P4");
}
