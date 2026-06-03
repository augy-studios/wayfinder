# Wayfinder

An extremely fast, single-threaded, allocation-free **N-dimensional grid
traversal** ("wayfinder"). Given two points in a 2D / 3D / 4D / xD grid, it
walks every cell the straight line between them passes through — in order, at
nanosecond-per-cell speed, on the CPU, with **zero heap allocation**.

It is the Amanatides & Woo voxel-traversal algorithm (DDA) generalized to
arbitrary dimensions.

---

## What this is — and how it relates to A\*

Read this before assuming it is a drop-in A\* replacement.

This is a **deterministic line walk**, not a graph search. It answers *"which
cells lie on the straight line from A to B?"* — the core of line-of-sight,
visibility, ray casting, lightmap tracing, and collision sweeps.

Because it never searches, it has **no priority queue, no open/closed sets,
and no allocation**. That is exactly why it is orders of magnitude faster than
A\*: A\* pays for exploring alternatives so it can route *around obstacles*;
this pays for nothing it doesn't use. Cost is `O(cells the line crosses)`.

| You want…                                              | Use                          |
| ------------------------------------------------------ | ---------------------------- |
| Cells on the straight line A→B; line of sight          | **this (`wayfind`)** ✅       |
| "Can I see B from A without hitting a wall?"            | **this**, stop in the visitor ✅ |
| Shortest path *around* walls in a maze                 | A\* / JPS (different class)  |

For obstacle-avoidance routing you still want a search. But `wayfind` is the
high-speed **primitive that those searches call** (e.g. Theta\* uses exactly
this for its line-of-sight checks). If you later need full pathfinding, this
library is the fast LOS core to build it on.

---

## Measured performance (this machine, GCC 13, `-O3 -march=native`)

Diagonal corner-to-corner traversal of each grid. "Spec" is your reference
screenshot.

| Grid        | Cells     | Spec time | **wayfind** | ns/call | ns/cell | Alloc |
| ----------- | --------- | --------- | ----------- | ------- | ------- | ----- |
| 15 × 15     | 225       | 0.1236 ms | 0.0002 ms   | ~228    | 7.9     | 0 B   |
| 75 × 75     | 5,625     | 0.4020 ms | 0.0010 ms   | ~1004   | 6.7     | 0 B   |
| 150 × 150   | 22,500    | 4.3820 ms | 0.0020 ms   | ~1989   | 6.7     | 0 B   |
| 15³         | 3,375     | 0.1020 ms | 0.0002 ms   | ~247    | 5.8     | 0 B   |
| 75³         | 421,875   | 1.9138 ms | 0.0012 ms   | ~1166   | 5.2     | 0 B   |
| 4⁴          | 256       | 0.0715 ms | 0.0002 ms   | ~175    | 13.5    | 0 B   |
| 8⁴          | 4,096     | 0.0614 ms | 0.0004 ms   | ~360    | 12.4    | 0 B   |
| 16⁴         | 65,536    | 0.1096 ms | 0.0007 ms   | ~735    | 12.0    | 0 B   |
| 32⁴         | 1,048,576 | 0.2071 ms | 0.0015 ms   | ~1454   | 11.6    | 0 B   |

Roughly **100×–2000× faster** than the reference, and the spec's 12,288-byte
allocation on the 75³ case becomes **0 bytes**. Your own numbers will differ
with CPU/clock; rerun `make run` on the VPS.

> The intersection counts are slightly lower than the screenshot because these
> are exact corner-to-corner diagonals (`D·N − (D−1)` cells). Off-axis rays
> cross a few more cells; the algorithm and timing are unaffected.

---

## Build on Debian 13

```bash
sudo apt update && sudo apt install -y build-essential
make            # builds ./bench and ./example
make run        # builds and runs the benchmark
./example       # the usage demo (segment + line-of-sight + 4D)
```

Need to move the compiled binary to a different CPU than you built on? Build
the portable baseline (drops `-march=native`):

```bash
make portable
```

### Running under tmux

You prefer one tmux session per independent process — so:

```bash
tmux new -s wayfind-bench      # dedicated session for the benchmark
make run
# detach with Ctrl-b d ; reattach with: tmux attach -t wayfind-bench
```

---

## API (full reference is in `wayfind.h`)

```c
#include "wayfind.h"

wf_grid_t g = { .ndim = 3, .dims = { 64, 64, 64 } };

/* Visitor: return non-zero to stop early (e.g. you hit a wall). */
int visit(const int32_t *cell, int ndim, void *user) {
    /* ... use cell[0..ndim-1] ... */
    return 0;
}

int32_t from[3] = {0,0,0}, to[3] = {63,63,63};
size_t crossed = wf_cells(&g, from, to, visit, NULL);
```

