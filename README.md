# draftDesk

A metrics-driven, **connection-first** greybox blockout tool for Unreal Engine 5.8.

Define your game's **player-movement metrics** (door size, step rise/run, ceiling, vault, jump, grapple, …) in the Details panel and generate **watertight test space to spec** — live. Every room is a full 3D shell that is solid *by construction* and opens **only** where you declare a connection (a threshold). Change a metric and the blockout rebuilds; drag a threshold and the room reshapes around it; and every layout is verified **walkable** by querying the navmesh — not by eyeballing a screenshot.

> LLM/MCP-enabled: built and extended through Claude driving the in-editor MCP server, but the runtime tool is plain UE C++ — no AI required to use it.

## How it works — the SHELL model
A layout is **Levels** (ordered storeys) + **Rooms** (interior footprints on a level) + **Thresholds** (the *single* opening primitive: door = passage = window = rail = stairwell = hatch = atrium). Geometry is a pure function of these: every room deposits a solid rectangle on each of its six faces, every threshold deposits an aperture, and each per-plane bucket emits `union(faces) − union(apertures)` via an exact, tolerance-free 2D Boolean. A face is solid **by construction** and opens only where a threshold proves a connection — so there are no silent holes. The load-bearing core (`Source/DraftDesk/Private/Shell/DdShellCore.h`) is UE-agnostic and is mirrored **byte-for-byte** by a pure-Python oracle + battery in [`Tools/shell/`](Tools/shell), which proves watertightness before any engine wiring.

## Documentation
Everything needed to rebuild and operate the kit on a fresh machine lives in this repo:
- **[Docs/SETUP.md](Docs/SETUP.md)** — fresh-machine setup from zero (UE, host project, MCP server, assets).
- **[Docs/GOALS.md](Docs/GOALS.md)** — the vision and threshold-first philosophy.
- **[Docs/RULES.md](Docs/RULES.md)** — the invariants (LD rules R1–R4, engine/data-model, MCP/authoring).
- **[Docs/RESISTANCE.md](Docs/RESISTANCE.md)** — gotchas/failure-modes as symptom → cause → fix.
- **[Docs/WORKFLOW.md](Docs/WORKFLOW.md)** — operate it end to end (build cycle + authoring loop + nav gate).
- **[Docs/SHELL_REDESIGN_v1.md](Docs/SHELL_REDESIGN_v1.md)** — the design spec for the connection-first shell (implemented).
- **[Tools/](Tools/README.md)** — the Python authoring harness (drives the generator over MCP) + the watertight oracle.
- `Docs/LD_RULES.md`, `Docs/LD_Metrics.json` — the construction rules + metric schema.

## Install
1. Clone into your project's `Plugins/` folder (or the engine's `Plugins/` to share across projects):
   ```
   git clone <repo-url> YourProject/Plugins/DraftDesk
   ```
2. Enable **draftDesk** in the project's Plugins (the editor MCP toolset lives in the editor-only `DraftDeskEditor` module, which depends on the `ToolsetRegistry` plugin — enable it too, or via `AllToolsets`).
3. Rebuild the editor target when prompted (`Build.bat YourProjectEditor Win64 Development` with the editor closed).

## Use
1. **Author the spec.** Create a `UDraftDeskSpec` Data Asset and set your metrics — the single source of truth.
2. **Blockout.** Drop an `ADraftDeskGenerator` into the level, point its `Spec` at that asset, and pick a **Preset** from `draftDesk|Layout`. The greybox builds instantly and **rebuilds live** when you edit the spec or the layout:
   - **Single-level:** Single Room, Corridor, Room-Hall-Room, L-Bend, T-Junction, Cross (4-way), 2×2 Room Grid.
   - **Vertical:** Split Level, Tower, Ramp, Mezzanine — **stacked levels** connected by stairwell shafts / ramps, or a tall room with a partial mezzanine over an atrium void. Metric-correct flights (built from `StepRise`/`StepRun` within `MaxStepTraversalAngle`) carry the vertical circulation.

   The actor origin is the entry threshold (R1), so a `PlayerStart` at the generator spawns you in the doorway of any preset. Ceilings are first-class but hidden by default (`bPlaceCeilings`) so a top-down editor cam reads the plan; the watertight **validation always runs on the full shell** regardless.
