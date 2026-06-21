# draftDesk — Goals

The north star, the design philosophy, and the roadmap. Read this first; it explains *why* the
rest of the kit is shaped the way it is.

## The problem

Greyboxing a level the normal way is *place → playtest → rebuild → repeat*. Every block is
eyeballed, scale drifts, and the numbers that actually decide whether a space *plays* (door width,
step rise, ceiling height, jump gap, vault height) live in the designer's head instead of in the
geometry. draftDesk's premise: **design by configuration, not by hand-placement.**

## North star — threshold-first authoring

> "I am really planning my levels around the thresholds. We MAP with them as our anchor points —
> plan the connections, then fill in the spaces."

A level is a **graph of connections** (doors, arches, windows, stairs) before it is a set of rooms.
So the authoring primitive is the **threshold**, not the wall:

1. The author places/drags **threshold markers** (movable editor actors) where the player should
   pass from one space to the next.
2. The geometry is **built around** those thresholds — rooms grow to meet them, walls carve to
   admit them.
3. *Thresholds are the input; geometry is the output.* A declared connection can **never** resolve
   to a solid wall (the connection guarantee). Nobody blocks a door when they design around flow.

This is the opposite of "draw walls, then cut doors." See [RULES.md](RULES.md) for the invariants
that make it hold, and [WORKFLOW.md](WORKFLOW.md) for the loop in practice.

## Vocabulary-driven authoring

The end state: **the author dictates a layout in level-design vocabulary and the tool produces it.**
"Entrance hall, guard station off the approach, grand stair up to a balcony, royal suite beyond" →
a metric-correct greybox. The pieces (room, corridor, junction, door, arch, window, stair, ramp,
dais, cover, pillar) are the vocabulary; the engine fills the spaces between the thresholds.

## Metrics are the single source of truth

Every dimension that affects play comes from a `UDraftDeskSpec` data asset (`FDraftDeskMetrics`):
door size, step rise/run, max step angle, ceiling, vault/window, jump up/gap, mantle, grapple,
capsule, speeds, FOV. Change a number, the blockout *and* the player locomotion respond live. The
geometry is a pure function of (metrics, layout). See `Docs/LD_Metrics.json` for the schema.

One of those metrics is the **grid** (`GridSnap`, default 50 cm): every footprint, opening, and floor
height snaps to it, and wall thickness rides as a whole cell so blocking stays grid-aligned for clean
pathing and easy modular assembly. The grid is *precision over artistic control* — you author to it,
or the tool rounds you onto it (including off-grid threshold-marker drags). See [RULES.md](RULES.md).

It is a call designers make **up front, not per-block.** The grid is the first field on the Spec and
defaults to a real value (50), so there is no unset state and no per-snap free-for-all to fall into —
you commit to a grid and the tool holds the whole blockout to it. The harness echoes the active grid
on every apply so you never build without seeing which grid you're on. Designers don't yolo snaps.

## Success criteria

- **Dictate → produce.** A described layout becomes a faithful, metric-correct greybox.
- **Resilient.** The pipeline never silently fails in a way that "pushes people away from the tool"
  — a missing door, a blocked passage, a door cutting through a wall top. Failures are loud, or
  (better) impossible by construction.
- **Session- and PC-portable.** Everything needed to rebuild and operate the kit lives in this repo
  (engine + `Tools/` harness + these docs), not in any one machine's head or chat history.

## Roadmap / status

**Done**
- Rooms-and-links graph engine; presets (SingleRoom, Corridor, RoomHallRoom, LBend, TJunction,
  Cross, Grid2x2, SplitLevel, Tower, Ramp, Mezzanine) + the `Custom` authored-arrays path.
- Metric-correct stairs/ramps (built from StepRise/StepRun within MaxStepTraversalAngle).
- **Connection guarantee** — a declared link always carves a real opening (tolerant facing +
  carve-both-walls).
- **Movable threshold markers** (`ADraftDeskThreshold`) as the authoring surface *(engine)*.
- **Marker → geometry sync, Stage A** *(harness — `dd_sync`)* — drag markers, run `dd_sync`, geometry
  rebuilds: slide, resize, merge-on-delete, with an entry-deletion guard. Relies on the engine clamp.
- **Durable opening clamp** *(engine — `CarveOpenings`)* — any authored door position/width stays on
  the shared wall, from any source.
- **Nav auto-sync** *(harness — `write_apply`, on apply)* — every harness apply re-fits the
  `NavMeshBoundsVolume`. (An in-editor Spec/preset edit rebuilds geometry but does not re-fit nav.)
- **Grid snap** *(engine — `Metrics.GridSnap`, default 50 cm)* — footprints, openings, and floors snap
  to the grid from any source; `WallThickness` rounds up to a whole cell so abutment dedup survives;
  stairs stay metric-correct. The harness (`dd_author`/`dd_sync`) snaps authored coords and reports
  off-grid marker drags.

**Next**
- **Stage B reshape** — a marker dragged *perpendicular* off its wall moves/grows the wall (and the
  room footprint) to follow it. The natural completion of threshold-first authoring.
- Robustness backlog (stairs markers → cross-flight offset; window Z-drag → sill; 3D-aware
  abutment; column-in-doorway check; min-pier between two doors on one wall; idempotent
  delete→re-add; **box snap exempts Z / footprint-only** — snapping a solid's center on Z floats thin
  floor decor, e.g. a 40cm dais, so author boxes on grid for now). See the per-item list in the project notes.

**Horizon**
- Consume the **GAME356 kit** — emit real `AInteractableDoor` actors at thresholds so the greybox is
  playable with working doors/interactables, not just openings.
