/*    view.cpp
 *
 *    Copyright (c) 2008, eFTE SF Group (see AUTHORS file)
 *    Copyright (c) 1994-1996, Marko Macek
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

#include "fte.h"

EView *ActiveView = 0;

extern BufferView *BufferList;

EView::EView(EModel *AModel) {
    if (ActiveView) {
        Prev = ActiveView;
        Next = ActiveView->Next;
        Prev->Next = this;
        Next->Prev = this;
    } else
        Prev = Next = this;
    ActiveView = this;
    Model = AModel;
    NextView = 0;
    Port = 0;
    MView = 0;
    CurMsg = 0;
    if (Model)
        Model->CreateViewPort(this);
}

EView::~EView() {
    if (Next != this) {
        Prev->Next = Next;
        Next->Prev = Prev;
        if (ActiveView == this)
            ActiveView = Next;
    } else
        ActiveView = 0;
    if (MView)
        MView->View = 0;
    if (Model)
        Model->RemoveView(this);
    if (Port)
        delete Port;
}

int EView::CanQuit() const {
    if (Model)
        return Model->CanQuit();
    else
        return 1;
}

void EView::FocusChange(int GetFocus) {
    if (GetFocus) {
        if (Model->View && Model->View->Port)
            Model->View->Port->GetPos();
        Model->CreateViewPort(this);
    } else {
        if (Model) {
            Model->RemoveView(this);
            delete Port;
            Port = 0;
            if (Model->View && Model->View->Port)
                Model->View->Port->StorePos();
        }
    }
}

void EView::Resize(int Width, int Height) {
    if (Port)
        Port->Resize(Width, Height);
}

void EView::SetModel(EModel *AModel) {
    Model = AModel;
    ActiveModel = Model;
}

void EView::SelectModel(EModel *AModel) {
    if (Model != AModel) {
        if (Model)
            FocusChange(0);
        SetModel(AModel);
        if (Model)
            FocusChange(1);
    }
}

void EView::SwitchToModel(EModel *AModel) {
    if (Model != AModel) {
        if (Model)
            FocusChange(0);

        AModel->Prev->Next = AModel->Next;
        AModel->Next->Prev = AModel->Prev;

        if (Model) {
            AModel->Next = Model;
            AModel->Prev = Model->Prev;
            AModel->Prev->Next = AModel;
            Model->Prev = AModel;
        } else {
            AModel->Next = AModel->Prev = AModel;
        }

        SetModel(AModel);

        if (Model)
            FocusChange(1);
    }
}

void EView::Activate(int GotFocus) {
    if (Model && Model->View != this && Port) {
        Model->SelectView(this);
        if (GotFocus) {
            Port->StorePos();
        } else {
            Port->GetPos();
        }
        Port->RepaintView();
        if (GotFocus)
            ActiveView = this;
    }
}

int EView::GetContext() const {
    return Model ? Model->GetContext() : 0;
}

EEventMap *EView::GetEventMap() {
    return Model ? Model->GetEventMap() : 0;
}

int EView::BeginMacro() {
    return Model ? Model->BeginMacro() : 0;
}

int EView::ExecCommand(int Command, ExState &State) {
    switch (Command) {
    case ExSwitchTo:
        return SwitchTo(State);
    case ExFilePrev:
        return FilePrev();
    case ExFileNext:
        return FileNext();
    case ExFileLast:
        return FileLast();
    case ExFileOpen:
        return FileOpen(State);
    case ExFileOpenInMode:
        return FileOpenInMode(State);
    case ExFileSaveAll:
        return FileSaveAll();
    case ExListRoutines:
        return ViewRoutines(State);
    case ExDirOpen:
        return DirOpen(State);
    case ExViewMessages:
        return ViewMessages(State);
    case ExCompile:
        return Compile(State);
    case ExRunCompiler:
        return RunCompiler(State);
    case ExCompilePrevError:
        return CompilePrevError(State);
    case ExCompileNextError:
        return CompileNextError(State);
    case ExCvs:
        return Cvs(State);
    case ExRunCvs:
        return RunCvs(State);
    case ExViewCvs:
        return ViewCvs(State);
    case ExClearCvsMessages:
        return ClearCvsMessages(State);
    case ExCvsDiff:
        return CvsDiff(State);
    case ExRunCvsDiff:
        return RunCvsDiff(State);
    case ExViewCvsDiff:
        return ViewCvsDiff(State);
    case ExCvsCommit:
        return CvsCommit(State);
    case ExRunCvsCommit:
        return RunCvsCommit(State);
    case ExViewCvsLog:
        return ViewCvsLog(State);
    case ExSvn:
        return Svn(State);
    case ExRunSvn:
        return RunSvn(State);
    case ExViewSvn:
        return ViewSvn(State);
    case ExClearSvnMessages:
        return ClearSvnMessages(State);
    case ExSvnDiff:
        return SvnDiff(State);
    case ExRunSvnDiff:
        return RunSvnDiff(State);
    case ExViewSvnDiff:
        return ViewSvnDiff(State);
    case ExSvnCommit:
        return SvnCommit(State);
    case ExRunSvnCommit:
        return RunSvnCommit(State);
    case ExViewSvnLog:
        return ViewSvnLog(State);
    case ExP4:
        return P4(State);
    case ExRunP4:
        return RunP4(State);
    case ExViewP4:
        return ViewP4(State);
    case ExClearP4Messages:
        return ClearP4Messages(State);
    case ExP4Diff:
        return P4Diff(State);
    case ExRunP4Diff:
        return RunP4Diff(State);
    case ExViewP4Diff:
        return ViewP4Diff(State);
    case ExP4Submit:
        return P4Submit(State);
    case ExRunP4Submit:
        return RunP4Submit(State);
    case ExViewP4Log:
        return ViewP4Log(State);
    case ExP4Edit:
        return P4Edit(State);
    case ExGit:
        return Git(State);
    case ExRunGit:
        return RunGit(State);
    case ExViewGit:
        return ViewGit(State);
    case ExClearGitMessages:
        return ClearGitMessages(State);
    case ExGitDiff:
        return GitDiff(State);
    case ExRunGitDiff:
        return RunGitDiff(State);
    case ExViewGitDiff:
        return ViewGitDiff(State);
    case ExGitCommit:
        return GitCommit(State);
    case ExRunGitCommit:
        return RunGitCommit(State);
    case ExViewGitLog:
        return ViewGitLog(State);
    case ExViewBuffers:
        return ViewBuffers(State);
    case ExShowKey:
        return ShowKey(State);
    case ExToggleSysClipboard:
        return ToggleSysClipboard(State);
    case ExSetPrintDevice:
        return SetPrintDevice(State);
    case ExShowVersion:
        return ShowVersion();
    case ExViewModeMap:
        return ViewModeMap(State);
    case ExClearMessages:
        return ClearMessages();
    case ExTagNext:
        return TagNext(this);
    case ExTagPrev:
        return TagPrev(this);
    case ExTagPop:
        return TagPop(this);
    case ExTagClear:
        TagClear();
        return 1;
    case ExTagLoad:
        return TagLoad(State);
    case ExShowHelp:
        return SysShowHelp(State, 0);
    case ExConfigRecompile:
        return ConfigRecompile(State);
    case ExRemoveGlobalBookmark:
        return RemoveGlobalBookmark(State);
    case ExGotoGlobalBookmark:
        return GotoGlobalBookmark(State);
    case ExPopGlobalBookmark:
        return PopGlobalBookmark();
    }
    return Model ? Model->ExecCommand(Command, State) : 0;
}

void EView::HandleEvent(TEvent &Event) {
    if (Model)
        Model->HandleEvent(Event);
    if (Port)
        Port->HandleEvent(Event);
    if (Event.What == evCommand) {
        switch (Event.Msg.Command) {
        case cmDroppedFile: {
            char *file = (char *)Event.Msg.Param2;

            if (IsDirectory(file))
                OpenDir(file);
            MultiFileLoad(0, file, NULL, this);
        }
        break;
        }
    }
}

void EView::UpdateView() {
    if (Port)
        Port->UpdateView();
}

void EView::RepaintView() {
    if (Port)
        Port->RepaintView();
}

void EView::UpdateStatus() {
    if (Port)
        Port->UpdateStatus();
}

void EView::RepaintStatus() {
    if (Port)
        Port->RepaintStatus();
}

void EView::DeleteModel(EModel *M) {
    EView *V;
    EModel *M1;
    char s[256];

    if (M == 0)
        return;

    M->GetName(s, sizeof(s));
    Msg(S_INFO, "Closing %s.", s);

    V = ActiveView = this;
    while (V) {
        M1 = V->Model;
        if (M1 == M) {
            if (M->Next != M)
                V->SelectModel(M->Next);
            else
                V->SelectModel(0);
        }
        V = V->Next;
        if (V == ActiveView)
            break;
    }
    delete M;
    SetMsg(0);
    return;
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

int EView::FilePrev() {
    if (Model) {
        EModel *n = Model->Prev;
        if (IgnoreBufferList && n && n->GetContext() == CONTEXT_BUFFERS) n = n->Prev;
        SelectModel(n);
        return 1;
    }
    return 0;
}

int EView::FileNext() {
    if (Model) {
        EModel *n = Model->Next;
        if (IgnoreBufferList && n && n->GetContext() == CONTEXT_BUFFERS) n = n->Next;
        SelectModel(n);
        return 1;
    }
    return 0;
}

int EView::FileLast() {
    if (Model) {
        EModel *n = Model->Next;
        if (IgnoreBufferList && n && n->GetContext() == CONTEXT_BUFFERS) n = n->Next;
        SwitchToModel(n);
        return 1;
    }
    return 0;
}

int EView::SwitchTo(ExState &State) {
    EModel *M;
    int No;

    if (State.GetIntParam(this, &No) == 0) {
        char str[10] = "";

        if (MView->Win->GetStr("Obj.Number", sizeof(str), (char *)str, 0) == 0) return 0;
        No = atoi(str);
    }
    M = Model;
    while (M) {
        if (M->ModelNo == No) {
            SwitchToModel(M);
            return 1;
        }
        M = M->Next;
        if (M == Model)
            return 0;
    }
    return 0;
}


int EView::FileSaveAll() {
    EModel *M = Model;
    while (M) {
        if (M->GetContext() == CONTEXT_FILE) {
            EBuffer *B = (EBuffer *)M;
            if (B->Modified) {
                SwitchToModel(B);
                if (B->Save() == 0) return 0;
            }
        }
        M = M->Next;
        if (M == Model) break;
    }
    return 1;
}

int EView::FileOpen(ExState &State) {
    char FName[MAXPATH];

    if (State.GetStrParam(this, FName, sizeof(FName)) == 0) {
        if (GetDefaultDirectory(Model, FName, sizeof(FName)) == 0)
            return 0;
        if (MView->Win->GetFile("Open file", sizeof(FName), FName, HIST_PATH, GF_OPEN) == 0) return 0;
    }

    if (strlen(FName) == 0) return 0;

    if (IsDirectory(FName))
        return OpenDir(FName);
    return MultiFileLoad(0, FName, NULL, this);
}

int EView::FileOpenInMode(ExState &State) {
    char Mode[32] = "";
    char FName[MAXPATH];

    if (State.GetStrParam(this, Mode, sizeof(Mode)) == 0)
        if (MView->Win->GetStr("Mode", sizeof(Mode), Mode, HIST_SETUP) != 1) return 0;

    if (FindMode(Mode) == 0) {
        MView->Win->Choice(GPC_ERROR, "Error", 1, "O&K", "Invalid mode '%s'", Mode);
        return 0;
    }

    if (GetDefaultDirectory(Model, FName, sizeof(FName)) == 0)
        return 0;
    if (State.GetStrParam(this, FName, sizeof(FName)) == 0)
        if (MView->Win->GetFile("Open file", sizeof(FName), FName, HIST_PATH, GF_OPEN) == 0) return 0;
    if (IsDirectory(FName))
        return OpenDir(FName);

    if (strlen(FName) == 0) return 0;

    return MultiFileLoad(0, FName, Mode, this);
}

int EView::SetPrintDevice(ExState &State) {
    char Dev[MAXPATH];

    strcpy(Dev, PrintDevice);
    if (State.GetStrParam(this, Dev, sizeof(Dev)) == 0)
        if (MView->Win->GetStr("Print to", sizeof(Dev), Dev, HIST_SETUP) == 0) return 0;

    strcpy(PrintDevice, Dev);
    return 1;
}

int EView::ToggleSysClipboard(ExState &/*State*/) {
    SystemClipboard = SystemClipboard ? 0 : 1;
    Msg(S_INFO, "SysClipboard is now %s.", SystemClipboard ? "ON" : "OFF");
    return 1;
}

