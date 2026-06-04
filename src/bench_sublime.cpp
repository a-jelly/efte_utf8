/*
 * bench_sublime.cpp — Benchmark for the sublime-syntax highlighting engine.
 *
 * Build:  g++ -O2 -DSUB_NO_FTE -o bench_sublime bench_sublime.cpp h_sublime.cpp -lyaml -lpcre2-8
 * Usage:  bench_sublime <grammar.sublime-syntax> <source-file> [iterations]
 *
 * Reports: total time, lines/sec, µs/line, and state count.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>
#include <string>
#include "h_sublime.h"

static double now_sec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s grammar source [iterations]\n", argv[0]);
        return 2;
    }
    int iters = (argc > 3) ? atoi(argv[3]) : 10;
    if (iters < 1) iters = 1;

    /* Load grammar */
    char err[256] = {0};
    double t0 = now_sec();
    SubGrammar *g = SubLoadGrammar(argv[1], err, sizeof(err));
    double t_load = now_sec() - t0;
    if (!g) { fprintf(stderr, "load failed: %s\n", err); return 1; }
    fprintf(stderr, "Grammar loaded in %.1f ms\n", t_load * 1000);

    /* Read source file into lines */
    FILE *f = fopen(argv[2], "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", argv[2]); return 1; }
    std::vector<std::string> lines;
    char buf[16384];
    while (fgets(buf, sizeof(buf), f)) {
        int len = strlen(buf);
        while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) len--;
        lines.push_back(std::string(buf, len));
    }
    fclose(f);
    int nlines = (int)lines.size();
    fprintf(stderr, "Source: %d lines\n", nlines);

    /* Allocate slot buffer (max line length) */
    size_t maxlen = 0;
    for (int i = 0; i < nlines; i++)
        if (lines[i].size() > maxlen) maxlen = lines[i].size();
    std::vector<unsigned char> slot(maxlen > 0 ? maxlen : 1);

    /* Warm up (1 pass) */
    {
        int state = 0;
        for (int i = 0; i < nlines; i++) {
            int len = (int)lines[i].size();
            slot.resize(len > 0 ? len : 1);
            state = SubHighlightLine(g, lines[i].c_str(), len, state, slot.data());
        }
    }

    /* Benchmark */
    double best = 1e30;
    double total = 0;
    for (int it = 0; it < iters; it++) {
        int state = 0;
        double t1 = now_sec();
        for (int i = 0; i < nlines; i++) {
            int len = (int)lines[i].size();
            slot.resize(len > 0 ? len : 1);
            state = SubHighlightLine(g, lines[i].c_str(), len, state, slot.data());
        }
        double elapsed = now_sec() - t1;
        total += elapsed;
        if (elapsed < best) best = elapsed;
    }

    double avg = total / iters;
    double lines_per_sec = nlines / avg;
    double us_per_line = avg / nlines * 1e6;

    fprintf(stderr, "\n=== Results (%d iterations, %d lines) ===\n", iters, nlines);
    fprintf(stderr, "  Best pass:    %7.1f ms  (%7.0f lines/sec, %5.1f µs/line)\n",
            best * 1000, nlines / best, best / nlines * 1e6);
    fprintf(stderr, "  Average pass: %7.1f ms  (%7.0f lines/sec, %5.1f µs/line)\n",
            avg * 1000, lines_per_sec, us_per_line);
    fprintf(stderr, "  Grammar load: %7.1f ms\n", t_load * 1000);

    SubFreeGrammar(g);
    return 0;
}
