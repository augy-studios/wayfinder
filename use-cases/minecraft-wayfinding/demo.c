/* demo.c - Demonstrates Theta* pathfinding over a Minecraft-style 3D terrain.
 *
 * Builds a small world with a hill and a wall, then finds a path from (1,1,1)
 * to (14,1,14) and prints the waypoints. The path cuts through open air via
 * Theta*'s LOS shortcutting, showing far fewer waypoints than a raw A* path.
 *
 * Compile:
 *   gcc -O2 -o demo demo.c theta_star.c ../../wayfind.c -lm
 * Run:
 *   ./demo
 */
#include "theta_star.h"
#include <stdio.h>
#include <string.h>

#define W 16
#define H 16
#define D 16

static unsigned char world[W * H * D];

static int is_solid(int x, int y, int z, void *ctx)
{
    (void)ctx;
    if (x < 0 || y < 0 || z < 0 || x >= W || y >= H || z >= D) return 1;
    return world[z * H * W + y * W + x];
}

static void set_block(int x, int y, int z) {
    if (x >= 0 && y >= 0 && z >= 0 && x < W && y < H && z < D)
        world[z * H * W + y * W + x] = 1;
}

int main(void)
{
    memset(world, 0, sizeof(world));

    /* Solid ground at y=0 */
    for (int x = 0; x < W; ++x)
        for (int z = 0; z < D; ++z)
            set_block(x, 0, z);

    /* A wall running along z=8, leaving a gap at x=7..8 */
    for (int x = 0; x < W; ++x)
        for (int y = 1; y <= 4; ++y)
            if (x < 7 || x > 8)
                set_block(x, y, 8);

    /* A hill at (10-12, 1-3, 4-6) */
    for (int x = 10; x <= 12; ++x)
        for (int y = 1; y <= 3; ++y)
            for (int z = 4; z <= 6; ++z)
                set_block(x, y, z);

    ts_pos_t start = { 1,  1,  1 };
    ts_pos_t goal  = { 14, 1, 14 };

    printf("World: %dx%dx%d, path from (%d,%d,%d) to (%d,%d,%d)\n",
           W, H, D,
           start.x, start.y, start.z,
           goal.x,  goal.y,  goal.z);
    printf("Obstacles: wall at z=8 (gap x=7-8), hill at x=10-12 y=1-3 z=4-6\n\n");

    ts_path_t path = ts_find_path(start, goal, W, H, D, is_solid, NULL);

    if (path.count == 0) {
        printf("No path found.\n");
        return 1;
    }

    printf("Theta* found %d waypoints (Theta* skips intermediate collinear nodes):\n",
           path.count);
    for (int i = 0; i < path.count; ++i)
        printf("  [%2d]  (%d, %d, %d)\n", i,
               path.nodes[i].x, path.nodes[i].y, path.nodes[i].z);

    ts_path_free(&path);
    return 0;
}
