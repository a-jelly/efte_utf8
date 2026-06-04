/*    h_sublime.cpp
 *
 *    sublime-syntax highlighting engine for eFTE.
 *
 *    A single table-driven engine that consumes community .sublime-syntax
 *    grammars (YAML) and drives eFTE's existing per-line / per-cell colour
 *    machinery. Regex matching is done with PCRE2 (8-bit, UTF + UCP).
 *
 *    The engine core (grammar model, YAML loader, matcher) is independent of
 *    eFTE internals so it can be unit-tested standalone; define SUB_NO_FTE to
 *    compile only the core (without the Hilit_SUBLIME wrapper).
 *
 *    MVP feature set: variables, contexts, prototype / meta_include_prototype,
 *    meta_scope / meta_content_scope, and per-rule match / scope / captures /
 *    push / pop / set / include (scalar context names only).
 *    Deferred: embed/escape, with_prototype, branch_point/branch/fail, push
 *    lists and inline anonymous contexts, numeric pop.
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>

#include <yaml.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "h_sublime.h"

#ifdef SUB_NO_FTE
/* Standalone build: mirror the CLR_* slot indices from c_mode.h. */
enum {
    CLR_Normal = 0, CLR_Keyword = 1, CLR_String = 2, CLR_Comment = 3,
    CLR_CPreprocessor = 4, CLR_Regexp = 5, CLR_Header = 6, CLR_Quotes = 7,
    CLR_Number = 8, CLR_HexNumber = 9, CLR_FloatNumber = 11, CLR_Function = 12,
    CLR_Command = 13, CLR_Tag = 14, CLR_Punctuation = 15, CLR_Variable = 21,
    CLR_Symbol = 22, CLR_Directive = 23, CLR_Label = 24, CLR_Special = 25
};
#else
#include "fte.h"
#endif

/* ------------------------------------------------------------------ */
/* Grammar model                                                       */
/* ------------------------------------------------------------------ */

enum { ACT_NONE = 0, ACT_PUSH, ACT_SET, ACT_POP };

struct SubRule {
    pcre2_code *re;                              /* MATCH rule (else 0)   */
    int         slot;                            /* scope -> CLR_*        */
    std::vector<std::pair<int,int> > captures;   /* (group, slot)         */
    int         action;                          /* ACT_*                 */
    std::vector<int> targets;                    /* ctx indices for push/set */
    int         include;                         /* ctx index for include  */

    SubRule() : re(0), slot(-1), action(ACT_NONE),
                include(-1) {}
};

struct SubContext {
    std::string          name;
    int                  metaSlot;          /* meta_scope         -> CLR_* */
    int                  metaContentSlot;   /* meta_content_scope -> CLR_* */
    bool                 includePrototype;
    std::vector<SubRule> rules;

    SubContext() : metaSlot(-1), metaContentSlot(-1), includePrototype(true) {}
};

struct SubGrammar {
    std::vector<SubContext>              ctx;
    std::unordered_map<std::string,int>  ctxIndex;
    int                                  mainIdx;
    int                                  prototypeIdx;

    /* context-stack interning: state id <-> stack of context indices */
    std::vector<std::vector<int> >       stacks;
    std::unordered_map<std::string,int>  stackKey;

    pcre2_match_data                    *md;

    SubGrammar() : mainIdx(0), prototypeIdx(-1), md(0) {}
};

/* ------------------------------------------------------------------ */
/* Small helpers                                                       */
/* ------------------------------------------------------------------ */

static int utf8_seqlen_at(const char *s, int len, int i) {
    if (i >= len) return 1;
    unsigned char c = (unsigned char)s[i];
    int n = 1;
    if      (c < 0x80)          n = 1;
    else if ((c & 0xE0) == 0xC0) n = 2;
    else if ((c & 0xF0) == 0xE0) n = 3;
    else if ((c & 0xF8) == 0xF0) n = 4;
    if (i + n > len) n = len - i;
    if (n < 1) n = 1;
    return n;
}

