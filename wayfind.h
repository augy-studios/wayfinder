/* wayfind.h - N-dimensional grid line traversal (generalized DDA).
 *
 * A deterministic, allocation-free "wayfinder": given two points in an
 * N-dimensional grid, it walks every cell the straight line between them
 * passes through, in order. This is the Amanatides & Woo voxel-traversal
 * algorithm generalized to arbitrary dimensions.
 *
 * It is NOT a graph search. It does not explore alternatives, so it has no
 * open/closed sets, no priority queue, and no heap allocation. That is
 * exactly why it runs orders of magnitude faster than A* for the straight-
 * line / line-of-sight problem: cost is O(cells crossed), nothing more.
 *
 * Portability: the whole thing is integer + double arithmetic with no OS,
 * library, or compiler-specific tricks in the algorithm itself. Porting the
 * core to Rust/Go/Java/C#/JS/Python is a near-mechanical translation.
 *
 * License: MIT. Augy Studios / augy-studios.
 */
#ifndef WAYFIND_H
#define WAYFIND_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum number of dimensions supported at compile time.
 * Bump this if you need more axes; it only grows a few stack arrays. */
#ifndef WF_MAX_DIMS
#define WF_MAX_DIMS 8
#endif

/* A grid is just its shape: how many cells along each axis.
 * Cell indices on axis i are valid in [0, dims[i]). */
typedef struct {
    int     ndim;                  /* number of active dimensions (<= WF_MAX_DIMS) */
    int32_t dims[WF_MAX_DIMS];     /* cell count per axis */
} wf_grid_t;

/* Visitor callback. Called once per cell, in traversal order.
 *   cell  - integer coordinates of the current cell (length == ndim)
 *   ndim  - number of dimensions
 *   user  - opaque pointer you passed in
 * Return 0 to keep going, non-zero to stop early (e.g. you hit a wall).
 * This is the mechanism for line-of-sight / "stop at first obstacle". */
typedef int (*wf_visitor_fn)(const int32_t *cell, int ndim, void *user);

/* --- Core ---------------------------------------------------------------- */

/* Traverse a ray from `origin` along `dir` for up to `max_t` units of `dir`.
 * `dir` need not be normalized; max_t is measured in multiples of its length
 * (so max_t = 1.0 with dir = target - origin stops at the target).
 * Returns the number of cells visited. Zero heap allocation. */
size_t wf_ray(const wf_grid_t *g,
              const double *origin,
              const double *dir,
              double max_t,
              wf_visitor_fn visit,
              void *user);

/* --- Convenience wrappers ------------------------------------------------ */

/* Walk the line segment between two continuous points a -> b (inclusive of
 * the cell containing b). Equivalent to wf_ray(origin=a, dir=b-a, max_t=1). */
size_t wf_segment(const wf_grid_t *g,
                  const double *a,
                  const double *b,
                  wf_visitor_fn visit,
                  void *user);

/* Walk from the center of integer cell `from` to the center of cell `to`. */
size_t wf_cells(const wf_grid_t *g,
                const int32_t *from,
                const int32_t *to,
                wf_visitor_fn visit,
                void *user);

/* Collect the cells of segment a -> b into the caller-owned buffer
 * `out_cells`, laid out flat as [c0_d0, c0_d1, ..., c1_d0, ...].
 * Writes at most `cap` cells. The RETURN value is the true number of cells
 * the line crosses, which may exceed `cap` (only the first `cap` are stored).
 * Zero heap allocation: the buffer is yours. */
size_t wf_collect(const wf_grid_t *g,
                  const double *a,
                  const double *b,
                  int32_t *out_cells,
                  size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* WAYFIND_H */