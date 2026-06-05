/*
 * e_p4log.cpp
 *
 * Copyright (c) 2008, eFTE SF Group (see AUTHORS file)
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 * S.Pinigin copy o_cvslog.cpp and replace cvs/Cvs/CVS to p4/P4/P4.
 *
 * Subclass of EBuffer for writing log for P4 commit. Creates temporary file
 * used for commit which is deleted when view is closed. Asks for commit or
 * discard on view close.
 */

#include "fte.h"
//#include "features.h"

EP4Log *P4LogView;

EP4Log::EP4Log(int createFlags, EModel **ARoot, const char *Directory, const char *AOnFiles): EBuffer(createFlags, ARoot, NULL) {
    int i, j, p;
    char msgFile[MAXPATH];
    char *OnFiles = strdup(AOnFiles);

    P4LogView = this;
    // Create filename for message
#ifdef UNIX
    // Use this in Unix - it says more to user
    sprintf(msgFile, "/tmp/efte%d-p4-msg", getpid());
#else
    tmpnam(msgFile);
#endif
    SetFileName(msgFile, P4LogMode);

    // Preload buffer with info
    InsertLine(0, 0, "");
    InsertLine(1, 60, "P4: -------------------------------------------------------");
    InsertLine(2, 59, "P4: Enter log. Lines beginning with 'P4:' will be removed");
    InsertLine(3, 4, "P4:");
    InsertLine(4, 18, "P4: Commiting in ");
    InsText(4, 18, strlen(Directory), Directory);
    if (OnFiles[0]) {
        p = 5;
        // Go through files - use GetFileStatus to show what to do with files
        // First count files
        int cnt = 0;
        i = 0;
        while (1) {
            if (OnFiles[i] == 0 || OnFiles[i] == ' ') {
                while (OnFiles[i] == ' ') i++;
                cnt++;
                if (!OnFiles[i]) break;
            } else i++;
        }
        int *position = new int[cnt];
        int *len = new int[cnt];
        char *status = new char[cnt];
        // Find out position and status for each file
        i = j = 0;
        position[0] = 0;
        while (1) {
            if (OnFiles[i] == 0 || OnFiles[i] == ' ') {
                // This is not thread-safe!
                len[j] = i - position[j];
                char c = OnFiles[i];
                OnFiles[i] = 0;
                status[j] = P4View->GetFileStatus(OnFiles + position[j]);
                if (status[j] == 0) status[j] = 'x';
                OnFiles[i] = c;
                while (OnFiles[i] == ' ') i++;
                if (!OnFiles[i]) break;
                position[++j] = i;
            } else i++;
        }
        // Go through status
        int fAdded = 0, fRemoved = 0, fModified = 0, fOther = 0;
        for (i = 0;i < cnt;i++) switch (status[i]) {
            case 'A':
            case 'a':
                fAdded++;
                break;
            case 'R':
            case 'r':
                fRemoved++;
                break;
            case 'M':
            case 'm':
                fModified++;
                break;
            default:
                fOther++;
            }
        // Now list files with given status
        ListFiles(p, fAdded, "Added", cnt, position, len, status, OnFiles, "Aa");
        ListFiles(p, fRemoved, "Removed", cnt, position, len, status, OnFiles, "Rr");
        ListFiles(p, fModified, "Modified", cnt, position, len, status, OnFiles, "Mm");
        ListFiles(p, fOther, "Other", cnt, position, len, status, OnFiles, "AaRrMm", 1);
        delete [] position;
        delete [] len;
        delete [] status;
    } else {
        InsertLine(5, 4, "P4:");
        InsertLine(6, 30, "P4: Commiting whole directory");
        p = 7;
    }
    InsertLine(p, 4, "P4:");
    InsertLine(p + 1, 60, "P4: -------------------------------------------------------");
    SetPos(0, 0);
    FreeUndo();
    free(OnFiles);
    Modified = 0;
}

EP4Log::~EP4Log() {
    P4LogView = 0;
}

void EP4Log::ListFiles(int &p, const int fCount, const char *title, const int cnt, const int *position,
                        const int *len, const char *status, const char *list, const char *excinc, const int exc) {
    if (fCount) {
        InsertLine(p++, 4, "P4:");
        int i = strlen(title);
        InsertLine(p, 5, "P4: ");
        InsText(p, 5, i, title);
        InsText(p, i += 5, 5, " file");
        i += 5;
        if (fCount != 1) InsText(p, i++, 1, "s");
        InsText(p++, i, 1, ":");
        for (i = 0;i < cnt;i++)
            if (!!strchr(excinc, status[i]) ^ !!exc) {
                // Should be displayed
                InsertLine(p, 9, "P4:     ");
                InsText(p, 9, 1, status + i);
                InsText(p, 10, 1, " ");
                InsText(p++, 11, len[i], list + position[i]);
            }
    }
}

// Overridden because we don't want to load file
EViewPort *EP4Log::CreateViewPort(EView *V) {
    V->Port = new EEditPort(this, V);
    AddView(V);
    return V->Port;
}

int EP4Log::CanQuit() const {
    return 0;
}

int EP4Log::ConfQuit(GxView *V, int /*multiFile*/) {
    int i;

    switch (V->Choice(GPC_ERROR, "P4 commit pending", 3, "C&ommit", "&Discard", "&Cancel", "")) {
    case 0: // Commit
        // First save - this is just try
        if (Save() == 0) return 0;
        // Now remove P4: lines and really save
        for (i = 0;i < RCount;) {
            PELine l = RLine(i);
            if (l->Count >= 4 && strncmp(l->Chars, "P4:", 4) == 0) DelLine(i);
            else i++;
        }
        Save();
        // DoneCommit returns 0 if OK
        return !P4View->DoneCommit(1);
    case 1: // Discard
        P4View->DoneCommit(0);
        return 1;
    case 2: // Cancel
    default:
        return 0;
    }
}

// Shown in "Closing xxx..." message when closing model
void EP4Log::GetName(char *AName, int MaxLen) const {
    strncpy(AName, "P4 log", MaxLen);
}

// Shown in buffer list
void EP4Log::GetInfo(char *AInfo, int /*MaxLen*/) const {
    sprintf(AInfo, "%2d %04d:%03d%cP4 log: %-140s", ModelNo, 1 + CP.Row, 1 + CP.Col, Modified ? '*' : ' ', FileName);
}

// Normal and short title (normal for window, short for icon in X)
void EP4Log::GetTitle(char *ATitle, int MaxLen, char *ASTitle, int SMaxLen) const {
    strncpy(ATitle, "P4 log", MaxLen);
    strncpy(ASTitle, "P4 log", SMaxLen);
}