int EView::ShowKey(ExState &/*State*/) {
    char buf[100];
    KeySel ks;

    ks.Mask = 0;
    ks.Key = MView->Win->GetChar(0);

    GetKeyName(buf, sizeof(buf), ks);
    Msg(S_INFO, "Key: '%s' - '%8X'", buf, ks.Key);
    return 1;
}

void EView::Msg(int level, const char *s, ...) {
    char msgbuftmp[MSGBUFTMP_SIZE];
    va_list ap;

    va_start(ap, s);
    vsprintf(msgbuftmp, s, ap);
    va_end(ap);

    if (level != S_BUSY)
        SetMsg(msgbuftmp);
}

void EView::SetMsg(const char *msg) {
    if (CurMsg)
        free(CurMsg);
    CurMsg = 0;
    if (msg && strlen(msg))
        CurMsg = strdup(msg);
    if (CurMsg && msg && MView) {
        TDrawBuffer B;
        char SColor;
        int Cols, Rows;

        MView->ConQuerySize(&Cols, &Rows);

        if (MView->IsActive())
            SColor = hcStatus_Active;
        else
            SColor = hcStatus_Normal;

        MoveChar(B, 0, Cols, ' ', SColor, Cols);
        MoveStr(B, 0, Cols, CurMsg, SColor, Cols);
        if (MView->Win->GetStatusContext() == MView)
            MView->ConPutBox(0, Rows - 1, Cols, 1, B);
        //printf("%s\n", Msg);
    }
}

