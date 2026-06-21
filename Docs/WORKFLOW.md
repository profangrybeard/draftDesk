# draftDesk — Workflow

How to operate the kit end to end on a fresh machine: prerequisites, the build cycle, and the
threshold-first authoring loop. The runnable harness is in [`../Tools/`](../Tools) (see its
[README](../Tools/README.md) for the per-command reference).

## Prerequisites

A fully set-up machine: UE 5.8, a host project with the plugin built, the in-editor MCP server
enabled (`127.0.0.1:8000/mcp`, alive only while the editor is open), the scene objects placed
(generator + spec + nav volume + PlayerStart), and the harness configured (`Tools/dd_config.py` →
`GEN`/`SPEC`; run scripts from inside `Tools/`). **From a fresh machine, do [SETUP.md](SETUP.md)
first** — it covers all of the above from zero.

## The two cycles

**Code cycle (when you change C++):** edit → **close the editor** → `Build.bat …Editor Win64
Development` → reopen → drive. (The DLL is locked while the editor is open; see
[RESISTANCE.md](RESISTANCE.md).)

**Authoring cycle (no rebuild):** the generator rebuilds live on every property change, so authoring
is just: push a layout / drag markers → sync → screenshot. No editor restart.

## The threshold-first authoring loop

This is the core loop, matching the [philosophy](GOALS.md): *plan connections, fill spaces.*

1. **Dictate or sketch a layout.** Describe it in LD vocabulary; compose it with the `Layout`
   library ([`Tools/dd_author.py`](../Tools/dd_author.py)) — `level`/`stacked_levels`, `room`/
   `corridor`/`east_of`/`north_of` (auto-abutment), and the threshold primitives `door`/`passage`/
   `window`/`rail`/`entry`/`exterior`/`stairwell`/`atrium`/`hatch`, plus `flight` (an explicit
   edge-landing staircase) and `box`/`cover`/`pillar`/`dais`. [`Tools/dd_castle.py`](../Tools/dd_castle.py)
   is a full worked example (Castle Chorrol, two levels, dual grand staircase) — copy it as a template.
2. **Push it.** `python dd_castle.py` writes the `Custom` payload (Levels/Rooms/Thresholds/Flights/
   Boxes), applies it via `ddrun`, re-fits the nav volume, **and auto-runs the nav gate** (step 5).
   The greybox builds instantly.
3. **Seed thresholds.** `python dd_seedmarkers.py [layout]` spawns one movable `ADraftDeskThreshold`
   marker per connection at its resolved opening, and **saves the level** so the markers persist.
4. **Drag.** In the editor, move the markers — **slide** along the wall (onto the grid,
   `Metrics.GridSnap`, default 50 cm), drag one **perpendicular** to reshape the room, or **delete**
   one to merge two spaces (deleting an entry is refused, R1).
5. **Sync.** `python dd_sync.py [layout]` reads the moved markers (projected onto their planes by
   [`dd_anchor`](../Tools/dd_anchor.py)) and rebuilds the geometry:
   - **slide** — along-wall `Position`, clamped onto the wall (so the entry never strands the origin);
   - **resize** — width/height; **merge** — a deleted interior marker → full `Passage`;
   - **reshape (Stage B)** — a marker dragged perpendicular **moves the shared wall**, both abutting
     rooms following it (reverted if FaceConnection loses overlap or a room goes degenerate).

   It then **re-runs the nav gate** and **saves the level** (so drags survive a restart). It reports
   what slid / reshaped / merged and which markers sit *past the wall* (those want a longer wall — an
   along-axis reshape, not built). Re-run 4–5 freely; the sync rebuilds from the layout baseline + the
   current markers, so it's idempotent.

## The nav gate (walkability, by query — not screenshots)

Watertight geometry can still be unwalkable (a stair that doesn't connect, an island room). The
editor module exposes **`DraftDeskEditor.DdNavToolset.CheckReachability`** (queries the live navmesh
via `FindPathToLocationSynchronously`). [`Tools/dd_navcheck.py`](../Tools/dd_navcheck.py) asserts:
- **per-connection** (`check_connections`): every declared threshold is traversable A↔B (catches a
  single blocked/sealed door that global reachability would mask);
- **global** (`check`): every room is reachable from the entrance.

Both retry to ride out the async navmesh rebuild, and run automatically inside `dd_castle` and
`dd_sync`. **A reshape only "sticks" if the layout stays 23/23 connections + every room walkable.**

## Verifying the core off-engine

The watertight geometry is proven without the editor: `Tools/shell/` is a pure-Python oracle
(`shell.py` + `rects2d.py`) with a battery (`python battery.py`, 29 cases) and a property fuzzer
(`adv_fuzz.py`); `Tools/shell/cpp/ShellBatteryTest.cpp` compiles the C++ core standalone (via cl.exe)
and a digest diff proves it is **byte-identical** to the oracle. Change the core → re-run both before
touching the engine.

## Where things live

- **Engine:** `Source/DraftDesk/` (runtime) — `DraftDeskMetrics.h` (schema), `DraftDeskLayout.h`
  (`FDdLevel`/`FDdRoom`/`FDdThreshold`/`FDdFlight` + enums), `Private/Shell/DdShellCore.h` (the
  portable watertight 2D-Boolean core), `DraftDeskGenerator.h` + `Private/DraftDeskGenerator.cpp`
  (build-on-edit actor: authored graph → core → boxes + flights), `DraftDeskThreshold.h/.cpp` (the
  movable marker actor), `DraftDeskStatics.h/.cpp` (locomotion). `Source/DraftDeskEditor/` (editor)
  — `DdNavToolset` (the nav-query MCP tool).
- **Harness:** `Tools/` — see [Tools/README.md](../Tools/README.md); `Tools/shell/` is the pure-data
  oracle the C++ core mirrors.
- **Reference:** `Docs/` — [GOALS](GOALS.md), [RULES](RULES.md), [RESISTANCE](RESISTANCE.md),
  [LD_RULES](LD_RULES.md), [LD_Metrics.json](LD_Metrics.json).
