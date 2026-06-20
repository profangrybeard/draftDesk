# draftDesk — Rules

The invariants the kit is built on. Break one and the geometry lies, the editor crashes, or a key
silently misses. Grouped: **LD construction rules**, **engine / data-model rules**, **MCP/authoring
rules**. The LD construction rules (R1–R4) live in [LD_RULES.md](LD_RULES.md); the metric schema in
[LD_Metrics.json](LD_Metrics.json). This file is the normative reference for the engine + harness.

## LD construction rules (summary — full text in LD_RULES.md)

- **R1 — PlayerStart at the space's origin.** The actor origin is the *entry threshold* the player
  first experiences; the generator's `NormalizeToEntry` translates the layout so the `bIsEntry`
  link lands at world (0,0,0). One `PlayerStart` at the generator spawns you in the doorway of any
  layout (all entries normalize to the origin).
- **R2 — Every space has an origin point.** Even a short approach corridor before the first room. No
  spawns floating mid-room.
- **R3 — Blocking uses the world-aligned grid material** (triplanar, never mesh-UV) so scale reads
  true on every surface regardless of mesh scale.
- **R4 — Blocking conforms to the LD metrics.** Doors, steps, ceilings, vault/window, jump, grapple
  are built from the metrics, never eyeballed.

## Engine / data-model rules

These hold inside `ADraftDeskGenerator` (`Source/DraftDesk/Private/DraftDeskGenerator.cpp`).

- **A layout is rooms + links.** A `FDraftDeskRoom` is an axis-aligned **interior** footprint
  (Min/Max XY) at a `FloorZ` with a clear `Height`. A `FDraftDeskLink` connects two rooms (or a room
  to the exterior, `RoomB == INDEX_NONE`). Stairs/boxes are the other two arrays. Geometry is a pure
  function of (rooms, links, stairs, boxes, metrics).
- **Walls grow OUTWARD by `WallThickness/2`.** So the room's Min/Max is the *true clear span* — a
  corridor authored 200 wide is 200 of walkable width (R4). Never inset the interior to "account for
  walls."
- **Abutment rule: leave exactly a `WallThickness` (T) gap between abutting interior extents**
  (`A.Max + T == B.Min`). Then the two wall planes coincide (`A.Max+T/2 == B.Min−T/2`) and the edge
  ledger **dedups them to ONE shared wall**. A forgotten gap → a double wall or a sliver. The
  harness `east_of`/`north_of` helpers place rooms with this gap automatically.
- **Unequal-width abutment does NOT dedup.** The wider room owns a full wall; the narrower room/
  corridor sets `OpenEdgeMask` on its through-edges so it registers no wall there.
- **Connection guarantee — a declared link NEVER resolves to a solid wall.** `FaceConnection`
  tolerantly finds how two rooms face (across any gap or unequal widths, picking the closest valid
  facing with perpendicular overlap), and the carve punches through **both** facing wall planes. A
  strict abutment check would seal a door the moment a room is resized 10cm short — that is the
  failure mode that "pushes people away from the tool," designed out.
- **Opening clamp — an opening always stays on the shared wall, for ANY authored position/width.**
  `CarveOpenings` computes the opening interval **once** from the room-overlap `[SLo,SHi]`:
  `Weff = min(Width, Span)`, `Center = clamp(midpoint + Position, SLo+Weff/2, SHi-Weff/2)`, shared by
  both wall planes. Consequences you can rely on: a door can't be wider than the shared wall, can't
  slide off it, can't open where the neighbour isn't, and both walls always carve one **aligned**
  passage. No corner-post margin is needed — each wall record overhangs the overlap by ≥T, so an
  opening flush to the overlap still leaves a structural pier.
- **Link `Width`/`Height`/`Sill` of 0 means "use the metric default"** — `DoorWidth`/`DoorHeight`
  for a Doorway, `CorridorWidth` for an Open/Stairs, `WindowClearHeight`+`HalfWallHeight` for a
  Window. Authoring a 0 is normal and correct; it's not "no door."