int EView::ViewBuffers(ExState &/*State*/) {
    if (BufferList == 0) {
        BufferList = new BufferView(0, &ActiveModel);
        SwitchToModel(BufferList);
    } else {
        BufferList->UpdateList();
        BufferList->Row = 1;
        SwitchToModel(BufferList);
        return 1;
    }
    return 0;
}

int EView::ViewRoutines(ExState &/*State*/) {
    //int rc = 1;
    //RoutineView *routines;
    EModel *M;
    EBuffer *Buffer;

    M = Model;
    if (M->GetContext() != CONTEXT_FILE)
        return 0;
    Buffer = (EBuffer *)M;

    if (Buffer->Routines == 0) {
        if (BFS(Buffer, BFS_RoutineRegexp) == 0) {
            MView->Win->Choice(GPC_ERROR, "Error", 1, "O&K", "No routine regexp.");
            return 0;
        }
        Buffer->Routines = new RoutineView(0, &ActiveModel, Buffer);
        if (Buffer->Routines == 0)
            return 0;
    } else {
        Buffer->Routines->UpdateList();
    }
    SwitchToModel(Buffer->Routines);
    return 1;
}

int EView::DirOpen(ExState &State) {
    char Path[MAXPATH];

    if (State.GetStrParam(this, Path, sizeof(Path)) == 0)
        if (GetDefaultDirectory(Model, Path, sizeof(Path)) == 0)
            return 0;
    return OpenDir(Path);
}

int EView::OpenDir(const char *Path) {
    char XPath[MAXPATH];
    EDirectory *dir = 0;

    if (ExpandPath(Path, XPath, sizeof(XPath)) == -1)
        return 0;
    {
        EModel *x = Model;
        while (x) {
            if (x->GetContext() == CONTEXT_DIRECTORY) {
                if (filecmp(((EDirectory *)x)->Path, XPath) == 0) {
                    dir = (EDirectory *)x;
                    break;
                }
            }
            x = x->Next;
            if (x == Model)
                break;
        }
    }
    if (dir == 0)
        dir = new EDirectory(0, &ActiveModel, XPath);
    SelectModel(dir);
    return 1;
}

int EView::Compile(ExState &State) {
    static char Cmd[256] = "";
    char Command[256] = "";

    if (CompilerMsgs != 0 && CompilerMsgs->Running) {
        Msg(S_INFO, "Already running...");
        return 0;
    }

    if (State.GetStrParam(this, Command, sizeof(Command)) == 0) {
        if (Model->GetContext() == CONTEXT_FILE) {
            EBuffer *B = (EBuffer *)Model;
            if (BFS(B, BFS_CompileCommand) != 0)
                strcpy(Cmd, BFS(B, BFS_CompileCommand));
        }
        if (Cmd[0] == 0)
            strcpy(Cmd, CompileCommand);

        if (MView->Win->GetStr("Compile", sizeof(Cmd), Cmd, HIST_COMPILE) == 0) return 0;

        strcpy(Command, Cmd);
    } else {
        if (MView->Win->GetStr("Compile", sizeof(Command), Command, HIST_COMPILE) == 0) return 0;
    }
    return Compile(Command);
}

int EView::RunCompiler(ExState &State) {
    char Command[256] = "";

    if (CompilerMsgs != 0 && CompilerMsgs->Running) {
        Msg(S_INFO, "Already running...");
        return 0;
    }

    if (State.GetStrParam(this, Command, sizeof(Command)) == 0) {
        if (Model->GetContext() == CONTEXT_FILE) {
            EBuffer *B = (EBuffer *)Model;
            if (BFS(B, BFS_CompileCommand) != 0)
                strcpy(Command, BFS(B, BFS_CompileCommand));
        }
        if (Command[0] == 0)
            strcpy(Command, CompileCommand);
    }
    return Compile(Command);
}

int EView::Compile(const char *Command) {
    char Dir[MAXPATH] = "";
    EMessages *msgs;

    if (CompilerMsgs != 0) {
        strcpy(Dir, CompilerMsgs->Directory);
        CompilerMsgs->RunPipe(Dir, Command);
        msgs = CompilerMsgs;
    } else {
        if (GetDefaultDirectory(Model, Dir, sizeof(Dir)) == 0)
            return 0;

        msgs = new EMessages(0, &ActiveModel, Dir, Command);
    }
    SwitchToModel(msgs);
    return 1;
}

int EView::ViewMessages(ExState &/*State*/) {
    if (CompilerMsgs != 0) {
        SwitchToModel(CompilerMsgs);
        return 1;
    }
    return 0;
}

int EView::CompilePrevError(ExState &/*State*/) {
    if (CompilerMsgs != 0)
        return CompilerMsgs->CompilePrevError(this);
    return 0;
}

int EView::CompileNextError(ExState &/*State*/) {
    if (CompilerMsgs != 0)
        return CompilerMsgs->CompileNextError(this);
    return 0;
}

int EView::ShowVersion() {
    if (access("/usr/local/share/doc/efte/README", 0) == 0)
        FileLoad(0, "/usr/local/share/doc/efte/README", 0, this);
    else if (access("/usr/share/doc/efte/README", 0) == 0)
        FileLoad(0, "/usr/share/doc/efte/README", 0, this);
    else if (access("/efte/doc/README", 0) == 0)
        FileLoad(0, "/efte/doc/README", 0, this);
    else if (access("/efte/README", 0) == 0)
        FileLoad(0, "/efte/README", 0, this);
    else if (access("/Program Files/efte/doc/README", 0) == 0)
        FileLoad(0, "/Program Files/efte/doc/README", 0, this);
    else if (access("/Program Files (x86)/doc/README", 0) == 0)
        FileLoad(0, "/Program Files (x86)/doc/README", 0, this);
    else
        MView->Win->Choice(0, "About", 1, "O&K", PROGRAM " " VERSION " " COPYRIGHT);
    return 1;
}

int EView::ViewModeMap(ExState &/*State*/) {
    if (TheEventMapView != 0)
        TheEventMapView->ViewMap(GetEventMap());
    else
        (void)new EventMapView(0, &ActiveModel, GetEventMap());
    if (TheEventMapView != 0)
        SwitchToModel(TheEventMapView);
    else
        return 0;
    return 1;
}

int EView::ClearMessages() {
    if (CompilerMsgs != 0 && CompilerMsgs->Running) {
        Msg(S_INFO, "Running...");
        return 0;
    }
    if (CompilerMsgs != 0) {
        CompilerMsgs->FreeErrors();
        CompilerMsgs->UpdateList();
    }
    return 1;
}

