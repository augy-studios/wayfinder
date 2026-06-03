/* example.c - the smallest useful demonstration of the API.
 *
 * 1. Walk a 2D segment and print the cells.
 * 2. Line-of-sight: stop the walk at the first blocked cell (this is how you
 *    use a pure traversal as a navigation/visibility primitive).
 * 3. Walk a 4D segment.
 */
#include "wayfind.h"
#include <stdio.h>

/* --- 1. print every cell ------------------------------------------------- */
static int print_cb(const int32_t *cell, int ndim, void *user)
{
    (void)user;
    printf("    (");
    for (int i = 0; i < ndim; ++i)
        printf("%d%s", cell[i], i + 1 < ndim ? ", " : "");
    printf(")\n");
    return 0;
}

/* --- 2. line of sight: a tiny obstacle map -------------------------------- */
typedef struct { const unsigned char *blocked; int w; } los_t;

static int los_cb(const int32_t *cell, int ndim, void *user)
{
    (void)ndim;
    los_t *m = (los_t *)user;
    int idx = cell[1] * m->w + cell[0];
    if (m->blocked[idx]) {
        printf("    blocked at (%d, %d) -- no line of sight\n", cell[0], cell[1]);
        return 1;                          /* non-zero: stop the walk */
    }
    return 0;
}

int main(void)
{
    /* 1. 2D segment ------------------------------------------------------- */
    wf_grid_t g2 = { .ndim = 2, .dims = { 10, 10 } };
    int32_t a[] = { 1, 1 }, b[] = { 8, 5 };
    printf("Segment (1,1) -> (8,5) on a 10x10 grid:\n");
    wf_cells(&g2, a, b, print_cb, NULL);

    /* 2. line of sight with a wall column at x = 4 ------------------------ */
    unsigned char map[100] = {0};
    for (int y = 0; y < 10; ++y) map[y * 10 + 4] = 1;   /* wall down x=4 */
    los_t m = { map, 10 };
    printf("\nLine of sight (1,1) -> (8,5) through a wall at x=4:\n");
    wf_cells(&g2, a, b, los_cb, &m);

    /* 3. 4D segment, collected into a fixed buffer (zero heap) ------------ */
    wf_grid_t g4 = { .ndim = 4, .dims = { 8, 8, 8, 8 } };
    int32_t f4[] = { 0, 0, 0, 0 }, t4[] = { 7, 7, 7, 7 };
    double da[4], db[4];
    for (int i = 0; i < 4; ++i) { da[i] = f4[i] + 0.5; db[i] = t4[i] + 0.5; }

    int32_t buf[64 * 4];                    /* room for 64 cells x 4 dims */
    size_t total = wf_collect(&g4, da, db, buf, 64);
    printf("\n4D diagonal (8^4) crosses %zu cells. First three:\n", total);
    for (size_t k = 0; k < 3 && k < total; ++k) {
        printf("    (%d, %d, %d, %d)\n",
               buf[k*4+0], buf[k*4+1], buf[k*4+2], buf[k*4+3]);
    }
    return 0;
}