static bool startsWith(const std::string &s, const char *pfx) {
    size_t n = strlen(pfx);
    return s.size() >= n && memcmp(s.data(), pfx, n) == 0;
}
static bool contains(const std::string &s, const char *sub) {
    return s.find(sub) != std::string::npos;
}

/* ------------------------------------------------------------------ */
/* scope (TextMate dotted name) -> CLR_* colour slot                   */
/* ------------------------------------------------------------------ */

static int tokenToSlot(const std::string &t) {
    /* delimiters first, so e.g. punctuation.definition.comment.begin
       colours with its region rather than as plain punctuation */
    if (startsWith(t, "comment") || contains(t, ".comment")) return CLR_Comment;
    if (startsWith(t, "constant.character.escape"))           return CLR_Special;
    if (startsWith(t, "string") || contains(t, ".string"))    return CLR_String;

    if (startsWith(t, "constant.numeric")) {
        if (contains(t, "hex") || contains(t, "oct") || contains(t, "bin"))
            return CLR_HexNumber;            /* CLR_Octal/Binary alias Hex */
        if (contains(t, "float")) return CLR_FloatNumber;
        return CLR_Number;
    }
    if (startsWith(t, "constant.language")) return CLR_Keyword;
    if (startsWith(t, "constant"))          return CLR_Number;

    if (contains(t, "annotation"))          return CLR_Directive;

    if (startsWith(t, "keyword"))           return CLR_Keyword;
    if (startsWith(t, "storage"))           return CLR_Keyword;

    if (startsWith(t, "variable.function")) return CLR_Function;
    if (startsWith(t, "entity.name.function") ||
        startsWith(t, "support.function"))  return CLR_Function;
    if (startsWith(t, "entity.name.tag"))   return CLR_Tag;
    if (startsWith(t, "entity.name.type") ||
        startsWith(t, "support.type"))      return CLR_Symbol;
    if (startsWith(t, "variable"))          return CLR_Variable;
    if (startsWith(t, "punctuation"))       return CLR_Punctuation;

    return -1;                              /* unknown */
}

static int scopeToSlot(const std::string &scope) {
    /* scope may be several space-separated names; the most specific
       (rightmost recognised) wins, defaulting to Normal. */
    int result = CLR_Normal;
    size_t i = 0;
    while (i < scope.size()) {
        while (i < scope.size() && scope[i] == ' ') i++;
        size_t j = i;
        while (j < scope.size() && scope[j] != ' ') j++;
        if (j > i) {
            int s = tokenToSlot(scope.substr(i, j - i));
            if (s >= 0) result = s;
        }
        i = j;
    }
    return result;
}

/* ------------------------------------------------------------------ */
/* YAML access helpers                                                 */
/* ------------------------------------------------------------------ */

static yaml_node_t *ynode(yaml_document_t *doc, yaml_node_t *map, const char *key) {
    if (!map || map->type != YAML_MAPPING_NODE) return 0;
    for (yaml_node_pair_t *p = map->data.mapping.pairs.start;
         p < map->data.mapping.pairs.top; p++) {
        yaml_node_t *k = yaml_document_get_node(doc, p->key);
        if (k && k->type == YAML_SCALAR_NODE &&
            strcmp((const char *)k->data.scalar.value, key) == 0)
            return yaml_document_get_node(doc, p->value);
    }
    return 0;
}

static std::string yscalar(yaml_node_t *n) {
    if (n && n->type == YAML_SCALAR_NODE)
        return std::string((const char *)n->data.scalar.value,
                           n->data.scalar.length);
    return std::string();
}

static std::string expandVars(const std::string &in,
                              std::unordered_map<std::string,std::string> &vars,
                              int depth) {

    if (depth > 16 || in.find("{{") == std::string::npos) return in;
    
    std::string out;
    out.reserve(in.size());
    
    for (size_t i = 0; i < in.size();) {
        if (in[i] == '{' && i + 1 < in.size() && in[i + 1] == '{') {
            size_t e = in.find("}}", i + 2);
            if (e != std::string::npos) {
                std::string name = in.substr(i + 2, e - (i + 2));
                std::unordered_map<std::string,std::string>::iterator it = vars.find(name);
                
                if (it != vars.end()) {
                    out += expandVars(it->second, vars, depth + 1);
                } else {
                    out += "{{";
                    out += name;
                    out += "}}";
                }
                
                i = e + 2;
                continue;
            }
        }
        out += in[i++];
    }
    return out;
}