int EView::TagLoad(ExState &State) {
    char Tag[MAXPATH];
    char FullTag[MAXPATH];

    char const* pTagFile = getenv("TAGFILE");
    if (pTagFile == NULL) {
        pTagFile = "tags";
    }
    if (ExpandPath(pTagFile, Tag, sizeof(Tag)) == -1)
        return 0;
    if (State.GetStrParam(this, Tag, sizeof(Tag)) == 0)
        if (MView->Win->GetFile("Load tags", sizeof(Tag), Tag, HIST_TAGFILES, GF_OPEN) == 0) return 0;

    if (ExpandPath(Tag, FullTag, sizeof(FullTag)) == -1)
        return 0;

    if (!FileExists(FullTag)) {
        Msg(S_INFO, "Tag file '%s' not found.", FullTag);
        return 0;
    }

    return ::TagLoad(FullTag);
}

int EView::ConfigRecompile(ExState &/*State*/) {
    if (ConfigSourcePath == 0 || ConfigFileName[0] == 0) {
        Msg(S_ERROR, "Cannot recompile (must use external configuration).");
        return 0;
    }

    char command[1024];

    strlcpy(command, "cefte \"", sizeof(command));
    strlcat(command, ConfigSourcePath, sizeof(command));
    strlcat(command, "\" ", sizeof(command));
#ifdef UNIX
    if (ExpandPath("~/.efterc", command + strlen(command), sizeof(command) - strlen(command)) != 0)
        return 0;
#else
    strlcat(command, "\"", sizeof(command));
    strlcat(command, ConfigFileName, sizeof(command));
    strlcat(command, "\"", sizeof(command));
#endif
    return Compile(command);
}

int EView::RemoveGlobalBookmark(ExState &State) {
    char name[256] = "";

    if (State.GetStrParam(this, name, sizeof(name)) == 0)
        if (MView->Win->GetStr("Remove Global Bookmark", sizeof(name), name, HIST_BOOKMARK) == 0) return 0;
    if (markIndex.Remove(name) == 0) {
        Msg(S_ERROR, "Error removing global bookmark %s.", name);
        return 0;
    }
    return 1;
}

int EView::GotoGlobalBookmark(ExState &State) {
    char name[256] = "";

    if (State.GetStrParam(this, name, sizeof(name)) == 0)
        if (MView->Win->GetStr("Goto Global Bookmark", sizeof(name), name, HIST_BOOKMARK) == 0) return 0;
    if (markIndex.View(this, name) == 0) {
        Msg(S_ERROR, "Error locating global bookmark %s.", name);
        return 0;
    }
    return 1;
}
int EView::PopGlobalBookmark() {
    if (markIndex.PopMark(this) == 0) {
        Msg(S_INFO, "Bookmark stack empty.");
        return 0;
    }
    return 1;
}

int EView::GetStrVar(int var, char *str, int buflen) {
    //switch (var) {
    //}
    return Model->GetStrVar(var, str, buflen);
}

int EView::GetIntVar(int var, int *value) {
    //switch (var) {
    //}
    return Model->GetIntVar(var, value);
}

int EView::Cvs(ExState &State) {
    static char Opts[128] = "";
    char Options[128] = "";

    if (CvsView != 0 && CvsView->Running) {
        Msg(S_INFO, "Already running...");
        return 0;
    }

    if (State.GetStrParam(this, Options, sizeof(Options)) == 0) {
        if (MView->Win->GetStr("CVS options", sizeof(Opts), Opts, HIST_CVS) == 0) return 0;
        strcpy(Options, Opts);
    } else {
        if (MView->Win->GetStr("CVS options", sizeof(Options), Options, HIST_CVS) == 0) return 0;
    }
    return Cvs(Options);
}

int EView::RunCvs(ExState &State) {
    char Options[128] = "";

    if (CvsView != 0 && CvsView->Running) {
        Msg(S_INFO, "Already running...");
        return 0;
    }

    State.GetStrParam(this, Options, sizeof(Options));
    return Cvs(Options);
}

int EView::Cvs(const char *Options) {
    char Dir[MAXPATH] = "";
    char Command[256] = "";
    char buf[1024] = "";
    char *OnFiles = buf;
    ECvs *cvs;

    if (GetDefaultDirectory(Model, Dir, sizeof(Dir)) == 0) return 0;

    strcpy(Command, CvsCommand);
    strcat(Command, " ");
    if (Options[0] != 0) {
        strcat(Command, Options);
        strcat(Command, " ");
    }

    switch (Model->GetContext()) {
    case CONTEXT_FILE:
        if (JustFileName(((EBuffer *)Model)->FileName, OnFiles, sizeof(buf)) != 0) return 0; // OnFiles points to buf
        break;
    case CONTEXT_CVSDIFF:
        OnFiles = strdup(CvsDiffView->OnFiles);
        break;
    case CONTEXT_CVS:
        OnFiles = ((ECvs *)Model)->MarkedAsList();
        if (!OnFiles) OnFiles = strdup(((ECvs *)Model)->OnFiles);
        break;
    }

    if (CvsView != 0) {
        CvsView->RunPipe(Dir, Command, OnFiles);
        cvs = CvsView;
    } else {
        cvs = new ECvs(0, &ActiveModel, Dir, Command, OnFiles);
    }
    if (OnFiles != buf) free(OnFiles);
    SwitchToModel(cvs);
    return 1;
}

int EView::ClearCvsMessages(ExState &/*State*/) {
    if (CvsView != 0) {
        if (CvsView->Running) {
            Msg(S_INFO, "Running...");
            return 0;
        } else {
            CvsView->FreeLines();
            CvsView->UpdateList();
            return 1;
        }
    }
    return 0;
}

int EView::ViewCvs(ExState &/*State*/) {
    if (CvsView != 0) {
        SwitchToModel(CvsView);
        return 1;
    }
    return 0;
}

int EView::CvsDiff(ExState &State) {
    static char Opts[128] = "";
    char Options[128] = "";

    if (CvsDiffView != 0 && CvsDiffView->Running) {
        Msg(S_INFO, "Already running...");
        return 0;
    }

    if (State.GetStrParam(this, Options, sizeof(Options)) == 0) {
        if (MView->Win->GetStr("CVS diff options", sizeof(Opts), Opts, HIST_CVSDIFF) == 0) return 0;
        strcpy(Options, Opts);
    } else {
        if (MView->Win->GetStr("CVS diff options", sizeof(Options), Options, HIST_CVSDIFF) == 0) return 0;
    }
    return CvsDiff(Options);
}

int EView::RunCvsDiff(ExState &State) {
    char Options[128] = "";

    if (CvsDiffView != 0 && CvsDiffView->Running) {
        Msg(S_INFO, "Already running...");
        return 0;
    }

    State.GetStrParam(this, Options, sizeof(Options));
    return CvsDiff(Options);
}

