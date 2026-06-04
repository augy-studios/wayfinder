/* theta_star.c - Theta* pathfinding using wayfinder for LOS.
 *
 * Algorithm overview:
 *  - A* over a 3D block grid (26-connected neighbours, octile heuristic).
 *  - Before relaxing an edge s -> neighbour, check LOS from s.parent -> neighbour
 *    (wayfinder voxel walk). If clear, use the grandparent instead — that is the
 *    Theta* "lazy" variant.
 *  - Result: paths cut corners and skip intermediate nodes whenever there is a
 *    clear sightline.
 *
 * Limitations of this demo:
 *  - Max grid 128^3 (about 2M nodes) — enough for a large Minecraft region.
 *  - No support for multi-block-tall entities yet (player is 1x1 for simplicity).
 *  - Open set is a binary min-heap (simple, fast enough for demo purposes).
 */
#include "theta_star.h"
#include "../../wayfind.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

/* ---------- helpers -------------------------------------------------------- */

static inline int idx3(int x, int y, int z, int w, int h)
{
    return z * h * w + y * w + x;
}

static inline double octile3(int ax, int ay, int az, int bx, int by, int bz)
{
    double dx = fabs((double)(ax - bx));
    double dy = fabs((double)(ay - by));
    double dz = fabs((double)(az - bz));
    /* sort dx >= dy >= dz */
    if (dx < dy) { double t = dx; dx = dy; dy = t; }
    if (dx < dz) { double t = dx; dx = dz; dz = t; }
    if (dy < dz) { double t = dy; dy = dz; dz = t; }
    return dx + (sqrt(2.0) - 1.0) * dy + (sqrt(3.0) - sqrt(2.0)) * dz;
}

/* ---------- node table ----------------------------------------------------- */

#define OPEN    1
#define CLOSED  2

typedef struct {
    float  g, f;      /* cost-so-far, f = g + h */
    int    parent;    /* flat index of parent node, -1 = none */
    uint8_t flags;
} ts_node_t;

/* ---------- binary min-heap (on f) ---------------------------------------- */

typedef struct {
    int  *data;
    int   size, cap;
} heap_t;

static void heap_push(heap_t *h, int v, const ts_node_t *nodes)
{
    if (h->size == h->cap) {
        h->cap = h->cap ? h->cap * 2 : 256;
        h->data = realloc(h->data, (size_t)h->cap * sizeof(int));
    }
    int i = h->size++;
    h->data[i] = v;
    while (i > 0) {
        int p = (i - 1) / 2;
        if (nodes[h->data[p]].f <= nodes[h->data[i]].f) break;
        int tmp = h->data[p]; h->data[p] = h->data[i]; h->data[i] = tmp;
        i = p;
    }
}

static int heap_pop(heap_t *h, const ts_node_t *nodes)
{
    int ret = h->data[0];
    h->data[0] = h->data[--h->size];
    int i = 0;
    for (;;) {
        int l = 2*i+1, r = 2*i+2, best = i;
        if (l < h->size && nodes[h->data[l]].f < nodes[h->data[best]].f) best = l;
        if (r < h->size && nodes[h->data[r]].f < nodes[h->data[best]].f) best = r;
        if (best == i) break;
        int tmp = h->data[i]; h->data[i] = h->data[best]; h->data[best] = tmp;
        i = best;
    }
    return ret;
}

/* ---------- LOS via wayfinder ---------------------------------------------- */

typedef struct { ts_solid_fn solid; void *ctx; } los_ctx_t;

static int los_visit(const int32_t *cell, int ndim, void *user)
{
    (void)ndim;
    los_ctx_t *lc = (los_ctx_t *)user;
    return lc->solid(cell[0], cell[1], cell[2], lc->ctx);
}

/* Returns 1 if there is an unobstructed straight line a -> b. */
static int has_los(ts_pos_t a, ts_pos_t b,
                   int w, int h, int d,
                   ts_solid_fn solid, void *ctx)
{
    wf_grid_t g = { .ndim = 3, .dims = { w, h, d } };
    int32_t fa[3] = { a.x, a.y, a.z };
    int32_t fb[3] = { b.x, b.y, b.z };
    los_ctx_t lc = { solid, ctx };
    /* wf_cells visits every voxel the line passes through; the visitor returns
     * non-zero on the first solid block, cutting the walk short. If any solid
     * block is hit, there is no LOS. We check by comparing visited count with
     * a full traversal. Use wf_segment so we can detect early termination. */
    double da[3] = { fa[0]+0.5, fa[1]+0.5, fa[2]+0.5 };
    double db[3] = { fb[0]+0.5, fb[1]+0.5, fb[2]+0.5 };
    /* Full count (no visitor): */
    size_t full = wf_collect(&g, da, db, NULL, 0);
    /* Walk with obstacle check: */
    size_t walked = wf_segment(&g, da, db, los_visit, &lc);
    return walked == full;  /* early stop means a wall was hit */
}

