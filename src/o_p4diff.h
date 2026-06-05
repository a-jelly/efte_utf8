/*
 * o_p4diff.h
 *
 * Copyright (c) 2008, eFTE SF Group (see AUTHORS file)
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 * S.Pinigin copy o_cvsdiff.h and replace cvs/Cvs/CVS to p4/P4/P4.
 *
 * Class showing output from P4 diff command. Allows copying of lines
 * to clipboard and allows to jump to lines in real sources.
 */

#ifndef P4DIFF_H_
#define P4DIFF_H_

class EP4Diff: public EP4Base {
public:
    int CurrLine, ToLine, InToFile;
    char *CurrFile;

    EP4Diff(int createFlags, EModel **ARoot, const char *Dir, const char *ACommand, char *AOnFiles);
    ~EP4Diff();

    void ParseFromTo(const char *line, int len);
    virtual void ParseLine(const char *line, int len);
    // Returns 0 if OK
    virtual int RunPipe(const char *Dir, const char *Command, const char *OnFiles);

    virtual int ExecCommand(int Command, ExState &State);
    int BlockCopy(int Append);

    virtual int GetContext() const;
    virtual EEventMap *GetEventMap();
};

extern EP4Diff *P4DiffView;

#endif
