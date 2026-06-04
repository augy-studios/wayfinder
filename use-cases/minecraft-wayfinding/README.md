# Minecraft Wayfinding — Theta* over a 3D Block Grid

## What this is

Wayfinder's core is the Amanatides & Woo voxel traversal algorithm, which was
invented for ray-casting in voxel grids. Minecraft's world is exactly that — a
3D block grid — so wayfinder applies directly.

This use-case builds on wayfinder in two layers:

```
┌────────────────────────────────────────┐
│  Minecraft world (3D block grid)       │
├────────────────────────────────────────┤
│  Theta* (ThetaStar.java / theta_star.c)│  ← pathfinding search
│    calls VoxelLOS for LOS shortcuts    │
├────────────────────────────────────────┤
│  wayfinder  (VoxelLOS.java / wayfind.c)│  ← line-of-sight primitive
└────────────────────────────────────────┘
```

**Theta\*** is an any-angle pathfinder: it uses wayfinder's LOS check to
shortcut straight through open space, producing smooth diagonal paths instead
of the axis-aligned staircase you get from plain A*. The demo output shows
4 waypoints through a world that would give plain A* 20+.

---

## Part 1 — Run the C demo on your Debian VPS

This tests the raw algorithm without Minecraft. Takes about 30 seconds.

```bash
# 1. Install build tools (one time)
sudo apt update && sudo apt install -y build-essential

# 2. Clone / enter the repo
cd use-cases/minecraft-wayfinding

# 3. Build and run
make
./demo
```

Expected output:
```
World: 16x16x16, path from (1,1,1) to (14,1,14)
Obstacles: wall at z=8 (gap x=7-8), hill at x=10-12 y=1-3 z=4-6

Theta* found 4 waypoints (Theta* skips intermediate collinear nodes):
  [ 0]  (1, 1, 1)
  [ 1]  (8, 1, 8)
  [ 2]  (9, 1, 9)
  [ 3]  (14, 1, 14)
```

The path squeezes through the gap in the wall (x=8) with only 4 waypoints.
Plain A* would produce a staircase of ~20 steps.

---

## Part 2 — Build the Fabric mod (exports a .jar)

The `mod/` directory contains a complete Fabric mod project. Building it
produces a `.jar` you install like any other Minecraft mod.

### 2.1 Prerequisites on Debian

```bash
# Java 21 (required for Minecraft 1.21+)
sudo apt update && sudo apt install -y openjdk-21-jdk

# Verify
java -version
# Should print: openjdk version "21.x.x" ...

# Gradle (used to build the mod; the wrapper handles the rest)
sudo apt install -y gradle
# Or via SDKMAN (gets a more recent Gradle):
#   curl -s "https://get.sdkman.io" | bash
#   source "$HOME/.sdkman/bin/sdkman-init.sh"
#   sdk install gradle 8.8
```

### 2.2 Generate the Gradle wrapper (one time)

The `mod/` directory has `build.gradle` and `gradle.properties` but not the
binary wrapper yet. Run this once to create it:

```bash
cd use-cases/minecraft-wayfinding/mod
gradle wrapper --gradle-version=8.8
```

This creates `gradlew`, `gradlew.bat`, and `gradle/wrapper/`. After this you
use `./gradlew` instead of `gradle` for all subsequent commands.

### 2.3 Build the mod

```bash
# Still inside mod/
./gradlew build
```

Gradle downloads Minecraft, the Fabric toolchain, and mappings on the first
run (expect 5–10 minutes and ~1 GB of downloads). Subsequent builds are fast.

The finished mod jar will be at:
```
mod/build/libs/wayfinder-1.0.0.jar
```

---

## Part 3 — Install the mod

### Singleplayer (your own PC)

