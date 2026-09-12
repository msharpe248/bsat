/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Typed adaptation of the Bentley/McIlroy partition policy distributed by
 * Apple Libc, revision 71bbe350ab79eef58113991d817ccc6165061a64,
 * stdlib/FreeBSD/qsort.c. Struct swaps replace byte/aliasing tricks; a local
 * allocation-free heapsort handles depth exhaustion. Equal keys deliberately
 * retain this fixed algorithm's order, independent of the host C library. */
#include "../include/reduction_sort.h"

typedef struct {
    bool (*poll)(void *);
    void *state;
    bool stopped;
} SortBudget;

static bool
stopped(SortBudget *budget)
{
    if (!budget) return false;
    if (!budget->stopped) budget->stopped = budget->poll(budget->state);
    return budget->stopped;
}

static int
score_cmp(const ClauseScore *a, const ClauseScore *b)
{
    if (a->lbd != b->lbd) return a->lbd < b->lbd ? -1 : 1;
    if (a->activity > b->activity) return -1;
    if (a->activity < b->activity) return 1;
    return 0;
}

static void
swap(ClauseScore *a, ClauseScore *b)
{
    ClauseScore t = *a;

    *a = *b;
    *b = t;
}

static size_t
med3(ClauseScore *a, size_t x, size_t y, size_t z)
{
    return score_cmp(a + x, a + y) < 0
               ? (score_cmp(a + y, a + z) < 0 ? y : (score_cmp(a + x, a + z) < 0 ? z : x))
               : (score_cmp(a + y, a + z) > 0 ? y : (score_cmp(a + x, a + z) < 0 ? x : z));
}

static bool
insertion(ClauseScore *a, size_t n, size_t limit, SortBudget *budget)
{
    size_t swaps = 0;

    for (size_t i = 1; i < n; ++i) {
        if (stopped(budget)) return false;
        for (size_t j = i; j && score_cmp(a + j - 1, a + j) > 0; --j) {
            if (stopped(budget)) return false;
            swap(a + j, a + j - 1);
            if (limit && ++swaps > limit) return false;
        }
    }
    return true;
}

static void
sift(ClauseScore *a, size_t root, size_t n, SortBudget *budget)
{
    while (root < n / 2) {
        if (stopped(budget)) return;
        size_t child = 2 * root + 1;

        if (child + 1 < n && score_cmp(a + child, a + child + 1) < 0) ++child;
        if (score_cmp(a + root, a + child) >= 0) break;
        swap(a + root, a + child);
        root = child;
    }
}

static void
score_heapsort(ClauseScore *a, size_t n, SortBudget *budget)
{
    for (size_t i = n / 2; i && !stopped(budget); i--)
        sift(a, i - 1, n, budget);
    for (size_t end = n; end > 1 && !stopped(budget); --end) {
        swap(a, a + end - 1);
        sift(a, 0, end - 1, budget);
    }
}

static void
vecswap(ClauseScore *a, ClauseScore *b, size_t n, SortBudget *budget)
{
    for (size_t i = 0; i < n; ++i) {
        if (stopped(budget)) return;
        swap(a + i, b + i);
    }
}

static void
sort_depth(ClauseScore *a, size_t n, unsigned depth, SortBudget *budget)
{
    while (n > 1 && !stopped(budget)) {
        if (!depth) {
            score_heapsort(a, n, budget);
            return;
        }
        --depth;
        if (n <= 7) {
            (void)insertion(a, n, 0, budget);
            return;
        }
        size_t pl = 0, pm = n / 2, pn = n - 1;

        if (n > 40) {
            size_t d = n / 8;

            pl = med3(a, pl, pl + d, pl + 2 * d);
            pm = med3(a, pm - d, pm, pm + d);
            pn = med3(a, pn - 2 * d, pn - d, pn);
        }
        pm = med3(a, pl, pm, pn);
        swap(a, a + pm);
        size_t pa = 1, pb = 1, pc = n - 1, pd = n - 1;
        bool swapped = false;
        int cmp;

        for (;;) {
            while (pb <= pc && !stopped(budget) && (cmp = score_cmp(a + pb, a)) <= 0) {
                if (!cmp) {
                    swapped = true;
                    swap(a + pa, a + pb);
                    ++pa;
                }
                ++pb;
            }
            while (pb <= pc && !stopped(budget) && (cmp = score_cmp(a + pc, a)) >= 0) {
                if (!cmp) {
                    swapped = true;
                    swap(a + pc, a + pd);
                    --pd;
                }
                --pc;
            }
            if (budget && budget->stopped) return;
            if (pb > pc) break;
            swap(a + pb, a + pc);
            swapped = true;
            ++pb;
            --pc;
        }
        size_t d = pa < pb - pa ? pa : pb - pa;

        vecswap(a, a + pb - d, d, budget);
        d = pd - pc < n - pd - 1 ? pd - pc : n - pd - 1;
        vecswap(a + pb, a + n - d, d, budget);
        if (!swapped && insertion(a, n, 1 + n / 4, budget)) return;
        if (budget && budget->stopped) return;
        size_t left = pb - pa, right = pd - pc;

        if (left <= right) {
            if (left > 1) sort_depth(a, left, depth, budget);
            if (right <= 1) return;
            a += n - right;
            n = right;
        } else {
            if (right > 1) sort_depth(a + n - right, right, depth, budget);
            if (left <= 1) return;
            n = left;
        }
    }
}

void
bsat_sort_clause_scores_depth(ClauseScore *a, size_t n, unsigned depth)
{
    sort_depth(a, n, depth, NULL);
}

void
bsat_sort_clause_scores(ClauseScore *a, size_t n)
{
    unsigned depth = 0;

    for (size_t k = n; k > 1; k >>= 1)
        ++depth;
    bsat_sort_clause_scores_depth(a, n, 2 * depth);
}

bool
bsat_sort_clause_scores_bounded(ClauseScore *a, size_t n, bool (*poll)(void *), void *state)
{
    unsigned depth = 0;
    SortBudget budget = {poll, state, false};

    for (size_t k = n; k > 1; k >>= 1)
        ++depth;
    sort_depth(a, n, 2 * depth, poll ? &budget : NULL);
    return !budget.stopped;
}
