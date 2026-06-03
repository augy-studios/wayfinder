/* wayfind.c - implementation. See wayfind.h for the contract.
 *
 * The hot loop is the entire algorithm. Read this once and you can port it
 * to any language in an afternoon:
 *
 *   for each axis i:
 *       cell[i]   = floor(origin[i])         // starting cell
 *       step[i]   = sign(dir[i])             // +1 / -1 / 0
 *       tDelta[i] = 1 / |dir[i]|             // ray distance to cross one cell
 *       tMax[i]   = distance to first cell boundary on axis i
 *   loop:
 *       visit(cell)
 *       a = axis with the smallest tMax     // next boundary we hit
 *       if tMax[a] > max_t: stop
 *       cell[a]  += step[a]                  // cross into the next cell
 *       tMax[a]  += tDelta[a]
 *       if cell[a] out of bounds: stop
 *
 * No allocation, no recursion, no branches in the body beyond the argmin and
 * one bounds check. Everything lives on the stack and in registers.
 */
#include "wayfind.h"
#include <math.h>

/* INFINITY is in math.h. We use it as "this axis never crosses a boundary"
 * for components of dir that are exactly zero. */

size_t wf_ray(const wf_grid_t *restrict g,
              const double *restrict origin,
              const double *restrict dir,
              double max_t,
              wf_visitor_fn visit,
              void *restrict user)
{
    const int n = g->ndim;

    /* Cache-line aligned scratch. For ndim <= 8 these are 64 bytes each and
     * sit entirely in one or two cache lines; the compiler keeps them hot. */
    _Alignas(64) int32_t cell[WF_MAX_DIMS];
    _Alignas(64) int32_t step[WF_MAX_DIMS];
    _Alignas(64) double  tmax[WF_MAX_DIMS];
    _Alignas(64) double  tdelta[WF_MAX_DIMS];

    for (int i = 0; i < n; ++i) {
        const double o = origin[i];
        const double d = dir[i];
        const int32_t c = (int32_t)floor(o);
        cell[i] = c;

        if (d > 0.0) {
            step[i]   = 1;
            tdelta[i] = 1.0 / d;
            tmax[i]   = ((double)(c + 1) - o) / d;
        } else if (d < 0.0) {
            step[i]   = -1;
            tdelta[i] = -1.0 / d;
            tmax[i]   = ((double)c - o) / d;
        } else {
            step[i]   = 0;
            tdelta[i] = INFINITY;
            tmax[i]   = INFINITY;
        }
    }

    /* If we don't even start inside the grid, there is nothing to visit.
     * The cast to unsigned folds the "< 0" and ">= dims" tests into one. */
    for (int i = 0; i < n; ++i)
        if ((uint32_t)cell[i] >= (uint32_t)g->dims[i])
            return 0;

    size_t count = 0;
    for (;;) {
        ++count;
        if (visit && visit(cell, n, user))
            return count;                 /* caller asked to stop (e.g. wall) */

        /* Which axis boundary do we cross next? Smallest tMax wins. */
        int    a    = 0;
        double best = tmax[0];
        for (int i = 1; i < n; ++i) {
            if (tmax[i] < best) { best = tmax[i]; a = i; }
        }

        if (best > max_t)
            return count;                 /* reached the end of the segment */

        cell[a] += step[a];
        tmax[a] += tdelta[a];

        /* Only the axis we stepped can have left the grid. */
        if ((uint32_t)cell[a] >= (uint32_t)g->dims[a])
            return count;
    }
}

size_t wf_segment(const wf_grid_t *g,
                  const double *a,
                  const double *b,
                  wf_visitor_fn visit,
                  void *user)
{
    double dir[WF_MAX_DIMS];
    for (int i = 0; i < g->ndim; ++i)
        dir[i] = b[i] - a[i];
    return wf_ray(g, a, dir, 1.0, visit, user);
}

size_t wf_cells(const wf_grid_t *g,
                const int32_t *from,
                const int32_t *to,
                wf_visitor_fn visit,
                void *user)
{
    double a[WF_MAX_DIMS], b[WF_MAX_DIMS];
    for (int i = 0; i < g->ndim; ++i) {
        a[i] = (double)from[i] + 0.5;     /* cell centers */
        b[i] = (double)to[i]   + 0.5;
    }
    return wf_segment(g, a, b, visit, user);
}

/* --- wf_collect: gather cells into a flat caller buffer, no heap --------- */

typedef struct {
    int32_t *out;
    size_t   cap;
    size_t   written;
    int      ndim;
} wf_collector_t;

static int wf_collect_cb(const int32_t *cell, int ndim, void *user)
{
    wf_collector_t *c = (wf_collector_t *)user;
    if (c->written < c->cap) {
        int32_t *dst = c->out + (size_t)c->written * (size_t)ndim;
        for (int i = 0; i < ndim; ++i)
            dst[i] = cell[i];
    }
    c->written++;
    return 0;                              /* never stop early */
}

size_t wf_collect(const wf_grid_t *g,
                  const double *a,
                  const double *b,
                  int32_t *out_cells,
                  size_t cap)
{
    wf_collector_t c = { out_cells, cap, 0, g->ndim };
    return wf_segment(g, a, b, wf_collect_cb, &c);
}