int EView::CvsDiff(const char *Options) {
    char Dir[MAXPATH] = "";
    char Command[256] = "";
    char buf[1024] = "";
    char *OnFiles = buf;
    ECvsDiff *diffs;

    if (GetDefaultDirectory(Model, Dir, sizeof(Dir)) == 0) return 0;

    strcpy(Command, CvsCommand);
    strcat(Command, " diff -c ");
    if (Options[0] != 0) {
        strcat(Command, Options);
        strcat(Command, " ");
    }

    switch (Model->GetContext()) {
    case CONTEXT_FILE:
        if (JustFileName(((EBuffer *)Model)->FileName, OnFiles, sizeof(buf)) != 0) return 0; // OnFiles points to buf
        break;
    case CONTEXT_CVSDIFF:
        OnFiles = strdup(CvsDiffView->OnFiles);
        break;
    case CONTEXT_CVS:
        OnFiles = ((ECvs *)Model)->MarkedAsList();
        if (!OnFiles) OnFiles = strdup(((ECvs *)Model)->OnFiles);
        break;
    }

    if (CvsDiffView != 0) {
        CvsDiffView->RunPipe(Dir, Command, OnFiles);
        diffs = CvsDiffView;
    } else {
        diffs = new ECvsDiff(0, &ActiveModel, Dir, Command, OnFiles);
    }
    if (OnFiles != buf) free(OnFiles);
    SwitchToModel(diffs);
    return 1;
}

int EView::ViewCvsDiff(ExState &/*State*/) {
    if (CvsDiffView != 0) {
        SwitchToModel(CvsDiffView);
        return 1;
    }
    return 0;
}

int EView::CvsCommit(ExState &State) {
    static char Opts[128] = "";
    char Options[128] = "";

    if (CvsView != 0 && CvsView->Running) {
        Msg(S_INFO, "Already running...");
        return 0;
    }

    if (State.GetStrParam(this, Options, sizeof(Options)) == 0) {
        if (MView->Win->GetStr("CVS commit options", sizeof(Opts), Opts, HIST_CVSCOMMIT) == 0) return 0;
        strcpy(Options, Opts);
    } else {
        if (MView->Win->GetStr("CVS commit options", sizeof(Options), Options, HIST_CVSCOMMIT) == 0) return 0;
    }
    return CvsCommit(Options);
}

int EView::RunCvsCommit(ExState &State) {
    char Options[128] = "";

    if (CvsView != 0 && CvsView->Running) {
        Msg(S_INFO, "Already running...");
        return 0;
    }

    State.GetStrParam(this, Options, sizeof(Options));
    return CvsCommit(Options);
}

int EView::CvsCommit(const char *Options) {
    char Dir[MAXPATH] = "";
    char Command[256] = "";
    char buf[1024] = "";
    char *OnFiles = buf;
    ECvs *cvs;

    if (GetDefaultDirectory(Model, Dir, sizeof(Dir)) == 0) return 0;

    strcpy(Command, CvsCommand);
    strcat(Command, " commit ");
    if (Options[0] != 0) {
        strcat(Command, Options);
        strcat(Command, " ");
    }

    switch (Model->GetContext()) {
    case CONTEXT_FILE:
        if (JustFileName(((EBuffer *)Model)->FileName, OnFiles, sizeof(buf)) != 0) return 0; // OnFiles points to buf
        break;
    case CONTEXT_CVSDIFF:
        OnFiles = strdup(CvsDiffView->OnFiles);
        break;
    case CONTEXT_CVS:
        OnFiles = ((ECvs *)Model)->MarkedAsList();
        if (!OnFiles) OnFiles = strdup(((ECvs *)Model)->OnFiles);
        break;
    }

    if (CvsView == 0) cvs = new ECvs(0, &ActiveModel);
    else cvs = CvsView;
    SwitchToModel(cvs);
    cvs->RunCommit(Dir, Command, OnFiles);
    if (OnFiles != buf) free(OnFiles);
    return 1;
}

int EView::ViewCvsLog(ExState &/*State*/) {
    if (CvsLogView != 0) {
        SwitchToModel(CvsLogView);
        return 1;
    }
    return 0;
}

int EView::Svn(ExState &State) {
    static char Opts[128] = "";
    char Options[128] = "";

    if (SvnView != 0 && SvnView->Running) {
        Msg(S_INFO, "Already running...");
        return 0;
    }

    if (State.GetStrParam(this, Options, sizeof(Options)) == 0) {
        if (MView->Win->GetStr("SVN options", sizeof(Opts), Opts, HIST_SVN) == 0) return 0;
        strcpy(Options, Opts);
    } else {
        if (MView->Win->GetStr("SVN options", sizeof(Options), Options, HIST_SVN) == 0) return 0;
    }
    return Svn(Options);
}

int EView::RunSvn(ExState &State) {
    char Options[128] = "";

    if (SvnView != 0 && SvnView->Running) {
        Msg(S_INFO, "Already running...");
        return 0;
    }

    State.GetStrParam(this, Options, sizeof(Options));
    return Svn(Options);
}

int EView::Svn(const char *Options) {
    char Dir[MAXPATH] = "";
    char Command[256] = "";
    char buf[1024] = "";
    char *OnFiles = buf;
    ESvn *svn;

    if (GetDefaultDirectory(Model, Dir, sizeof(Dir)) == 0) return 0;

    strcpy(Command, SvnCommand);
    strcat(Command, " ");
    if (Options[0] != 0) {
        strcat(Command, Options);
        strcat(Command, " ");
    }

    switch (Model->GetContext()) {
    case CONTEXT_FILE:
        if (JustFileName(((EBuffer *)Model)->FileName, OnFiles, sizeof(buf)) != 0) return 0; // OnFiles points to buf
        break;
    case CONTEXT_SVNDIFF:
        OnFiles = strdup(SvnDiffView->OnFiles);
        break;
    case CONTEXT_SVN:
        OnFiles = ((ESvn *)Model)->MarkedAsList();
        if (!OnFiles) OnFiles = strdup(((ESvn *)Model)->OnFiles);
        break;
    }

    if (SvnView != 0) {
        SvnView->RunPipe(Dir, Command, OnFiles);
        svn = SvnView;
    } else {
        svn = new ESvn(0, &ActiveModel, Dir, Command, OnFiles);
    }
    if (OnFiles != buf) free(OnFiles);
    SwitchToModel(svn);
    return 1;
}

int EView::ClearSvnMessages(ExState &/*State*/) {
    if (SvnView != 0) {
        if (SvnView->Running) {
            Msg(S_INFO, "Running...");
            return 0;
        } else {
            SvnView->FreeLines();
            SvnView->UpdateList();
            return 1;
        }
    }
    return 0;
}

int EView::ViewSvn(ExState &/*State*/) {
    if (SvnView != 0) {
        SwitchToModel(SvnView);
        return 1;
    }
    return 0;
}

