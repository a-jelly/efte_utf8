/*
 * o_git.cpp
 *
 * Git support for eFTE, modelled after SVN/P4 implementation.
 * Parses output of git status --porcelain, git diff, git log.
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

EGit *GitView = 0;

EGit::EGit(int createFlags, EModel **ARoot, const char *ADir, const char *ACommand, const char *AOnFiles): EGitBase(createFlags, ARoot, "Git") {
    GitView = this;
    LogFile = 0;
    Commiting = 0;
    RunPipe(ADir, ACommand, AOnFiles);
}

EGit::EGit(int createFlags, EModel **ARoot): EGitBase(createFlags, ARoot, "Git") {
    GitView = this;
    LogFile = 0;
}

EGit::~EGit() {
    GitView = 0;
    RemoveLogFile();
}

void EGit::RemoveLogFile() {
    if (LogFile) { unlink(LogFile); free(LogFile); LogFile = 0; }
}

char *EGit::MarkedAsList() {
    int len = 0;
    for (int i = 0; i < LineCount; i++)
        if (Lines[i]->Status & 2) len += strlen(Lines[i]->File) + 1;
    if (len == 0) {
        if (Lines[Row]->Status & 4) return strdup(Lines[Row]->File);
        else return NULL;
    }
    char *s = (char *)malloc(len + 1);
    s[0] = 0;
    for (int i = 0; i < LineCount; i++)
        if (Lines[i]->Status & 2) strcat(strcat(s, Lines[i]->File), " ");
    s[strlen(s) - 1] = 0;
    return s;
}

char EGit::GetFileStatus(const char *file) {
    for (int i = LineCount - 1; i >= 0; i--)
        if (Lines[i]->File && filecmp(Lines[i]->File, file) == 0) return Lines[i]->Msg[0];
    return 0;
}

/* git status --porcelain: XY filename
 * X=index Y=worktree: M=modified A=added D=deleted R=renamed ??=untracked UU=conflict */
void EGit::ParseLine(const char *line, int len) {
    if (len >= 4 && line[2] == ' ') {
        char x = line[0], y = line[1];
        if ((x == '?' && y == '?') || x == 'M' || y == 'M' || x == 'A' || y == 'A' ||
            x == 'D' || y == 'D' || x == 'R' || y == 'R' || x == 'C' || y == 'C' ||
            x == 'U' || y == 'U') {
            const char *file = line + 3;
            const char *arrow = strstr(file, " -> ");
            int flen = (arrow && (x == 'R' || y == 'R')) ? (int)(arrow - file) : (int)strlen(file);
            char *fname = (char *)malloc(flen + 1);
            memcpy(fname, file, flen);
            fname[flen] = 0;
            AddLine(fname, -1, line, 5);
            free(fname);
            return;
        }
    }
    if (len > 6 && strncmp(line, "+++ b/", 6) == 0) { AddLine(line + 6, -1, line, 1); return; }
    if (len > 6 && strncmp(line, "--- a/", 6) == 0) { AddLine(line + 6, -1, line, 1); return; }
    AddLine(0, -1, line);
}

int EGit::RunPipe(const char *ADir, const char *ACommand, const char *AOnFiles) {
    Commiting = 0;
    if (!SameDir(Directory, ADir)) FreeLines();
    return EGitBase::RunPipe(ADir, ACommand, AOnFiles);
}

void EGit::ClosePipe() { EGitBase::ClosePipe(); Commiting = 0; }

int EGit::RunCommit(const char *ADir, const char *ACommand, const char *AOnFiles) {
    if (!SameDir(Directory, ADir)) FreeLines();
    free(Command); free(Directory); free(OnFiles);
    Command = strdup(ACommand); Directory = strdup(ADir); OnFiles = strdup(AOnFiles);
    RemoveLogFile(); Running = 1;
    EGitLog *gitlog = new EGitLog(0, &ActiveModel, Directory, OnFiles);
    LogFile = strdup(gitlog->FileName);
    View->SwitchToModel(gitlog);
    AddLine(LogFile, -1, "Git commit - enter message, then close buffer", 1);
    return 0;
}

extern BufferView *BufferList;

int EGit::DoneCommit(int commit) {
    Running = 0;
    free(Lines[LineCount - 1]->File); free(Lines[LineCount - 1]->Msg);
    LineCount--; UpdateList();
    if (commit) {
        char *ACommand = (char *)malloc(strlen(Command) + strlen(LogFile) + 32);
        char *ADirectory = strdup(Directory);
        char *AOnFiles = strdup(OnFiles);
        sprintf(ACommand, "%s -F \"%s\"", Command, LogFile);
        int ret = RunPipe(ADirectory, ACommand, AOnFiles);
        free(ACommand); free(ADirectory); free(AOnFiles);
        Commiting = 1;
        if (ActiveView->Model == GitLogView) {
            if (GitLogView->Next != GitView) {
                GitView->Prev->Next = GitView->Next; GitView->Next->Prev = GitView->Prev;
                GitView->Next = GitLogView->Next; GitLogView->Next->Prev = GitView;
                GitLogView->Next = GitView; GitView->Prev = GitLogView;
            }
        }
        return ret;
    } else { RemoveLogFile(); UpdateList(); return 0; }
}

int EGit::CanQuit() const { return Running ? 0 : 1; }

int EGit::ConfQuit(GxView *V, int multiFile) {
    if (GitLogView) {
        if (GitLogView->ConfQuit(V, multiFile)) ActiveView->DeleteModel(GitLogView);
        else return 0;
    }
    if (Running) {
        switch (V->Choice(GPC_ERROR, "Git command is running", 2, "&Kill", "&Cancel", "")) {
        case 0: return 1; default: return 0;
        }
    } else return 1;
}

int EGit::GetContext() const { return CONTEXT_GIT; }
EEventMap *EGit::GetEventMap() { return FindEventMap("GIT"); }