/* ------------------------------------------------------------------ */
/* Stack interning (context stack <-> 16-bit state id)                 */
/* ------------------------------------------------------------------ */

static int internStack(SubGrammar *g, const std::vector<int> &s) {
    std::string key;
    key.reserve(s.size() * 4);
    char buf[16];
    for (size_t i = 0; i < s.size(); i++) {
        snprintf(buf, sizeof(buf), "%d,", s[i]);
        key += buf;
    }
    std::unordered_map<std::string,int>::iterator it = g->stackKey.find(key);
    if (it != g->stackKey.end()) return it->second;
    if (g->stacks.size() >= 0xFFFF) return 0;   /* overflow: restart at main */
    int id = (int)g->stacks.size();
    g->stacks.push_back(s);
    g->stackKey[key] = id;
    return id;
}

static int stackContentSlot(SubGrammar *g, const std::vector<int> &s) {
    for (int i = (int)s.size() - 1; i >= 0; i--) {
        const SubContext &c = g->ctx[s[i]];
        if (c.metaContentSlot >= 0) return c.metaContentSlot;
        if (c.metaSlot >= 0)        return c.metaSlot;
    }
    return CLR_Normal;
}

static pcre2_code *compileRe(const std::string &pat) {
    int errc;
    PCRE2_SIZE erro;
    pcre2_code *re = pcre2_compile((PCRE2_SPTR)pat.c_str(), pat.size(),
                                   PCRE2_UTF | PCRE2_UCP, &errc, &erro, 0);
    if (re) {
        pcre2_jit_compile(re, PCRE2_JIT_COMPLETE);
    }
    return re;
}

