package com.augystudios.wayfinder;

import net.minecraft.block.BlockState;
import net.minecraft.util.math.BlockPos;
import net.minecraft.world.World;

import java.util.*;
import java.util.function.Predicate;

/**
 * Theta* pathfinding over Minecraft's 3-D block grid.
 *
 * Uses VoxelLOS (Java port of wayfinder) for line-of-sight shortcuts:
 * whenever a grandparent has clear LOS to the current neighbour, the
 * intermediate nodes are skipped. This produces smooth, diagonal paths
 * without the stair-stepping of plain A*.
 *
 * Search is bounded to a box around start+goal plus PADDING blocks of
 * slack on each side, capped at MAX_SIDE per axis.
 */
public final class ThetaStar {

    private ThetaStar() {}

    private static final int PADDING  = 8;
    private static final int MAX_SIDE = 64;  // per axis

    private static final int[][] DIRS = {
        // face neighbours
        {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1},
        // edge neighbours
        {1,1,0},{1,-1,0},{-1,1,0},{-1,-1,0},
        {1,0,1},{1,0,-1},{-1,0,1},{-1,0,-1},
        {0,1,1},{0,1,-1},{0,-1,1},{0,-1,-1},
        // corner neighbours
        {1,1,1},{1,1,-1},{1,-1,1},{1,-1,-1},
        {-1,1,1},{-1,1,-1},{-1,-1,1},{-1,-1,-1}
    };

    public record Result(List<BlockPos> path, String message) {}

    public static Result find(BlockPos start, BlockPos goal, World world) {
        if (start.equals(goal))
            return new Result(List.of(start), "Already there.");

        // Bounding box in local (0-based) coordinates
        int minX = Math.min(start.getX(), goal.getX()) - PADDING;
        int minY = Math.min(start.getY(), goal.getY()) - PADDING;
        int minZ = Math.min(start.getZ(), goal.getZ()) - PADDING;
        int w = Math.min(Math.abs(goal.getX()-start.getX()) + PADDING*2 + 1, MAX_SIDE);
        int h = Math.min(Math.abs(goal.getY()-start.getY()) + PADDING*2 + 1, MAX_SIDE);
        int d = Math.min(Math.abs(goal.getZ()-start.getZ()) + PADDING*2 + 1, MAX_SIDE);

        int sx = start.getX()-minX, sy = start.getY()-minY, sz = start.getZ()-minZ;
        int gx = goal.getX()-minX,  gy = goal.getY()-minY,  gz = goal.getZ()-minZ;

        if (!inBounds(sx,sy,sz,w,h,d) || !inBounds(gx,gy,gz,w,h,d))
            return new Result(Collections.emptyList(),
                "Goal is too far away or out of search bounds (max ~" + MAX_SIDE + " blocks).");

        int total = w * h * d;
        float[] g     = new float[total];
        int[]   par   = new int[total];
        byte[]  flags = new byte[total];   // bit 0 = open, bit 1 = closed
        Arrays.fill(g, Float.MAX_VALUE);
        Arrays.fill(par, -1);

        // Block solidity: a block is impassable if its collision shape is non-empty.
        Predicate<int[]> isSolid = abs -> {
            BlockPos bp = new BlockPos(abs[0], abs[1], abs[2]);
            BlockState bs = world.getBlockState(bp);
            return !bs.getCollisionShape(world, bp).isEmpty();
        };

        PriorityQueue<Integer> open =
            new PriorityQueue<>(Comparator.comparingDouble(i -> g[i] + octile(
                i%w, (i/w)%h, i/(w*h), gx, gy, gz)));

        int si = flat(sx,sy,sz,w,h);
        int gi = flat(gx,gy,gz,w,h);
        g[si] = 0;
        par[si] = si;
        flags[si] = 1;
        open.add(si);

        while (!open.isEmpty()) {
            int ci = open.poll();
            if (ci == gi) break;
            if ((flags[ci] & 2) != 0) continue;
            flags[ci] |= 2;

            int cx = ci%w, cy=(ci/w)%h, cz=ci/(w*h);

            for (int[] dv : DIRS) {
                int nx=cx+dv[0], ny=cy+dv[1], nz=cz+dv[2];
                if (!inBounds(nx,ny,nz,w,h,d)) continue;
                if (isSolid.test(new int[]{nx+minX, ny+minY, nz+minZ})) continue;
                int ni = flat(nx,ny,nz,w,h);
                if ((flags[ni] & 2) != 0) continue;

                // Theta*: try grandparent shortcut via LOS
                int pi = par[ci];
                int px=pi%w, py=(pi/w)%h, pz=pi/(w*h);

                float ng;
                int useParent;
                if (pi != ci && VoxelLOS.hasLOS(
                        px+minX, py+minY, pz+minZ,
                        nx+minX, ny+minY, nz+minZ,
                        isSolid)) {
                    double dx=nx-px, dy2=ny-py, dz2=nz-pz;
                    ng = g[pi] + (float)Math.sqrt(dx*dx + dy2*dy2 + dz2*dz2);
                    useParent = pi;
                } else {
                    ng = g[ci] + (float)Math.sqrt(dv[0]*dv[0]+dv[1]*dv[1]+dv[2]*dv[2]);
                    useParent = ci;
                }

                if (ng < g[ni]) {
                    g[ni] = ng;
                    par[ni] = useParent;
                    flags[ni] = 1;
                    open.add(ni);
                }
            }
        }

        if (g[gi] == Float.MAX_VALUE)
            return new Result(Collections.emptyList(), "No path found (area may be fully enclosed).");

        // Reconstruct path by following parent pointers
        List<BlockPos> path = new ArrayList<>();
        int i = gi;
        while (i != par[i]) {
            path.add(new BlockPos(i%w + minX, (i/w)%h + minY, i/(w*h) + minZ));
            i = par[i];
        }
        path.add(new BlockPos(i%w + minX, (i/w)%h + minY, i/(w*h) + minZ));
        Collections.reverse(path);
        return new Result(Collections.unmodifiableList(path),
            "Path found: " + path.size() + " waypoint(s).");
    }

    // --- helpers -------------------------------------------------------------

    private static int flat(int x, int y, int z, int w, int h) {
        return z*h*w + y*w + x;
    }

    private static boolean inBounds(int x, int y, int z, int w, int h, int d) {
        return x>=0 && y>=0 && z>=0 && x<w && y<h && z<d;
    }

    private static float octile(int ax, int ay, int az, int bx, int by, int bz) {
        double dx=Math.abs(ax-bx), dy=Math.abs(ay-by), dz=Math.abs(az-bz);
        // sort so s[0] <= s[1] <= s[2]
        double[] s = {dx, dy, dz};
        Arrays.sort(s);
        return (float)(s[2] + (Math.sqrt(2)-1)*s[1] + (Math.sqrt(3)-Math.sqrt(2))*s[0]);
    }
}