3. **Game feel.** Make a `GameMode` that consumes the same spec — either subclass `ADraftDeskGameMode` or, in your own GameMode, hold a `Spec` and call `UDraftDeskStatics::ApplyLocomotion(Pawn, Spec)` on spawn (pushes `RunSpeed → MaxWalkSpeed` etc. onto the pawn's `CharacterMovementComponent`).

> **Important — match your project's playable setup.** `ADraftDeskGameMode` only applies the spec; it does not know your character, controller, or HUD. A playable pawn needs its matching `PlayerControllerClass` (where Enhanced Input is usually wired). Two clean options: subclass `ADraftDeskGameMode` and set `DefaultPawnClass`/`PlayerControllerClass`/`HUDClass` from your project's default GameMode; or keep your existing GameMode and just hold a `Spec` + call `ApplyLocomotion` on `RestartPlayer`.

## Author by threshold (markers + sync)
Beyond presets, draftDesk is a graph you author directly, the [threshold-first way](Docs/GOALS.md): plan the connections, fill the spaces.
- **`Custom` preset** builds from authored arrays (Levels / Rooms / Thresholds / Flights / Boxes). The [`Tools/`](Tools/README.md) harness composes these from a dictated layout (`dd_author`, with `dd_castle` as a worked example).
- **Movable threshold markers** (`ADraftDeskThreshold`) are the authoring surface: seed one per connection, **drag** them, then **sync** — the geometry rebuilds around them:
  - **slide** along the wall, **resize** (width/height), **merge** (delete a marker → the wall dissolves to a passage; deleting an entry is refused, R1),
  - **reshape (Stage B)** — drag a threshold *perpendicular* off its wall and the wall **moves**, both abutting rooms' facing edges following it (shared-face-follows-both).
  A declared connection can never resolve to a solid wall, and an opening can never slide off its wall (engine-clamped, any source). Every sync is **nav-gated** (a reshape only sticks if the navmesh stays connected) and **saved to the level** (drags survive an editor restart).

## Walkability is a gate, not a guess
The real test of a blockout is *"can the player actually walk everywhere it should?"* — which watertight geometry alone can't answer. The editor module exposes **`DraftDeskEditor.DdNavToolset.CheckReachability`**, which queries the live navmesh (`FindPathToLocationSynchronously`). [`Tools/dd_navcheck.py`](Tools/dd_navcheck.py) asserts, per layout: every declared **threshold** *and every* **flight** is traversable, and every **room** is reachable from the entrance. A flight is tested **base→top**, which isolates one staircase — a dual staircase with one dead side still passes global reachability (you take the other stair), so only the per-flight test catches it. `dd_castle` runs it automatically after each apply.

## Layout (files)
- `Source/DraftDesk/` — runtime C++ module
  - `DraftDeskMetrics.h` — `FDraftDeskMetrics` (the metric schema)
  - `DraftDeskLayout.h` — `FDdLevel` / `FDdRoom` / `FDdThreshold` / `FDdFlight` + `EDdThresholdKind` / `EDdPlaneClass`
  - `Private/Shell/DdShellCore.h` — the portable, UE-agnostic watertight 2D-Boolean core (deposit faces + apertures → Boolean → boxes + flight plans + rail gaps)
  - `DraftDeskGenerator.h/.cpp` — build-on-edit actor; converts the authored graph → `DdShellCore` → instanced boxes + stair flights
  - `DraftDeskThreshold.h/.cpp` — `ADraftDeskThreshold` (movable marker actor)
  - `DraftDeskStatics.*` — `ApplyLocomotion`
- `Source/DraftDeskEditor/` — editor-only module: `UDdNavToolset` (the nav-query MCP toolset)
- `Content/` — plugin content (grid material)
- `Tools/` — Python authoring harness (compose → push → seed → drag → sync → nav-gate → save); `Tools/shell/` — the pure-data oracle, battery, and the standalone C++ core test
- `Docs/` — goals, rules, resistance, workflow, the SHELL spec, construction rules + metric defaults

## Roadmap
**Done:** the connection-first watertight **SHELL** (Levels/Rooms/Thresholds; watertight by construction, proven by a 42-case oracle + C++ battery); all presets re-authored (stacked levels + stairwell/atrium for verticality); explicit edge-landing stair flights with **rail gaps derived from the flight** (`RailGap-from-flight`); **marker-drag authoring** — seed → drag → sync with slide / resize / merge / **Stage B reshape**, **nav-gated** and **persisted to the level**; a **nav-query** MCP tool + `dd_navcheck` walkability gate — **per-connection**, testing every threshold *and every flight* (each stair base→top, so one dead side of a dual staircase is caught even though global reachability masks it).
**Next:**
- Rewrite the stale `dd_genrepair` (still on the retired link model); door-frame trim; quiet cosmetic sync-report noise.
- Robustness backlog: column-in-doorway check; min-pier between two doors on one wall; 3D-aware abutment; window Z-drag → sill; stair/ramp marker edits.
- Auto-place a `PlayerStart` per space at its entry threshold (R2).
- Consume the **GAME356 kit** — emit real `AInteractableDoor` actors at thresholds.