/* parse one rule mapping node; appends to ctx.rules or sets ctx meta */
static void parseRule(SubGrammar *g, yaml_document_t *doc, yaml_node_t *m,
                      int ctxIdx,
                      std::unordered_map<std::string,std::string> &vars) {
    if (!m || m->type != YAML_MAPPING_NODE) return;

    /* context-level meta directives appear as list items */
    yaml_node_t *ms  = ynode(doc, m, "meta_scope");
    yaml_node_t *mcs = ynode(doc, m, "meta_content_scope");
    yaml_node_t *mip = ynode(doc, m, "meta_include_prototype");
    if (ms)  { g->ctx[ctxIdx].metaSlot        = scopeToSlot(yscalar(ms));  return; }
    if (mcs) { g->ctx[ctxIdx].metaContentSlot = scopeToSlot(yscalar(mcs)); return; }
    if (mip) { if (yscalar(mip) == "false") g->ctx[ctxIdx].includePrototype = false; return; }

    SubRule r;

    yaml_node_t *inc = ynode(doc, m, "include");
    if (inc) {
        std::string name = yscalar(inc);
        /* cross-syntax includes ("scope:...") are not supported in MVP */
        if (name.compare(0, 6, "scope:") != 0) {
            std::unordered_map<std::string,int>::iterator it = g->ctxIndex.find(name);
            if (it != g->ctxIndex.end()) { r.include = it->second; g->ctx[ctxIdx].rules.push_back(r); }
        }
        return;
    }

    yaml_node_t *mt = ynode(doc, m, "match");
    if (!mt) return;
    r.re = compileRe(expandVars(yscalar(mt), vars, 0));
    if (!r.re) return;   /* drop rules whose regex PCRE2 cannot compile */

    yaml_node_t *sc = ynode(doc, m, "scope");
    if (sc) r.slot = scopeToSlot(yscalar(sc));

    yaml_node_t *cap = ynode(doc, m, "captures");
    if (cap && cap->type == YAML_MAPPING_NODE) {
        for (yaml_node_pair_t *p = cap->data.mapping.pairs.start;
             p < cap->data.mapping.pairs.top; p++) {
            yaml_node_t *k = yaml_document_get_node(doc, p->key);
            yaml_node_t *v = yaml_document_get_node(doc, p->value);
            if (k && v) r.captures.push_back(
                std::make_pair(atoi(yscalar(k).c_str()), scopeToSlot(yscalar(v))));
        }
    }

    yaml_node_t *push = ynode(doc, m, "push");
    yaml_node_t *set  = ynode(doc, m, "set");
    yaml_node_t *pop  = ynode(doc, m, "pop");

    yaml_node_t *act_node = push ? push : set;
    int          act_kind = push ? ACT_PUSH : (set ? ACT_SET : ACT_NONE);

    if (act_node && act_kind != ACT_NONE) {
        if (act_node->type == YAML_SCALAR_NODE) {
            std::unordered_map<std::string,int>::iterator it = g->ctxIndex.find(yscalar(act_node));
            if (it != g->ctxIndex.end()) {
                r.action = act_kind;
                r.targets.push_back(it->second);
            }
        } else if (act_node->type == YAML_SEQUENCE_NODE && act_node->data.sequence.items.start < act_node->data.sequence.items.top) {
            
            yaml_node_t *first = yaml_document_get_node(doc, *act_node->data.sequence.items.start);
            
            if (first && first->type == YAML_MAPPING_NODE) {
                int anonIdx = (int)g->ctx.size();
                SubContext anonCtx;
                char buf[32];
                snprintf(buf, sizeof(buf), "__anon_%d", anonIdx);
                anonCtx.name = buf;
                g->ctx.push_back(anonCtx);

                for (yaml_node_item_t *it = act_node->data.sequence.items.start;
                     it < act_node->data.sequence.items.top; it++) {
                    parseRule(g, doc, yaml_document_get_node(doc, *it), anonIdx, vars);
                }

                r.action = act_kind;
                r.targets.push_back(anonIdx);
            } else {
                for (yaml_node_item_t *it = act_node->data.sequence.items.start;
                     it < act_node->data.sequence.items.top; it++) {
                    yaml_node_t *item = yaml_document_get_node(doc, *it);
                    if (item && item->type == YAML_SCALAR_NODE) {
                        std::unordered_map<std::string,int>::iterator ci = g->ctxIndex.find(yscalar(item));
                        if (ci != g->ctxIndex.end())
                            r.targets.push_back(ci->second);
                    }
                }
                if (!r.targets.empty()) r.action = act_kind;
            }
        }
    } else if (pop) {
        r.action = ACT_POP;
    }

    g->ctx[ctxIdx].rules.push_back(r);
}