int EView::SvnDiff(ExState &State) {
    static char Opts[128] = "";
    char Options[128] = "";

    if (SvnDiffView != 0 && SvnDiffView->Running) {
        Msg(S_INFO, "Already running...");
        return 0;
    }

    if (State.GetStrParam(this, Options, sizeof(Options)) == 0) {
        if (MView->Win->GetStr("SVN diff options", sizeof(Opts), Opts, HIST_SVNDIFF) == 0) return 0;
        strcpy(Options, Opts);
    } else {
        if (MView->Win->GetStr("SVN diff options", sizeof(Options), Options, HIST_SVNDIFF) == 0) return 0;
    }
    return SvnDiff(Options);
}

int EView::RunSvnDiff(ExState &State) {
    char Options[128] = "";

    if (SvnDiffView != 0 && SvnDiffView->Running) {
        Msg(S_INFO, "Already running...");
        return 0;
    }

    State.GetStrParam(this, Options, sizeof(Options));
    return SvnDiff(Options);
}

int EView::SvnDiff(const char *Options) {
    char Dir[MAXPATH] = "";
    char Command[256] = "";
    char buf[1024] = "";
    char *OnFiles = buf;
    ESvnDiff *diffs;

    if (GetDefaultDirectory(Model, Dir, sizeof(Dir)) == 0) return 0;

    strcpy(Command, SvnCommand);
    strcat(Command, " diff ");
    if (Options[0] != 0) {
        strcat(Command, Options);
        strcat(Command, " ");
    }

    switch (Model->GetContext()) {
    case CONTEXT_FILE:
        if (JustFileName(((EBuffer *)Model)->FileName, OnFiles, sizeof(buf)) != 0) return 0; // OnFiles points to buf
        break;
    case CONTEXT_SVNDIFF:
        OnFiles = strdup(SvnDiffView->OnFiles);
        break;
    case CONTEXT_SVN:
        OnFiles = ((ESvn *)Model)->MarkedAsList();
        if (!OnFiles) OnFiles = strdup(((ESvn *)Model)->OnFiles);
        break;
    }

    if (SvnDiffView != 0) {
        SvnDiffView->RunPipe(Dir, Command, OnFiles);
        diffs = SvnDiffView;
    } else {
        diffs = new ESvnDiff(0, &ActiveModel, Dir, Command, OnFiles);
    }
    if (OnFiles != buf) free(OnFiles);
    SwitchToModel(diffs);
    return 1;
}

int EView::ViewSvnDiff(ExState &/*State*/) {
    if (SvnDiffView != 0) {
        SwitchToModel(SvnDiffView);
        return 1;
    }
    return 0;
}

int EView::SvnCommit(ExState &State) {
    static char Opts[128] = "";
    char Options[128] = "";

    if (SvnView != 0 && SvnView->Running) {
        Msg(S_INFO, "Already running...");
        return 0;
    }

    if (State.GetStrParam(this, Options, sizeof(Options)) == 0) {
        if (MView->Win->GetStr("SVN commit options", sizeof(Opts), Opts, HIST_SVNCOMMIT) == 0) return 0;
        strcpy(Options, Opts);
    } else {
        if (MView->Win->GetStr("SVN commit options", sizeof(Options), Options, HIST_SVNCOMMIT) == 0) return 0;
    }
    return SvnCommit(Options);
}

int EView::RunSvnCommit(ExState &State) {
    char Options[128] = "";

    if (SvnView != 0 && SvnView->Running) {
        Msg(S_INFO, "Already running...");
        return 0;
    }

    State.GetStrParam(this, Options, sizeof(Options));
    return SvnCommit(Options);
}

int EView::SvnCommit(const char *Options) {
    char Dir[MAXPATH] = "";
    char Command[256] = "";
    char buf[1024] = "";
    char *OnFiles = buf;
    ESvn *svn;

    if (GetDefaultDirectory(Model, Dir, sizeof(Dir)) == 0) return 0;

    strcpy(Command, SvnCommand);
    strcat(Command, " commit ");
    if (Options[0] != 0) {
        strcat(Command, Options);
        strcat(Command, " ");
    }

    switch (Model->GetContext()) {
    case CONTEXT_FILE:
        if (JustFileName(((EBuffer *)Model)->FileName, OnFiles, sizeof(buf)) != 0) return 0; // OnFiles points to buf
        break;
    case CONTEXT_SVNDIFF:
        OnFiles = strdup(SvnDiffView->OnFiles);
        break;
    case CONTEXT_SVN:
        OnFiles = ((ESvn *)Model)->MarkedAsList();
        if (!OnFiles) OnFiles = strdup(((ESvn *)Model)->OnFiles);
        break;
    }

    if (SvnView == 0) svn = new ESvn(0, &ActiveModel);
    else svn = SvnView;
    SwitchToModel(svn);
    svn->RunCommit(Dir, Command, OnFiles);
    if (OnFiles != buf) free(OnFiles);
    return 1;
}

int EView::ViewSvnLog(ExState &/*State*/) {
    if (SvnLogView != 0) {
        SwitchToModel(SvnLogView);
        return 1;
    }
    return 0;
}

/* ---- P4 (Perforce) ---- */

int EView::P4(ExState &State) {
    static char Opts[128] = "";
    char Options[128] = "";
    if (P4View != 0 && P4View->Running) { Msg(S_INFO, "Already running..."); return 0; }
    if (State.GetStrParam(this, Options, sizeof(Options)) == 0) {
        if (MView->Win->GetStr("P4 options", sizeof(Opts), Opts, HIST_P4) == 0) return 0;
        strcpy(Options, Opts);
    } else {
        if (MView->Win->GetStr("P4 options", sizeof(Options), Options, HIST_P4) == 0) return 0;
    }
    return P4(Options);
}

int EView::RunP4(ExState &State) {
    char Options[128] = "";
    if (P4View != 0 && P4View->Running) { Msg(S_INFO, "Already running..."); return 0; }
    State.GetStrParam(this, Options, sizeof(Options));
    return P4(Options);
}

int EView::P4(const char *Options) {
    char Dir[MAXPATH] = "";
    char Command[256] = "";
    char buf[1024] = "";
    char *OnFiles = buf;
    EP4 *p4;

    if (GetDefaultDirectory(Model, Dir, sizeof(Dir)) == 0) return 0;

    strcpy(Command, P4Command);
    strcat(Command, " ");
    if (Options[0] != 0) {
        strcat(Command, Options);
        strcat(Command, " ");
    }

    switch (Model->GetContext()) {
    case CONTEXT_FILE:
        if (JustFileName(((EBuffer *)Model)->FileName, OnFiles, sizeof(buf)) != 0) return 0;
        break;
    case CONTEXT_P4DIFF:
        OnFiles = strdup(P4DiffView->OnFiles);
        break;
    case CONTEXT_P4:
        OnFiles = ((EP4 *)Model)->MarkedAsList();
        if (!OnFiles) OnFiles = strdup(((EP4 *)Model)->OnFiles);
        break;
    }

    if (P4View == 0) p4 = new EP4(0, &ActiveModel);
    else p4 = P4View;
    SwitchToModel(p4);
    p4->RunPipe(Dir, Command, OnFiles);
    if (OnFiles != buf) free(OnFiles);
    return 1;
}

