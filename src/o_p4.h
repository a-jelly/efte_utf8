/*
 * o_p4.h
 *
 * Copyright (c) 2008, eFTE SF Group (see AUTHORS file)
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 *
 * S.Pinigin copy o_cvs.h and replace cvs/Cvs/CVS to p4/P4/P4.
 *
 * Class providing access to most of P4 commands.
 */

#ifndef P4_H_
#define P4_H_

class EP4: public EP4Base {
public:
    char *LogFile;
    int Commiting;

    EP4(int createFlags, EModel **ARoot, const char *Dir, const char *ACommand, const char *AOnFiles);
    EP4(int createFlags, EModel **ARoot);
    ~EP4();

    void RemoveLogFile();
    // Return marked files in allocated space separated list
    char *MarkedAsList();
    // Return P4 status char of file or 0 if unknown
    // (if char is lowercase, state was guessed from last command invoked upon file)
    char GetFileStatus(const char *file);

    virtual void ParseLine(const char *line, int len);
    // Returns 0 if OK
    virtual int RunPipe(const char *Dir, const char *Command, const char *OnFiles);
    virtual void ClosePipe();
    // Start commit process (opens message buffer), returns 0 if OK
    int RunCommit(const char *Dir, const char *Command, const char *OnFiles);
    // Finish commit process (called on message buffer close), returns 0 if OK
    int DoneCommit(int commit);

    virtual int CanQuit() const;
    virtual int ConfQuit(GxView *V, int multiFile);

    virtual int GetContext() const;
    virtual EEventMap *GetEventMap();
};

extern EP4 *P4View;

#endif
