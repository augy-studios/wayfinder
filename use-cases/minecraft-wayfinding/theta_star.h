/* theta_star.h - Theta* pathfinding over a 3D block grid using wayfinder
 * for line-of-sight checks.
 *
 * Theta* is any-angle pathfinding: when two nodes have a clear straight line
 * between them (tested via wayfinder's voxel traversal), the parent pointer
 * is updated to skip intermediate waypoints. This produces smooth, natural
 * paths without the axis-aligned stairstepping of plain A*.
 *
 * Coordinate convention: axes are (x, y, z) with y = vertical (Minecraft
 * convention). A "block" is a 1x1x1 cube; its cell index equals its floor
 * coordinate.
 */
#ifndef THETA_STAR_H
#define THETA_STAR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TS_MAX_DIM 128  /* max grid size per axis */

/* Opaque block map: caller supplies a function that returns 1 if a block
 * at (x,y,z) is solid (impassable), 0 if walkable. */
typedef int (*ts_solid_fn)(int x, int y, int z, void *ctx);

/* A 3-D integer position. */
typedef struct { int x, y, z; } ts_pos_t;

/* A waypoint list: the computed path from start -> goal, endpoints included.
 * Memory is malloc'd by ts_find_path; call ts_path_free when done. */
typedef struct {
    ts_pos_t *nodes;  /* array of waypoints */
    int        count; /* number of waypoints (0 = no path found) */
} ts_path_t;

/* Find a path in a bounding box [0,w) x [0,h) x [0,d).
 *   solid   - block query callback
 *   ctx     - passed through to solid
 *   w,h,d   - grid extents (max 128 each)
 * Returns an allocated ts_path_t. count == 0 means no path was found. */
ts_path_t ts_find_path(ts_pos_t start, ts_pos_t goal,
                       int w, int h, int d,
                       ts_solid_fn solid, void *ctx);

void ts_path_free(ts_path_t *p);

#ifdef __cplusplus
}
#endif
#endif /* THETA_STAR_H */
