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

**Next**
- **Stage B reshape** — a marker dragged *perpendicular* off its wall moves/grows the wall (and the
  room footprint) to follow it. The natural completion of threshold-first authoring.
- Robustness backlog (stairs markers → cross-flight offset; window Z-drag → sill; 3D-aware
  abutment; column-in-doorway check; min-pier between two doors on one wall; idempotent
  delete→re-add). See the per-item list in the project notes.

**Horizon**
- Consume the **GAME356 kit** — emit real `AInteractableDoor` actors at thresholds so the greybox is
  playable with working doors/interactables, not just openings.