int EView::ClearP4Messages(ExState &/*State*/) {
    if (P4View != 0) {
        P4View->FreeLines();
        P4View->UpdateList();
        return 1;
    }
    return 0;
}

int EView::ViewP4(ExState &/*State*/) {
    if (P4View != 0) {
        SwitchToModel(P4View);
        return 1;
    }
    return 0;
}

int EView::P4Diff(ExState &State) {
    static char Opts[128] = "";
    char Options[128] = "";
    if (P4View != 0 && P4View->Running) { Msg(S_INFO, "Already running..."); return 0; }
    if (State.GetStrParam(this, Options, sizeof(Options)) == 0) {
        if (MView->Win->GetStr("P4 diff options", sizeof(Opts), Opts, HIST_P4DIFF) == 0) return 0;
        strcpy(Options, Opts);
    } else {
        if (MView->Win->GetStr("P4 diff options", sizeof(Options), Options, HIST_P4DIFF) == 0) return 0;
    }
    return P4Diff(Options);
}

int EView::RunP4Diff(ExState &State) {
    char Options[128] = "";
    if (P4View != 0 && P4View->Running) { Msg(S_INFO, "Already running..."); return 0; }
    State.GetStrParam(this, Options, sizeof(Options));
    return P4Diff(Options);
}

int EView::P4Diff(const char *Options) {
    char Dir[MAXPATH] = "";
    char Command[256] = "";
    char buf[1024] = "";
    char *OnFiles = buf;

    if (GetDefaultDirectory(Model, Dir, sizeof(Dir)) == 0) return 0;

    strcpy(Command, P4Command);
    strcat(Command, " diff -du ");
    if (Options[0] != 0) {
        strcat(Command, Options);
        strcat(Command, " ");
    }

    switch (Model->GetContext()) {
    case CONTEXT_FILE:
        if (JustFileName(((EBuffer *)Model)->FileName, OnFiles, sizeof(buf)) != 0) return 0;
        break;
    case CONTEXT_P4DIFF:
        OnFiles = strdup(P4DiffView->OnFiles);
        break;
    case CONTEXT_P4:
        OnFiles = ((EP4 *)Model)->MarkedAsList();
        if (!OnFiles) OnFiles = strdup(((EP4 *)Model)->OnFiles);
        break;
    }

    EP4Diff *diffs;
    if (P4DiffView != 0) {
        diffs = P4DiffView;
        SwitchToModel(diffs);
        diffs->RunPipe(Dir, Command, OnFiles);
    } else {
        diffs = new EP4Diff(0, &ActiveModel, Dir, Command, OnFiles);
    }
    if (OnFiles != buf) free(OnFiles);
    SwitchToModel(diffs);
    return 1;
}

int EView::ViewP4Diff(ExState &/*State*/) {
    if (P4DiffView != 0) {
        SwitchToModel(P4DiffView);
        return 1;
    }
    return 0;
}

int EView::P4Submit(ExState &State) {
    static char Opts[128] = "";
    char Options[128] = "";
    if (P4View != 0 && P4View->Running) { Msg(S_INFO, "Already running..."); return 0; }
    if (State.GetStrParam(this, Options, sizeof(Options)) == 0) {
        if (MView->Win->GetStr("P4 submit options", sizeof(Opts), Opts, HIST_P4SUBMIT) == 0) return 0;
        strcpy(Options, Opts);
    } else {
        if (MView->Win->GetStr("P4 submit options", sizeof(Options), Options, HIST_P4SUBMIT) == 0) return 0;
    }
    return P4Submit(Options);
}

int EView::RunP4Submit(ExState &State) {
    char Options[128] = "";
    if (P4View != 0 && P4View->Running) { Msg(S_INFO, "Already running..."); return 0; }
    State.GetStrParam(this, Options, sizeof(Options));
    return P4Submit(Options);
}

int EView::P4Submit(const char *Options) {
    char Dir[MAXPATH] = "";
    char Command[256] = "";
    char buf[1024] = "";
    char *OnFiles = buf;
    EP4 *p4;

    if (GetDefaultDirectory(Model, Dir, sizeof(Dir)) == 0) return 0;

    strcpy(Command, P4Command);
    strcat(Command, " submit ");
    if (Options[0] != 0) {
        strcat(Command, Options);
        strcat(Command, " ");
    }

    switch (Model->GetContext()) {
    case CONTEXT_FILE:
        if (JustFileName(((EBuffer *)Model)->FileName, OnFiles, sizeof(buf)) != 0) return 0;
        break;
    case CONTEXT_P4DIFF:
        OnFiles = strdup(P4DiffView->OnFiles);
        break;
    case CONTEXT_P4:
        OnFiles = ((EP4 *)Model)->MarkedAsList();
        if (!OnFiles) OnFiles = strdup(((EP4 *)Model)->OnFiles);
        break;
    }

    if (P4View == 0) p4 = new EP4(0, &ActiveModel);
    else p4 = P4View;
    SwitchToModel(p4);
    p4->RunCommit(Dir, Command, OnFiles);
    if (OnFiles != buf) free(OnFiles);
    return 1;
}

int EView::ViewP4Log(ExState &/*State*/) {
    if (P4LogView != 0) {
        SwitchToModel(P4LogView);
        return 1;
    }
    return 0;
}

int EView::P4Edit(ExState &/*State*/) {
    char Dir[MAXPATH] = "";
    char Command[256] = "";
    char buf[1024] = "";
    char *OnFiles = buf;

    if (GetDefaultDirectory(Model, Dir, sizeof(Dir)) == 0) return 0;

    strcpy(Command, P4Command);
    strcat(Command, " edit ");

    if (Model->GetContext() == CONTEXT_FILE) {
        if (JustFileName(((EBuffer *)Model)->FileName, OnFiles, sizeof(buf)) != 0) return 0;
    } else if (Model->GetContext() == CONTEXT_P4) {
        OnFiles = ((EP4 *)Model)->MarkedAsList();
        if (!OnFiles) OnFiles = strdup(((EP4 *)Model)->OnFiles);
    }

    if (P4View == 0) {
        EP4 *p4 = new EP4(0, &ActiveModel);
        SwitchToModel(p4);
        p4->RunPipe(Dir, Command, OnFiles);
    } else {
        SwitchToModel(P4View);
        P4View->RunPipe(Dir, Command, OnFiles);
    }
    if (OnFiles != buf) free(OnFiles);
    return 1;
}

/* ---- Git ---- */

int EView::Git(ExState &State) {
    static char Opts[128] = "";
    char Options[128] = "";
    if (GitView != 0 && GitView->Running) { Msg(S_INFO, "Already running..."); return 0; }
    if (State.GetStrParam(this, Options, sizeof(Options)) == 0) {
        if (MView->Win->GetStr("Git options", sizeof(Opts), Opts, HIST_GIT) == 0) return 0;
        strcpy(Options, Opts);
    } else {
        if (MView->Win->GetStr("Git options", sizeof(Options), Options, HIST_GIT) == 0) return 0;
    }
    return Git(Options);
}

