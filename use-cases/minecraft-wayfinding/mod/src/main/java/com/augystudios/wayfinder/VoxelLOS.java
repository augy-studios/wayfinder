package com.augystudios.wayfinder;

import java.util.function.Predicate;

/**
 * Java port of wayfind.c — Amanatides & Woo voxel traversal in 3-D.
 *
 * Visits every block a straight line passes through, in order, with zero
 * heap allocation (only primitives and a single re-used int[] per call).
 * Used by ThetaStar to answer "is there a clear line of sight from A to B?"
 */
public final class VoxelLOS {

    private VoxelLOS() {}

    /**
     * Walk a ray from {@code origin} along {@code dir} for up to {@code maxT} units.
     * Mirrors the hot-loop in wayfind.c exactly.
     *
     * @param origin 3-element double array [x, y, z]
     * @param dir    3-element direction (need not be normalised; maxT=1 stops at origin+dir)
     * @param maxT   stop when the ray parameter t exceeds this value
     * @param bounds grid extents [w, h, d]; cells outside are not visited
     * @param visitor called once per cell with an int[3]; return true to stop early
     * @return number of cells visited (including the cell that triggered early stop)
     */
    public static int walkRay(double[] origin, double[] dir, double maxT,
                               int[] bounds, Predicate<int[]> visitor) {
        int[] cell   = new int[3];
        int[] step   = new int[3];
        double[] tmax   = new double[3];
        double[] tdelta = new double[3];

        for (int i = 0; i < 3; i++) {
            double o = origin[i];
            double d = dir[i];
            cell[i] = (int) Math.floor(o);

            if (d > 0.0) {
                step[i]   =  1;
                tdelta[i] =  1.0 / d;
                tmax[i]   = ((cell[i] + 1) - o) / d;
            } else if (d < 0.0) {
                step[i]   = -1;
                tdelta[i] = -1.0 / d;
                tmax[i]   = (cell[i] - o) / d;
            } else {
                step[i]   = 0;
                tdelta[i] = Double.POSITIVE_INFINITY;
                tmax[i]   = Double.POSITIVE_INFINITY;
            }
        }

        for (int i = 0; i < 3; i++)
            if (cell[i] < 0 || cell[i] >= bounds[i]) return 0;

        int[] cur = {cell[0], cell[1], cell[2]};
        int count = 0;
        for (;;) {
            count++;
            if (visitor != null && visitor.test(cur)) return count;

            int a = 0;
            double best = tmax[0];
            if (tmax[1] < best) { best = tmax[1]; a = 1; }
            if (tmax[2] < best) { best = tmax[2]; a = 2; }

            if (best > maxT) return count;

            cell[a] += step[a];
            tmax[a] += tdelta[a];
            if (cell[a] < 0 || cell[a] >= bounds[a]) return count;

            cur[0] = cell[0]; cur[1] = cell[1]; cur[2] = cell[2];
        }
    }

    /**
     * Returns true if the straight line from (ax,ay,az) to (bx,by,bz) passes
     * through no solid block according to {@code isSolid}.
     * isSolid receives absolute world coordinates.
     */
    public static boolean hasLOS(int ax, int ay, int az,
                                  int bx, int by, int bz,
                                  Predicate<int[]> isSolid) {
        int ox = Math.min(ax, bx) - 1;
        int oy = Math.min(ay, by) - 1;
        int oz = Math.min(az, bz) - 1;
        int[] bounds = {
            Math.abs(bx - ax) + 3,
            Math.abs(by - ay) + 3,
            Math.abs(bz - az) + 3
        };

        double[] start = { ax - ox + 0.5, ay - oy + 0.5, az - oz + 0.5 };
        double[] end   = { bx - ox + 0.5, by - oy + 0.5, bz - oz + 0.5 };
        double[] dir   = { end[0]-start[0], end[1]-start[1], end[2]-start[2] };

        Predicate<int[]> localSolid = c ->
            isSolid.test(new int[]{ c[0]+ox, c[1]+oy, c[2]+oz });

        int full   = walkRay(start, dir, 1.0, bounds, null);
        int walked = walkRay(start, dir, 1.0, bounds, localSolid);
        return walked == full;
    }
}