/* ---------- main pathfinder ------------------------------------------------ */

ts_path_t ts_find_path(ts_pos_t start, ts_pos_t goal,
                       int w, int h, int d,
                       ts_solid_fn solid, void *ctx)
{
    ts_path_t result = { NULL, 0 };
    int total = w * h * d;

    ts_node_t *nodes = calloc((size_t)total, sizeof(ts_node_t));
    if (!nodes) return result;
    for (int i = 0; i < total; ++i) { nodes[i].g = FLT_MAX; nodes[i].parent = -1; }

    heap_t open = { NULL, 0, 0 };

    int si = idx3(start.x, start.y, start.z, w, h);
    int gi = idx3(goal.x,  goal.y,  goal.z,  w, h);

    nodes[si].g      = 0.0f;
    nodes[si].f      = (float)octile3(start.x, start.y, start.z, goal.x, goal.y, goal.z);
    nodes[si].parent = si;
    nodes[si].flags  = OPEN;
    heap_push(&open, si, nodes);

    /* 26-connected neighbourhood offsets */
    static const int8_t DIRS[26][3] = {
        {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1},
        {1,1,0},{1,-1,0},{-1,1,0},{-1,-1,0},
        {1,0,1},{1,0,-1},{-1,0,1},{-1,0,-1},
        {0,1,1},{0,1,-1},{0,-1,1},{0,-1,-1},
        {1,1,1},{1,1,-1},{1,-1,1},{1,-1,-1},
        {-1,1,1},{-1,1,-1},{-1,-1,1},{-1,-1,-1}
    };

    while (open.size > 0) {
        int ci = heap_pop(&open, nodes);
        if (ci == gi) break;
        if (nodes[ci].flags & CLOSED) continue;
        nodes[ci].flags |= CLOSED;

        int cx = ci % w;
        int cy = (ci / w) % h;
        int cz = ci / (w * h);

        for (int di = 0; di < 26; ++di) {
            int nx = cx + DIRS[di][0];
            int ny = cy + DIRS[di][1];
            int nz = cz + DIRS[di][2];
            if (nx < 0 || ny < 0 || nz < 0 || nx >= w || ny >= h || nz >= d) continue;
            if (solid(nx, ny, nz, ctx)) continue;

            int ni = idx3(nx, ny, nz, w, h);
            if (nodes[ni].flags & CLOSED) continue;

            /* Theta*: try to link neighbour to grandparent instead of current */
            int pi = nodes[ci].parent;
            int px = pi % w, py = (pi / w) % h, pz = pi / (w * h);
            ts_pos_t ppos = { px, py, pz }, npos = { nx, ny, nz };

            float ng;
            int use_parent;
            if (pi != ci && has_los(ppos, npos, w, h, d, solid, ctx)) {
                /* link through grandparent */
                double dx = nx - px, dy_d = ny - py, dz = nz - pz;
                ng = nodes[pi].g + (float)sqrt(dx*dx + dy_d*dy_d + dz*dz);
                use_parent = pi;
            } else {
                /* normal A* edge */
                double dx = DIRS[di][0], dy_d = DIRS[di][1], dz2 = DIRS[di][2];
                ng = nodes[ci].g + (float)sqrt(dx*dx + dy_d*dy_d + dz2*dz2);
                use_parent = ci;
            }

            if (ng < nodes[ni].g) {
                nodes[ni].g      = ng;
                nodes[ni].f      = ng + (float)octile3(nx, ny, nz, goal.x, goal.y, goal.z);
                nodes[ni].parent = use_parent;
                nodes[ni].flags  = OPEN;
                heap_push(&open, ni, nodes);
            }
        }
    }

    /* Reconstruct path by walking parent pointers. */
    if (nodes[gi].g < FLT_MAX) {
        int len = 0;
        for (int i = gi; i != nodes[i].parent; i = nodes[i].parent) len++;
        len++; /* include start */

        result.nodes = malloc((size_t)len * sizeof(ts_pos_t));
        result.count = len;

        int i = gi, k = len - 1;
        while (k >= 0) {
            result.nodes[k].x = i % w;
            result.nodes[k].y = (i / w) % h;
            result.nodes[k].z = i / (w * h);
            int next = nodes[i].parent;
            if (next == i) { k--; break; }
            i = next;
            k--;
        }
    }

    free(open.data);
    free(nodes);
    return result;
}

void ts_path_free(ts_path_t *p)
{
    free(p->nodes);
    p->nodes = NULL;
    p->count = 0;
}