1. Install **Fabric Loader** from [fabricmc.net/use/installer](https://fabricmc.net/use/installer/).
2. Download **Fabric API** from [modrinth.com/mod/fabric-api](https://modrinth.com/mod/fabric-api) — pick the version for MC 1.21.1.
3. Copy both jars into your mods folder:
   - Windows: `%appdata%\.minecraft\mods\`
   - macOS: `~/Library/Application Support/minecraft/mods/`
   - Linux: `~/.minecraft/mods/`
4. Launch Minecraft with the Fabric profile.

### Multiplayer — install on the server (Debian VPS)

```bash
# 1. Create a server directory
mkdir -p ~/minecraft-server && cd ~/minecraft-server

# 2. Download the Fabric server installer (check fabricmc.net for latest URL)
wget https://maven.fabricmc.net/net/fabricmc/fabric-installer/1.0.1/fabric-installer-1.0.1.jar

# 3. Install Fabric server for MC 1.21.1
java -jar fabric-installer-1.0.1.jar server -mcversion 1.21.1 -downloadMinecraft

# 4. Accept the EULA
echo "eula=true" > eula.txt

# 5. Create the mods folder and copy in the two required jars
mkdir -p mods
#   a) fabric-api  — download from modrinth or use wget with the direct URL
#   b) wayfinder   — copy from your build output
cp /path/to/wayfinder-1.0.0.jar mods/
cp /path/to/fabric-api-*.jar    mods/

# 6. Start the server
java -Xmx2G -Xms1G -jar fabric-server-launch.jar nogui
```

**Keep the server running** with tmux or screen:
```bash
sudo apt install -y tmux
tmux new-session -s mc
java -Xmx2G -Xms1G -jar fabric-server-launch.jar nogui
# Detach: Ctrl-B then D
# Reattach: tmux attach -t mc
```

Or create a systemd service for automatic startup:
```ini
# /etc/systemd/system/minecraft.service
[Unit]
Description=Minecraft Fabric Server
After=network.target

[Service]
User=YOUR_LINUX_USER
WorkingDirectory=/home/YOUR_LINUX_USER/minecraft-server
ExecStart=/usr/bin/java -Xmx2G -Xms1G -jar fabric-server-launch.jar nogui
Restart=on-failure

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl enable --now minecraft
sudo journalctl -fu minecraft   # watch logs
```

> **Server mods note:** For Fabric server-side mods the `/navigate` command
> works for all players connected to the server. Players do NOT need the mod
> installed on their own PC — it is entirely server-side.

---

## Part 4 — In-game commands

All commands require the player to be in-game (not the server console) because
they need your current position as the start point.

| Command | What it does |
|---------|-------------|
| `/navigate <x> <y> <z>` | Find the path from where you stand to the given block coordinates and display it |
| `/navigate stop` | Clear the current path display |
| `/navigate here` | Print your current block coordinates |

### Example session

```
> /navigate 250 64 -180

Wayfinder: searching (12, 64, 5) → (250, 64, -180)…
Path found: 6 waypoint(s).
  [0] (12, 64, 5)
  [1] (58, 64, -20)
  [2] (120, 64, -75)
  [3] (180, 64, -110)
  [4] (220, 64, -150)
  [5] (250, 64, -180)
```

Glowing **END_ROD particles** are also spawned at each waypoint so you can
see the path in the world without reading coordinates.

### Operator / permission level

By default Brigadier commands registered without `.requires(...)` are
available to all players. To restrict to ops only, change the command
registration in `NavigateCommand.java`:

```java
dispatcher.register(literal("navigate")
    .requires(src -> src.hasPermissionLevel(2))  // 2 = OP level
    ...
```

---

## Part 5 — Singleplayer vs. multiplayer

| Feature | Singleplayer | Multiplayer |
|---------|-------------|------------|
| Works? | ✅ Yes | ✅ Yes |
| Mod needed on client? | Yes (you are both client and server) | **No** — server-side only mod |
| Mod needed on server? | N/A (integrated server) | Yes — install on the server |
| Path shown to other players? | N/A | No — only the requesting player sees the path |
| Works in all gamemodes? | Yes (Survival, Creative, Adventure, Spectator) | Yes |
| Works in the Nether / End? | Yes — the algorithm is dimension-agnostic | Yes |
| LAN worlds? | Yes — install mod on the host machine | Yes |

**Singleplayer note:** In singleplayer, Minecraft runs an integrated server.
The mod runs on that integrated server, so you install it in the normal
`mods/` folder and it just works.

---

## Part 6 — Minecraft version compatibility

| MC version | Java required | Supported? | Notes |
|------------|--------------|-----------|-------|
| **1.21.x** | Java 21 | ✅ **Primary target** | `gradle.properties` is set for 1.21.1 |
| **1.20.x** | Java 17 | ✅ With tweaks | Change `minecraft_version`, `yarn_mappings`, `fabric_version` in `gradle.properties`; some method names differ |
| **1.19.x** | Java 17 | ✅ With tweaks | Same as above; `ServerPlayerEntity.getServerWorld()` may differ |
| **1.18.x** | Java 17 | ✅ With tweaks | World height changed in 1.18 (y: −64 to 320); update y argument bounds |
| **1.17.x** | Java 16 | ⚠️ Possible | Significant API changes; not tested |
| **1.14–1.16** | Java 8/11 | ⚠️ Old Fabric API | Command API differences; not recommended |
| Below 1.14 | — | ❌ No Fabric | Use Forge instead; algorithm code is unchanged |

### Switching to a different MC version

1. Go to [fabricmc.net/develop](https://fabricmc.net/develop/) and look up the
   correct values for your target version.
2. Edit `mod/gradle.properties`:
   ```properties
   minecraft_version=1.20.4
   yarn_mappings=1.20.4+build.3
   loader_version=0.15.11
   fabric_version=0.97.0+1.20.4
   ```
3. Run `./gradlew build` again. If any method names have changed, the compiler
   will tell you exactly what to fix.

---

## Part 7 — Current limitations and next steps

| Limitation | Impact | Fix |
|------------|--------|-----|
| Search bounded to ~64 blocks per axis | Can't path across large distances in one call | Hierarchical search or waypoint chaining |
| No vertical gap handling | Won't find paths that require jumping | Add jump-aware neighbour generation |
| Player treated as 1×1×1 | Ignores the real 0.6×1.8 player hitbox | Check a vertical column of 2 blocks |
| Particles fade quickly | Path disappears in seconds | Persistent bossbar / map pin overlay |
| No path persistence | Every `/navigate` recomputes from scratch | Cache the last path per player |

---

## Files

```
minecraft-wayfinding/
├── README.md               ← this file
│
├── (C reference implementation)
├── theta_star.h / .c       ← Theta* in C
├── demo.c                  ← runnable C demo
├── Makefile                ← builds demo
│
└── mod/                    ← Fabric mod (builds the .jar)
    ├── build.gradle
    ├── gradle.properties   ← change MC/Fabric versions here
    ├── settings.gradle
    └── src/main/
        ├── java/com/augystudios/wayfinder/
        │   ├── WayfinderMod.java       ← mod entry point
        │   ├── VoxelLOS.java          ← Java port of wayfind.c
        │   ├── ThetaStar.java         ← Theta* pathfinder
        │   └── NavigateCommand.java   ← /navigate command
        └── resources/
            └── fabric.mod.json
```
