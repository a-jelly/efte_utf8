/*
 * o_gitdiff.h
 *
 * Copyright (c) 2008, eFTE SF Group (see AUTHORS file)
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 * S.Pinigin copy o_cvsdiff.h and replace cvs/Cvs/CVS to git/Git/GIT.
 *
 * Class showing output from GIT diff command. Allows copying of lines
 * to clipboard and allows to jump to lines in real sources.
 */

#ifndef GITDIFF_H_
#define GITDIFF_H_

class EGitDiff: public EGitBase {
public:
    int CurrLine, ToLine, InToFile;
    char *CurrFile;

    EGitDiff(int createFlags, EModel **ARoot, const char *Dir, const char *ACommand, char *AOnFiles);
    ~EGitDiff();

    void ParseFromTo(const char *line, int len);
    virtual void ParseLine(const char *line, int len);
    // Returns 0 if OK
    virtual int RunPipe(const char *Dir, const char *Command, const char *OnFiles);

    virtual int ExecCommand(int Command, ExState &State);
    int BlockCopy(int Append);

    virtual int GetContext() const;
    virtual EEventMap *GetEventMap();
};

extern EGitDiff *GitDiffView;

#endif
