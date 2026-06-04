/*    h_sublime.h
 *
 *    sublime-syntax highlighting engine for eFTE.
 *
 *    Public, eFTE-independent core API. The same core is used by the
 *    Hilit_SUBLIME() highlighter and by the standalone test harness.
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 */

#ifndef H_SUBLIME_H
#define H_SUBLIME_H

#include <stddef.h>

/* Opaque compiled grammar. */
struct SubGrammar;

/* Load and compile a .sublime-syntax (YAML) file.
 * On failure returns 0 and writes a message into err (if err != 0). */
SubGrammar *SubLoadGrammar(const char *path, char *err, size_t errlen);

void SubFreeGrammar(SubGrammar *g);

/* Highlight one line of UTF-8 text.
 *   chars/len   : the line bytes (no trailing newline).
 *   entryState  : interned context-stack id at start of line (0 == initial).
 *   slot        : caller buffer of at least `len` bytes; on return slot[k] is
 *                 the CLR_* colour slot for byte k.
 * Returns the interned context-stack id for the *end* of the line, to be fed
 * back in as entryState of the next line. */
int SubHighlightLine(SubGrammar *g, const char *chars, int len,
                     int entryState, unsigned char *slot);

/* eFTE integration: lazily load grammar on first use.
 * Only available in the full editor build (not SUB_NO_FTE). */
#ifndef SUB_NO_FTE
class EColorize;
void SubEnsureGrammar(EColorize *col);
#endif

#endif /* H_SUBLIME_H */