SubGrammar *SubLoadGrammar(const char *path, char *err, size_t errlen) {
    FILE *f = fopen(path, "rb");
    if (!f) { if (err) snprintf(err, errlen, "cannot open %s", path); return 0; }

    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, f);
    if (!yaml_parser_load(&parser, &doc)) {
        if (err) snprintf(err, errlen, "YAML parse error in %s", path);
        yaml_parser_delete(&parser);
        fclose(f);
        return 0;
    }

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    yaml_node_t *contexts = ynode(&doc, root, "contexts");
    if (!root || !contexts || contexts->type != YAML_MAPPING_NODE) {
        if (err) snprintf(err, errlen, "no 'contexts' mapping in %s", path);
        yaml_document_delete(&doc);
        yaml_parser_delete(&parser);
        fclose(f);
        return 0;
    }

    SubGrammar *g = new SubGrammar();

    /* variables */
    std::unordered_map<std::string,std::string> vars;
    yaml_node_t *vnode = ynode(&doc, root, "variables");
    if (vnode && vnode->type == YAML_MAPPING_NODE) {
        for (yaml_node_pair_t *p = vnode->data.mapping.pairs.start;
             p < vnode->data.mapping.pairs.top; p++) {
            yaml_node_t *k = yaml_document_get_node(&doc, p->key);
            yaml_node_t *v = yaml_document_get_node(&doc, p->value);
            if (k && v) vars[yscalar(k)] = yscalar(v);
        }
    }

    /* pass 1: name every context so push/set/include can resolve */
    for (yaml_node_pair_t *p = contexts->data.mapping.pairs.start;
         p < contexts->data.mapping.pairs.top; p++) {
        yaml_node_t *k = yaml_document_get_node(&doc, p->key);
        if (!k || k->type != YAML_SCALAR_NODE) continue;
        std::string name = yscalar(k);
        g->ctxIndex[name] = (int)g->ctx.size();
        SubContext c;
        c.name = name;
        g->ctx.push_back(c);
    }


    /* pass 2: parse rules */
    int ci = 0;
    for (yaml_node_pair_t *p = contexts->data.mapping.pairs.start;
         p < contexts->data.mapping.pairs.top; p++, ci++) {
        yaml_node_t *seq = yaml_document_get_node(&doc, p->value);
        if (!seq || seq->type != YAML_SEQUENCE_NODE) continue;
        
        for (yaml_node_item_t *it = seq->data.sequence.items.start;
             it < seq->data.sequence.items.top; it++) {
            parseRule(g, &doc, yaml_document_get_node(&doc, *it), ci, vars);
        }
    }

    std::unordered_map<std::string,int>::iterator mit = g->ctxIndex.find("main");
    g->mainIdx = (mit != g->ctxIndex.end()) ? mit->second : 0;
    std::unordered_map<std::string,int>::iterator pit = g->ctxIndex.find("prototype");
    g->prototypeIdx = (pit != g->ctxIndex.end()) ? pit->second : -1;

    g->md = pcre2_match_data_create(64, 0);

    /* state id 0 == [main] */
    std::vector<int> init;
    init.push_back(g->mainIdx);
    internStack(g, init);

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
    fclose(f);
    return g;
}

void SubFreeGrammar(SubGrammar *g) {
    if (!g) return;
    for (size_t c = 0; c < g->ctx.size(); c++)
        for (size_t r = 0; r < g->ctx[c].rules.size(); r++)
            if (g->ctx[c].rules[r].re) pcre2_code_free(g->ctx[c].rules[r].re);
    if (g->md) pcre2_match_data_free(g->md);
    delete g;
}

/* ------------------------------------------------------------------ */
/* Matching                                                            */
/* ------------------------------------------------------------------ */

#define SUB_MAXGRP 64

struct SubBest {
    int        start;
    int        end;
    int        slot;
    const std::vector<std::pair<int,int> > *captures;
    int        action;
    const std::vector<int> *targets;
    PCRE2_SIZE ov[2 * SUB_MAXGRP];
    int        ngrp;
};


static void scanContext(SubGrammar *g, int ctxIdx,
                        const char *chars, int len, int pos,
                        SubBest &best, std::vector<char> &visited) {
    if (ctxIdx < 0 || visited[ctxIdx]) return;
    visited[ctxIdx] = 1;
    const SubContext &c = g->ctx[ctxIdx];
    for (size_t k = 0; k < c.rules.size(); k++) {
        const SubRule &r = c.rules[k];
        if (r.include >= 0) { scanContext(g, r.include, chars, len, pos, best, visited); continue; }
        if (!r.re) continue;
        
        int options = 0;
        if (r.action == ACT_NONE) {
            options |= PCRE2_NOTEMPTY;
        }

        int rc = pcre2_match(r.re, (PCRE2_SPTR)chars, len, pos, options, g->md, 0);
        if (rc < 0) continue;                       /* no match / error */
        
        PCRE2_SIZE *ov = pcre2_get_ovector_pointer(g->md);
        int ms = (int)ov[0];
        if (best.start < 0 || ms < best.start) {    /* earliest wins, ties keep first */
            best.start    = ms;
            best.end      = (int)ov[1];
            best.slot     = r.slot;
            best.captures = &r.captures;
            best.action   = r.action;
            best.targets  = &r.targets;
            int n = (rc > SUB_MAXGRP) ? SUB_MAXGRP : (rc > 0 ? rc : 1);
            best.ngrp = n;
            for (int x = 0; x < 2 * n; x++) best.ov[x] = ov[x];
        }
    }
}