- **Loud failure.** A declared link that cannot resolve logs a `UE_LOG(Warning, …unresolved link…)`
  from `CarveOpenings` — never a silent sealed wall.
- **Stairs/Ramp links are queued, not carved.** A vertical link (`Stairs`/`Ramp` with two rooms) is
  turned into a stair *flight* from the rooms' geometry in `CarveOpenings` and does NOT punch a wall
  opening — the rooms are separated by the run, so `FaceConnection`/`ResolveLinkEdge` would (rightly)
  reject the non-abutting pair. This is why a stairs link doesn't "carve a door."
- **Stairs/ramps are metric-correct.** `N = max(1, ceil(DZ/StepRise), ceil(DZ/(StepRun·tan(θ))))`
  where `θ = min(MaxStepTraversalAngle, 89°)`; effective rise `DZ/N ≤ StepRise`. `StepCount` returns
  0 for `DZ ≤ 1` (no climb). The same `StepCount/TotalRun` sizes both the inter-floor gap (so the
  upper floor is the flush landing) and the emitted flight.
- **Window openings keep a sill + lintel.** A `Window` carves a clear band `[Sill, Sill+Clear]`
  (defaults `HalfWallHeight` + `WindowClearHeight`); a window over a **rail** edge degrades to a
  full-clear gap. Non-full-clear openings clamp `Height ≤ WallH−40` and `≥ Sill+40` so the opening
  never cuts the wall top.

## MCP / authoring rules

For driving the generator over the in-editor MCP server (the `Tools/` harness).

- **`set_properties` is CASE-INSENSITIVE on keys.** Both `Width` and `width` (and `CellSize`/
  `cellSize`, etc.) apply, on the generator and on marker actors — UE resolves the property by
  `FName`. (Verified by an in-call A/B test. An earlier "PascalCase is silently ignored" claim was a
  misdiagnosis — markers reading 0 were the *write-persistence flakiness* in
  [RESISTANCE.md](RESISTANCE.md), not casing. The generator apply path uses PascalCase array keys and
  always worked.) The harness mixes both casings harmlessly.
- **`get_properties` OUTPUT casing depends on depth.** Top-level UPROPERTY names come back
  **PascalCase** (`"Width": 360`); **nested struct fields** come back **first-char-lowercase**
  (`"doorWidth": 240`). This asymmetry is real — match the *read* side to the depth.
- **Enums serialize as the value-name string** (`"Doorway"`, `"West"`, `"Custom"`). `FVector2D` =
  `{x,y}`, `FVector` = `{x,y,z}`.
- **Metrics live in the Spec asset, not the generator.** Read `generator.Spec` → then the Spec's
  `Metrics` struct. Reading `Metrics` off the generator errors.
- **Apply order is fixed** (the generator rebuilds on *each* array set, one at a time): clear
  connections (Links/Stairs/Boxes) → clear Rooms → set Rooms → set Stairs/Boxes → set Links. Never
  leave Links populated while Rooms is empty (logs spurious broken-link noise, and historically
  asserted on a stale room index). The harness `write_apply()` bakes this order in.
- **Authored link room indices are bounds-checked.** `ResolveLinkEdge`/`CarveOpenings` skip a link
  whose RoomA/RoomB is out of range, so a mid-edit transient (arrays applied one at a time) can't
  crash the editor.
- **Nav follows the footprint — harness-side, on apply.** This is NOT an engine feature (there is no
  nav code in `ADraftDeskGenerator`). The harness `write_apply()` re-fits the `NavMeshBoundsVolume`
  to the generator bounds (+500 margin) on every apply. So an in-editor Spec/preset edit rebuilds
  geometry but does *not* re-fit nav; only a harness apply does.

See [RESISTANCE.md](RESISTANCE.md) for the failure modes these rules defend against, and how to
recover when one bites.