int EView::RunGit(ExState &State) {
    char Options[128] = "";
    if (GitView != 0 && GitView->Running) { Msg(S_INFO, "Already running..."); return 0; }
    State.GetStrParam(this, Options, sizeof(Options));
    return Git(Options);
}

int EView::Git(const char *Options) {
    char Dir[MAXPATH] = "";
    char Command[MAXPATH + 256] = "";
    char buf[1024] = "";
    char *OnFiles = buf;

    if (GetDefaultDirectory(Model, Dir, sizeof(Dir)) == 0) return 0;

    strcpy(Command, GitCommand); strcat(Command, " -C "); strcat(Command, Dir);
    strcat(Command, " ");
    if (Options[0] != 0) { strcat(Command, Options); strcat(Command, " "); }

    switch (Model->GetContext()) {
    case CONTEXT_FILE:
        if (JustFileName(((EBuffer *)Model)->FileName, OnFiles, sizeof(buf)) != 0) return 0;
        break;
    case CONTEXT_GITDIFF:
        OnFiles = strdup(GitDiffView->OnFiles);
        break;
    case CONTEXT_GIT:
        OnFiles = ((EGit *)Model)->MarkedAsList();
        if (!OnFiles) OnFiles = strdup(((EGit *)Model)->OnFiles);
        break;
    }

    EGit *git;
    if (GitView == 0) git = new EGit(0, &ActiveModel);
    else git = GitView;
    SwitchToModel(git);
    git->RunPipe(Dir, Command, OnFiles);
    if (OnFiles != buf) free(OnFiles);
    return 1;
}

int EView::ClearGitMessages(ExState &/*State*/) {
    if (GitView != 0) { GitView->FreeLines(); GitView->UpdateList(); return 1; }
    return 0;
}

int EView::ViewGit(ExState &/*State*/) {
    if (GitView != 0) { SwitchToModel(GitView); return 1; }
    return 0;
}

int EView::GitDiff(ExState &State) {
    static char Opts[128] = "";
    char Options[128] = "";
    if (GitView != 0 && GitView->Running) { Msg(S_INFO, "Already running..."); return 0; }
    if (State.GetStrParam(this, Options, sizeof(Options)) == 0) {
        if (MView->Win->GetStr("Git diff options", sizeof(Opts), Opts, HIST_GITDIFF) == 0) return 0;
        strcpy(Options, Opts);
    } else {
        if (MView->Win->GetStr("Git diff options", sizeof(Options), Options, HIST_GITDIFF) == 0) return 0;
    }
    return GitDiff(Options);
}

int EView::RunGitDiff(ExState &State) {
    char Options[128] = "";
    if (GitView != 0 && GitView->Running) { Msg(S_INFO, "Already running..."); return 0; }
    State.GetStrParam(this, Options, sizeof(Options));
    return GitDiff(Options);
}

int EView::GitDiff(const char *Options) {
    char Dir[MAXPATH] = "";
    char Command[MAXPATH + 256] = "";
    char buf[1024] = "";
    char *OnFiles = buf;

    if (GetDefaultDirectory(Model, Dir, sizeof(Dir)) == 0) return 0;

    strcpy(Command, GitCommand); strcat(Command, " -C "); strcat(Command, Dir);
    strcat(Command, " diff ");
    if (Options[0] != 0) { strcat(Command, Options); strcat(Command, " "); }

    switch (Model->GetContext()) {
    case CONTEXT_FILE:
        if (JustFileName(((EBuffer *)Model)->FileName, OnFiles, sizeof(buf)) != 0) return 0;
        break;
    case CONTEXT_GITDIFF:
        OnFiles = strdup(GitDiffView->OnFiles);
        break;
    case CONTEXT_GIT:
        OnFiles = ((EGit *)Model)->MarkedAsList();
        if (!OnFiles) OnFiles = strdup(((EGit *)Model)->OnFiles);
        break;
    }

    EGitDiff *diffs;
    if (GitDiffView != 0) { diffs = GitDiffView; SwitchToModel(diffs); diffs->RunPipe(Dir, Command, OnFiles); }
    else diffs = new EGitDiff(0, &ActiveModel, Dir, Command, OnFiles);
    if (OnFiles != buf) free(OnFiles);
    SwitchToModel(diffs);
    return 1;
}

int EView::ViewGitDiff(ExState &/*State*/) {
    if (GitDiffView != 0) { SwitchToModel(GitDiffView); return 1; }
    return 0;
}

int EView::GitCommit(ExState &State) {
    static char Opts[128] = "";
    char Options[128] = "";
    if (GitView != 0 && GitView->Running) { Msg(S_INFO, "Already running..."); return 0; }
    if (State.GetStrParam(this, Options, sizeof(Options)) == 0) {
        if (MView->Win->GetStr("Git commit options", sizeof(Opts), Opts, HIST_GITCOMMIT) == 0) return 0;
        strcpy(Options, Opts);
    } else {
        if (MView->Win->GetStr("Git commit options", sizeof(Options), Options, HIST_GITCOMMIT) == 0) return 0;
    }
    return GitCommit(Options);
}

int EView::RunGitCommit(ExState &State) {
    char Options[128] = "";
    if (GitView != 0 && GitView->Running) { Msg(S_INFO, "Already running..."); return 0; }
    State.GetStrParam(this, Options, sizeof(Options));
    return GitCommit(Options);
}

int EView::GitCommit(const char *Options) {
    char Dir[MAXPATH] = "";
    char Command[MAXPATH + 256] = "";
    char buf[1024] = "";
    char *OnFiles = buf;

    if (GetDefaultDirectory(Model, Dir, sizeof(Dir)) == 0) return 0;

    strcpy(Command, GitCommand); strcat(Command, " -C "); strcat(Command, Dir);
    strcat(Command, " commit ");
    if (Options[0] != 0) { strcat(Command, Options); strcat(Command, " "); }

    switch (Model->GetContext()) {
    case CONTEXT_FILE:
        if (JustFileName(((EBuffer *)Model)->FileName, OnFiles, sizeof(buf)) != 0) return 0;
        break;
    case CONTEXT_GITDIFF:
        OnFiles = strdup(GitDiffView->OnFiles);
        break;
    case CONTEXT_GIT:
        OnFiles = ((EGit *)Model)->MarkedAsList();
        if (!OnFiles) OnFiles = strdup(((EGit *)Model)->OnFiles);
        break;
    }

    EGit *git;
    if (GitView == 0) git = new EGit(0, &ActiveModel);
    else git = GitView;
    SwitchToModel(git);
    git->RunCommit(Dir, Command, OnFiles);
    if (OnFiles != buf) free(OnFiles);
    return 1;
}

int EView::ViewGitLog(ExState &/*State*/) {
    if (GitLogView != 0) { SwitchToModel(GitLogView); return 1; }
    return 0;
}