int SubHighlightLine(SubGrammar *g, const char *chars, int len,
                     int entryState, unsigned char *slot) {
    if (!g || g->ctx.empty()) return 0;
    if (entryState < 0 || entryState >= (int)g->stacks.size()) entryState = 0;

    std::vector<int> stack = g->stacks[entryState];
    if (stack.empty()) stack.push_back(g->mainIdx);

    for (int k = 0; k < len; k++) slot[k] = (unsigned char)CLR_Normal;

    int contentSlot = stackContentSlot(g, stack);
    int pos = 0, lastPos = -1, iter = 0;
    const int maxIter = (int)g->ctx.size() + 16;
    std::vector<char> visited(g->ctx.size());

    while (pos <= len) {
        int top = stack.back();

        SubBest best;
        best.start = -1;
        for (size_t z = 0; z < visited.size(); z++) visited[z] = 0;
        if (g->ctx[top].includePrototype && g->prototypeIdx >= 0 && top != g->prototypeIdx)
            scanContext(g, g->prototypeIdx, chars, len, pos, best, visited);
        scanContext(g, top, chars, len, pos, best, visited);

        if (best.start < 0) {                       /* nothing more matches */
            for (int k = pos; k < len; k++) slot[k] = (unsigned char)contentSlot;
            break;
        }

        int ms = best.start, me = best.end;
        int matchSlot = (best.slot >= 0) ? best.slot : contentSlot;
        for (int k = pos; k < ms && k < len; k++) slot[k] = (unsigned char)contentSlot;
        for (int k = (ms < 0 ? 0 : ms); k < me && k < len; k++) slot[k] = (unsigned char)matchSlot;

        for (size_t ccap = 0; ccap < best.captures->size(); ccap++) {
            int grp = (*best.captures)[ccap].first;
            int cs  = (*best.captures)[ccap].second;
            if (grp >= 0 && grp < best.ngrp && best.ov[2 * grp] != PCRE2_UNSET) {
                int gs = (int)best.ov[2 * grp], ge = (int)best.ov[2 * grp + 1];
                for (int k = gs; k < ge && k < len; k++) slot[k] = (unsigned char)cs;
            }
        }

        bool changed = false;
        switch (best.action) {
        case ACT_PUSH:
            if (best.targets && !best.targets->empty()) {
                for (size_t t = 0; t < best.targets->size(); t++)
                    stack.push_back((*best.targets)[t]);
                changed = true;
            }
            break;
        case ACT_SET:
            if (best.targets && !best.targets->empty()) {
                stack.back() = (*best.targets)[0];
                for (size_t t = 1; t < best.targets->size(); t++)
                    stack.push_back((*best.targets)[t]);
                changed = true;
            }
            break;
        case ACT_POP:  if (stack.size() > 1) { stack.pop_back(); changed = true; } break;
        default: break;
        }
        if (changed) contentSlot = stackContentSlot(g, stack);

        if (me > ms) {
            pos = me;
        } else {                                    /* zero-width match */
            if (changed) {
                pos = ms;                           /* stay; new context */
            } else {
                int adv = utf8_seqlen_at(chars, len, ms);
                for (int k = ms; k < ms + adv && k < len; k++) slot[k] = (unsigned char)contentSlot;
                pos = ms + adv;
            }
        }

        if (pos == lastPos) {                        /* loop guard */
            if (++iter > maxIter) {
                if (pos < len) {
                    int adv = utf8_seqlen_at(chars, len, pos);
                    for (int k = pos; k < pos + adv && k < len; k++) slot[k] = (unsigned char)contentSlot;
                    pos += adv;
                } else {
                    break;
                }
                iter = 0;
            }
        } else {
            lastPos = pos;
            iter = 0;
        }
    }

    return internStack(g, stack);
}

/* ------------------------------------------------------------------ */
/* eFTE highlighter wrapper                                            */
/* ------------------------------------------------------------------ */

#ifndef SUB_NO_FTE

#include <unistd.h>   /* access() */
#if __has_include("config.h")
#include "config.h"   /* EFTE_INSTALL_DIR from CMake */
#endif

/* Try to load a grammar file from a specific directory + SyntaxFile.
 * Returns 1 on success (col->sg set), 0 on failure. */