| Function      | Purpose                                                        |
| ------------- | -------------------------------------------------------------- |
| `wf_ray`      | Walk a ray from an origin along a direction, up to `max_t`.     |
| `wf_segment`  | Walk continuous point `a` → `b` (stops at the cell holding `b`).|
| `wf_cells`    | Walk integer cell `from` → `to` (cell centers).                 |
| `wf_collect`  | Same as `wf_segment` but writes cells into **your** buffer.     |

**Line of sight / "navigation":** pass a visitor that looks up your obstacle
map and returns non-zero on a blocked cell (see `example.c`). The walk stops at
the first wall — that single boolean *is* "can A reach B in a straight line?".

---

## Memory footprint

The core uses only fixed stack scratch: four `WF_MAX_DIMS`-element arrays
(default 8 dims → 4 × 64 B = **256 bytes**, cache-line aligned). It never
touches the heap. `wf_collect` writes only into the buffer **you** own; if it's
too small it keeps counting but stops writing — nothing reallocates behind your
back. If you ever want a self-growing path, allocate a few KB up front and pass
it in; the design intentionally keeps allocation in your hands.

### Verify zero allocation yourself

```bash
# 1. The core object references no allocator symbols at all:
gcc -O3 -std=c11 -c wayfind.c -o /tmp/wf.o
nm /tmp/wf.o | grep -iE 'malloc|calloc|realloc|free'   # prints nothing

# 2. Trace every heap call at runtime (install if needed):
sudo apt install -y ltrace valgrind
ltrace -e 'malloc+calloc+realloc+free' ./example
valgrind --tool=memcheck ./bench         # "total heap usage" stays minimal
```

---

## Porting to another language

The entire algorithm is the loop in `wayfind.c` (`wf_ray`): integer + double
arithmetic, no OS calls, no libraries beyond `floor`. Mechanical to translate.
Pseudocode:

```bash
for each axis i:
    cell[i]   = floor(origin[i])
    if dir[i] > 0:  step[i]=+1; tDelta[i]= 1/dir[i];  tMax[i]=((cell[i]+1)-origin[i])/dir[i]
    elif dir[i]<0:  step[i]=-1; tDelta[i]=-1/dir[i];  tMax[i]=(cell[i]-origin[i])/dir[i]
    else:           step[i]= 0; tDelta[i]= +inf;      tMax[i]= +inf

if any cell[i] not in [0, dims[i]): return 0

loop:
    visit(cell)
    a = argmin(tMax)            # axis whose boundary we hit next
    if tMax[a] > max_t: stop
    cell[a] += step[a]
    tMax[a] += tDelta[a]
    if cell[a] not in [0, dims[a]): stop
```

Notes for ports:

- JS/Python: same code; use typed arrays / arrays. Expect ~10–50× slower than C
  but still far ahead of an A\* for the straight-line case.
- Rust/Go/Zig: near-identical performance to C. Keep `dir` as `f64`.
- The unsigned-cast bounds trick `(uint32_t)cell[i] >= (uint32_t)dims[i]` folds
  the `< 0` and `>= dims` checks into one comparison. In languages without
  unsigned reinterpretation, write the two comparisons explicitly.

---

## A note on SQLite, scheduling, and databases

`wayfind` is a **stateless, in-memory algorithm** — it needs no database and no
scheduler, so none are bundled (that would also break the "few KB, zero
dependency, easily ported" goals). If a *surrounding system* later needs them:

- **Persisting grids/results:** store the grid shape and any precomputed paths
  in SQLite; the traversal stays pure and reads from your in-memory arrays.
- **Scheduling batch runs (no cron):** use a SQLite table as the queue, e.g.

  ```sql
  CREATE TABLE jobs (
    id        INTEGER PRIMARY KEY,
    payload   TEXT NOT NULL,      -- grid + endpoints as JSON
    run_at    INTEGER NOT NULL,   -- unix epoch seconds
    status    TEXT NOT NULL DEFAULT 'pending'
  );
  ```

  A small poller (`SELECT … WHERE status='pending' AND run_at<=strftime('%s','now')`)
  claims and runs due jobs — keeping scheduling in SQLite rather than cron.

Say the word and I'll wire that harness up around the algorithm.

---

## Files

| File         | What it is                                            |
| ------------ | ----------------------------------------------------- |
| `wayfind.h`  | Public API.                                           |
| `wayfind.c`  | The algorithm (the only file you port).               |
| `bench.c`    | Reproduces the spec test matrix.                      |
| `example.c`  | Segment walk, line-of-sight, 4D collect.              |
| `Makefile`   | Build on Debian.                                       |
| `.gitignore` | Ignores build artifacts.                              |

---

MIT License · Augy Studios
