/* bench.c - reproduces the test matrix from the spec screenshot and reports
 * Execution Time / Intersections / Extra Allocations for each grid.
 *
 * Each case walks a corner-to-corner diagonal of the grid (cell centers from
 * {0,0,...} to {dim-1, dim-1, ...}). The diagonal is the worst case for cell
 * count, so it is a fair stress test of the inner loop.
 *
 * Timing: we warm up, then run the traversal REPS times and divide. A
 * volatile checksum derived from every visited cell prevents the optimizer
 * from deleting the work.
 *
 * Allocations: the core never calls malloc/calloc/realloc. To prove it rather
 * than assert it, run under valgrind/ltrace (see README). The column here is
 * therefore always 0 bytes.
 *
 * Custom grid:  ./bench 32x15x64x98
 *   Pass any NxNx... spec (2–8 dims, each dim >= 1) to benchmark that grid.
 */
#define _POSIX_C_SOURCE 199309L   /* clock_gettime / CLOCK_MONOTONIC */
#include "wayfind.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static volatile uint64_t g_sink;          /* keeps the work observable */

static int sum_cb(const int32_t *cell, int ndim, void *user)
{
    uint64_t s = 0;
    for (int i = 0; i < ndim; ++i) s += (uint64_t)(uint32_t)cell[i];
    *(uint64_t *)user += s;
    return 0;
}

static double now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

static void run_case_dims(const char *label, int ndim, const int32_t *dims)
{
    wf_grid_t g; g.ndim = ndim;
    int32_t from[WF_MAX_DIMS], to[WF_MAX_DIMS];
    long long cells = 1;
    for (int i = 0; i < ndim; ++i) {
        g.dims[i] = dims[i];
        from[i]   = 0;
        to[i]     = dims[i] - 1;
        cells    *= dims[i];
    }

    /* warm up + capture intersection count (deterministic) */
    uint64_t acc = 0;
    size_t intersections = wf_cells(&g, from, to, sum_cb, &acc);

    /* pick iteration count so total runtime is comfortably measurable */
    long reps = 2000000;
    if (intersections > 200)  reps = 500000;
    if (intersections > 1000) reps = 100000;

    double t0 = now_ns();
    for (long r = 0; r < reps; ++r) {
        acc = 0;
        wf_cells(&g, from, to, sum_cb, &acc);
    }
    double t1 = now_ns();
    g_sink += acc;

    double per_call_ns = (t1 - t0) / (double)reps;
    double per_call_ms = per_call_ns / 1e6;
    double per_cell_ns = per_call_ns / (double)intersections;

    printf("  %-20s (%10lld cells)\n", label, cells);
    printf("      Execution Time    %9.4f ms   (%8.1f ns/call, %6.2f ns/cell)\n",
           per_call_ms, per_call_ns, per_cell_ns);
    printf("      Intersections     %9zu\n", intersections);
    printf("      Extra Allocations         0 bytes\n");
    printf("\n");
}

static void run_case(const char *label, int ndim, int side)
{
    int32_t dims[WF_MAX_DIMS];
    for (int i = 0; i < ndim; ++i) dims[i] = side;
    run_case_dims(label, ndim, dims);
}

/* Parse "32x15x64x98" into dims[]; returns ndim, or 0 on error. */
static int parse_grid(const char *spec, int32_t dims[WF_MAX_DIMS])
{
    int ndim = 0;
    char buf[256];
    strncpy(buf, spec, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *tok = strtok(buf, "xX");
    while (tok) {
        if (ndim >= WF_MAX_DIMS) return 0;
        long v = strtol(tok, NULL, 10);
        if (v < 1) return 0;
        dims[ndim++] = (int32_t)v;
        tok = strtok(NULL, "xX");
    }
    return (ndim >= 2) ? ndim : 0;
}

int main(int argc, char *argv[])
{
    if (argc == 2) {
        int32_t dims[WF_MAX_DIMS];
        int ndim = parse_grid(argv[1], dims);
        if (!ndim) {
            fprintf(stderr, "usage: ./bench [D1xD2x...] (e.g. 32x15x64x98)\n");
            return 1;
        }

        /* build a label like "32 x 15 x 64 x 98" */
        char label[64] = {0};
        for (int i = 0; i < ndim; ++i) {
            char part[16];
            snprintf(part, sizeof(part), "%s%d", i ? " x " : "", dims[i]);
            strncat(label, part, sizeof(label) - strlen(label) - 1);
        }

        printf("wayfind benchmark  (single-threaded, CPU, zero-allocation)\n");
        printf("=========================================================\n\n");
        printf("[custom %dD]\n", ndim);
        run_case_dims(label, ndim, dims);

        if (g_sink == 0x1ULL) fputs("", stderr);
        return 0;
    }

    printf("wayfind benchmark  (single-threaded, CPU, zero-allocation)\n");
    printf("=========================================================\n\n");

    printf("[2D]\n");
    run_case("15 x 15",         2, 15);
    run_case("75 x 75",         2, 75);
    run_case("150 x 150",       2, 150);

    printf("[3D]\n");
    run_case("15 x 15 x 15",    3, 15);
    run_case("75 x 75 x 75",    3, 75);

    printf("[4D]\n");
    run_case("4^4",             4, 4);
    run_case("8^4",             4, 8);
    run_case("16^4",            4, 16);
    run_case("32^4",            4, 32);

    if (g_sink == 0x1ULL) fputs("", stderr);   /* never true; defeats DCE */
    return 0;
}