static int tryGrammarAt(EColorize *col, const char *dir) {
    char path[MAXPATH], err[256];
    if (dir[0]) {
        JoinDirFile(path, dir, col->SyntaxFile);
    } else {
        strncpy(path, col->SyntaxFile, sizeof(path));
        path[sizeof(path) - 1] = 0;
    }
    char expanded[MAXPATH];
    if (ExpandPath(path, expanded, sizeof(expanded)) == 0 &&
        access(expanded, R_OK) == 0) {
        col->sg = SubLoadGrammar(expanded, err, sizeof(err));
        if (col->sg) return 1;
    }
    return 0;
}

void SubEnsureGrammar(EColorize *col) {
    if (col->SyntaxParser != HILIT_SUBLIME || col->sg != 0 || col->SyntaxFile == 0)
        return;

    /* Same search order as LoadFile in cfte.cpp: */
    extern char ConfigDir[MAXPATH];
    extern char ConfigFileName[MAXPATH];

    /* 1. ConfigDir (set by CFteMain from the resolved config path) */
    if (tryGrammarAt(col, ConfigDir)) return;

    /* 2. directory of ConfigFileName (covers -C with full path) */
    {
        char cfgdir[MAXPATH];
        strncpy(cfgdir, ConfigFileName, sizeof(cfgdir));
        cfgdir[sizeof(cfgdir) - 1] = 0;
        int k;
        for (k = (int)strlen(cfgdir); k > 0; k--)
            if (ISSLASH(cfgdir[k - 1])) break;
        cfgdir[k] = 0;
        if (cfgdir[0] && tryGrammarAt(col, cfgdir)) return;
    }

    /* 3. ~/.efte/ */
    if (tryGrammarAt(col, "~/.efte")) return;

    /* 4-5. standard install paths */
#ifdef EFTE_INSTALL_DIR
    {
        char dir[MAXPATH];
        snprintf(dir, sizeof(dir), "%s/share/efte/local", EFTE_INSTALL_DIR);
        if (tryGrammarAt(col, dir)) return;
        snprintf(dir, sizeof(dir), "%s/share/efte/config", EFTE_INSTALL_DIR);
        if (tryGrammarAt(col, dir)) return;
    }
#endif

    /* 6. common fallback prefixes if EFTE_INSTALL_DIR not available */
    if (tryGrammarAt(col, "/usr/local/share/efte/config")) return;
    if (tryGrammarAt(col, "/usr/share/efte/config")) return;

    /* 7. SyntaxFile as-is (relative to CWD) */
    tryGrammarAt(col, "");
}

int Hilit_SUBLIME(EBuffer *BF, int /*LN*/, PCell B, int Pos, int Width,
                  ELine *Line, hlState &State, hsState *StateMap, int *ECol) {
    EColorize *col = BF->Mode->fColorize;
    SubGrammar *g  = (SubGrammar *)col->sg;
    HILIT_VARS(col->Colors, Line);

    if (!g) { if (ECol) *ECol = 0; return 0; }

    static std::vector<unsigned char> slots;
    int n = Line->Count;
    slots.assign((n > 0 ? n : 1), (unsigned char)CLR_Normal);

    State = (hlState)SubHighlightLine(g, Line->Chars, n, (int)State, slots.data());


    for (i = 0; i < Line->Count;) {
        Color = (ChColor)slots[i];
        IF_TAB()
        else {
            int j = 1;
            while (i + j < Line->Count && 
                   slots[i + j] == Color && 
                   Line->Chars[i + j] != '\t') {
                j++;
            }


            if (StateMap)
                memset(StateMap + i, State, j);

            int v_len = 0;
            for (int k = 0; k < j; k++) {
                if (((unsigned char)Line->Chars[i + k] & 0xC0) != 0x80) v_len++;
            }

            if (B) {
                MoveMem(B, C - Pos, Width, Line->Chars + i, HILIT_CLRD(), j);
            }

            C += v_len;
            i += j;
        }
    }

    if (ECol) *ECol = C;
    return 0;
}

#endif /* !SUB_NO_FTE */
