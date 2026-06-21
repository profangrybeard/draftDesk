# draftDesk SHELL v1 — design spec

> **STATUS: IMPLEMENTED.** This design is shipped. The watertight core is `Source/DraftDesk/Private/Shell/DdShellCore.h` (mirrored byte-for-byte by the `Tools/shell/` Python oracle, proven by a 42-case battery + property fuzzer); the data model is `DraftDeskLayout.h` (Levels/Rooms/Thresholds/Flights); the generator defers all geometry to the core. Beyond the spec, the build also landed: explicit edge-landing stair flights with **rail gaps derived from the flight**, the **marker-drag authoring loop** (seed → drag → sync; slide/resize/merge + Stage B reshape; nav-gated; persisted), and a **nav-query MCP tool** (`DraftDeskEditor.DdNavToolset`) + `dd_navcheck` walkability gate. The "open questions" below were resolved (atrium-as-threshold, explicit Levels[], strip-decomposition emit, SlabT, ProjectAnchorToPlane, validator policy) — see the project memory. Kept as the design record.


_Synthesized by the `draftdesk-shell-system-redesign` workflow — winner **Planar Boundary Ledger (PBL): unified 3D plane-sweep with relational openings**; 23 hard cases; 15 agents._

## Approach ranking

- **Planar Boundary Ledger (PBL): unified 3D plane-sweep with relational openings** — 74/100
- **Cellular Occupancy Shell (COS): levels as cell arrangements, faces as boundaries, openings as relational carves** — 68/100
- **Threshold-Anchored Cellular Shell (TACS): deduped 6-face Face Ledger, openness only via Thresholds** — 62/100
- **Unified Plane Ledger (UPL): walls and slabs through one deduped-plane-with-openings emitter** — 41/100

## Title
draftDesk SHELL v1 — Planar Boundary Ledger with COS BaseZ levels, a designed stair-hole-from-flight resolver, and TACS anchor authoring (the grafted, hole-free PBL)

## Summary
Final, build-ready shell architecture. Core is PBL: every room is a 6-face AABB; every face is deposited as a 2D solid rectangle into a per-plane ledger bucket; thresholds deposit aperture rectangles; each bucket emits (union of faces) MINUS (union of apertures) via an exact, tolerance-free axis-aligned 2D Boolean (coordinate-compression sweep + greedy maximal-rectangle merge). Walls, floors, ceilings, doors, windows, rails, stairwells, hatches and atria are ONE mechanism. OpenEdgeMask / RailEdgeMask / bCeiling / bNoFloor and the FDraftDeskStair-vs-Stairs-link duplication are deleted; a face is solid by construction and opens only where a threshold exists.

FOUR GRAFTS repair every fatal/major stress break on PBL:

(1) COS BaseZ level invariant REPLACES the self-contradictory FDdPlaneKey.Level. Levels are a first-class ordered array; BaseZ[n+1] = BaseZ[n] + Height[n] + SlabT is structural, so a shared slab is ONE coordinate (lower ceiling z == upper floor z) BY CONSTRUCTION, not a near-equal-Z merge heuristic. The slab bucket keys on the integer INTERFACE index (the plane between level n and level n+1 is interface n+1), so unrelated rooms that merely share a Z can never false-merge and a stack can never false-split. Per-room Height becomes an OVERRIDE reconciled at build (loud warn if Height+SlabT != the level BaseZ delta) — this dodges the TACS fatal (per-room Height desyncing the slab key) while keeping a real headroom field.

(2) A DESIGNED stair-hole-from-flight resolver closes PBL's unspecified FootprintIntersection path. A Stairwell/Ramp threshold derives its 2D slab aperture from the flight plan (StartU/Dir/TotalRun/Width/CrossV) targeting the interface bucket at the UPPER level, then carves the hole AND queues the flight in ONE derivation. A horizontal opening-clamp (the missing analogue of the vertical clamp) keeps the aperture inside the bucket's shared/own slab XY — no level-leak, never a sealed stair.

(3) An explicit off-grid-stair / on-grid-slab RECONCILIATION rule: because StepRun=30 is not a GridSnap (50) multiple, the slab WELL is snapped to the grid but sized to fully ENCLOSE the off-grid flight extent plus a landing margin (round OUT, never IN), so the top tread always lands on solid floor and never rams a quantized-in sliver. The same rule derives RailGap apertures at stair landings from the flight CrossV (not authored Position) — closing the dormant 'rail-edge openings only fire on hand-authored graphs' finding.

(4) TACS Anchor-as-storage threshold + engine-side ProjectAnchorToPlane as the single source of truth for Position, killing the Python threshold_points() hand-mirror. But PBL's tolerant overlap-REQUIRED FaceConnection stays the gate (NOT TACS 'nearest face + grow-room-to-anchor'): a zero-overlap / corner-touch anchor still fails LOUD and carves nothing.

The Phase-0 pure-data 2D-Boolean battery is kept and elevated with a closed-manifold / watertight-area assertion run on every preset before any UE wiring. The two conceded partial-floor cases (atrium, mezzanine) are made structural via a slab-suppression rule (interface cells under a Void threshold emit no slab) plus an explicit Cantilever guard that fails loud rather than silently roofing a void. Greenfield on the geometry core; surgical on graph/IO. The 50cm two-level castle (dd_castle) migrates cleanly: balcony becomes Level 1, the two hand-authored stairs + two hand-authored rail gaps collapse into two Stair thresholds whose RailGaps derive from CrossV.

## Data model
POST-OpenEdgeMask data model. All USTRUCT(BlueprintType), reflected, authorable. Deletes OpenEdgeMask, RailEdgeMask, bCeiling, bNoFloor, bPlaceCeilings, and the FDraftDeskStair-vs-Stairs/Ramp-link duplication.

=== NEW: FDdLevel (first-class storey; the COS graft) ===
  int32 Index            // 0..N, contiguous, ordered
  float BaseZ            // top-of-floor of this level (actor-local cm). INVARIANT: BaseZ[n+1] = BaseZ[n] + Height[n] + SlabT
  float Height           // nominal clear storey height (>= max(CeilingMin, DoorHeight+60)); per-room Height overrides
  float SlabT            // floor/ceiling slab thickness for THIS level's ceiling interface; = BuiltWallT rounded up to a whole GridSnap.Z cell
Levels live in an ordered TArray<FDdLevel> on the generator (authored in Custom mode, or built by a preset). They are the discrete vertical truth. A room references a level by index; FloorZ is derived = Levels[room.Level].BaseZ unless the room carries a FloorZ override (reconciled, see below).

=== FDdRoom (a full 3D shell, not a footprint) ===
  FVector2D Min, Max     // interior footprint (clear span; walls grow OUTWARD — R4 preserved)
  int32 Level = 0        // INTEGER storey index into Levels[]. The discrete key for stack dedup + leak-proof separation.
  float FloorZ = NaN     // OVERRIDE only. Default NaN => FloorZ := Levels[Level].BaseZ. If set, build-time reconcile asserts |FloorZ - Levels[Level].BaseZ| < eps or warns LOUD and snaps to the level (so the slab key never desyncs — the TACS fatal is structurally impossible).
  float Height = 0       // OVERRIDE clear interior height. 0 => Levels[Level].Height. At build, if Height>0 AND a room is stacked-under another, assert Height + SlabT == (Levels[Level+1].BaseZ - Levels[Level].BaseZ) or warn LOUD (mismatched-ceiling case is legal between SIDE-BY-SIDE rooms; under a stack it is an error).
  uint8 SurfaceFlags = FLOOR|CEIL   // per-face PRESENCE intent only (FloorBit, CeilBit). 'this room intends no surface here at all' (courtyard missing roof, pit missing floor). NOT 'open this side'. Walls are ALWAYS present; a wall opens only via a threshold.
  bool  bColumns = false
  // helpers W()/D()/CX()/CY() unchanged.
There is NO OpenEdgeMask and NO RailEdgeMask. 'Open corridor mouth' = a Passage threshold sized to the full overlap. A rail = a Rail threshold spanning the edge. This is the charter's relational-openness made structural.

=== FDdThreshold (the SINGLE opening primitive: door=passage=window=rail=stairwell=hatch=atrium) ===
  int32 RoomA                       // owner room
  int32 RoomB = INDEX_NONE          // INDEX_NONE => exterior
  EDdThresholdKind Kind             // Doorway, Passage, Window, Rail, Stairwell, Hatch, Ramp, Atrium
  EDdPlaneClass Plane               // Vertical (carves a wall between A and B/exterior) or Horizontal (carves the floor/ceiling SLAB between a lower and upper room)
  float Position = 0                // signed offset along the shared interval (vertical drag axis 1; horizontal U axis)
  float Position2 = 0               // signed offset on the 2nd in-plane axis (horizontal openings are 2D: a stairwell has X and Y). Unused for vertical.
  float Width = 0                   // 0 => kind default (DoorWidth / CorridorWidth / stair footprint width)
  float Depth = 0                   // horizontal-only: the run dimension of the hole. 0 => derived from the flight TotalRun (the off-grid-out rule).
  float Height = 0                  // vertical clear height; 0 => DoorHeight / WindowClearHeight
  float Sill = 0                    // window sill; 0 => HalfWallHeight. Rail uses HalfWallHeight as its TOP.
  bool  bIsEntry = false            // R1 single-entry (unchanged)
  EDraftDeskEdge ExteriorEdge       // only when RoomB==INDEX_NONE && Plane==Vertical
  bool  bRamp = false               // Stairwell/Atrium with bRamp => pitched slab instead of treads
  // derived-at-build, not authored: the flight plan (StartU/Dir/CrossV/W) for Stairwell/Ramp and the RailGap apertures it induces on any adjacent Rail.
A threshold UNIFIES the old FDraftDeskLink, FDraftDeskStair, the Window/Stairs/Ramp Kinds, and the rail mask.

=== ADraftDeskThreshold (the TACS anchor graft) ===
Gains RoomA / RoomB / Plane / Kind / Width / Height / Depth / Sill / bIsEntry. Its WORLD TRANSFORM is the position truth; the engine projects it onto the resolved plane to derive Position/Position2 (ProjectAnchorToPlane, single source of truth). This kills the threshold_points() hand-mirror and the aspirational/disconnected-graph-refs gap.

=== INTERNAL LEDGER TYPES (not reflected — plane-sweep state) ===
FDdPlaneKey { uint8 Class (0=constX wall, 1=constY wall, 2=horizontal slab); int64 PlaneQ = RoundToInt(Plane / Quantum); int32 Interface (slab buckets ONLY — the integer interface index between levels; the COS graft REPLACES the incoherent Level field). } Quantum = GridSnap on that axis (grid-cell exact — fixes silent dedup drift). For walls, Interface is unused/0. For slabs, the bucket identity is (Class=2, PlaneQ, Interface) and Interface ALONE (not near-equal Z) decides merge: a lower room's ceiling and the upper room's floor both target Interface = upper.Level, so they share a bucket BY CONSTRUCTION; two unrelated rooms at the same raw Z but different stacks land on different interfaces and never merge.
FDdFaceRect { float ALo,AHi (in-plane axis A); float BLo,BHi (axis B: walls B=Z, slabs B=2nd footprint axis); float Thick (BuiltWallT or SlabT); int32 SourceRoom (diagnostics). }
FDdAperture { float ALo,AHi,BLo,BHi (hole rect in plane 2D); EApertureProfile profile (FullClear | Doored(lintel) | Windowed(sill+lintel) | RailGap | SlabHole); float SillZ,CapZ (vertical profiles); int32 SourceThreshold (provenance). }
The whole model: Rooms deposit FaceRects, Thresholds deposit Apertures, both keyed to FDdPlaneKey buckets; emission = exact 2D Boolean per bucket.

## Shell-emission algorithm (walls)
UNIFIED PLANE-SWEEP EMISSION. ONE routine for all 4 plane families; the 'wall algorithm' and 'slab algorithm' are the SAME code parameterized by FDdPlaneKey.Class. Replaces BuildEdgeLedger + CarveOpenings + EmitWall + EmitFloorsAndCeilings.

--- PASS 0: RECONCILE LEVELS (the COS graft) ---
Build Levels[] (authored or preset). Assert/snap the BaseZ invariant: for each adjacent pair, BaseZ[n+1] must equal BaseZ[n] + Height[n] + SlabT; if a room overrides Height under a stack, assert it matches or warn LOUD and use the level value. Resolve each room's FloorZ := Levels[room.Level].BaseZ (or its reconciled override) and EffH := room.Height>0 ? room.Height : Levels[room.Level].Height, clamped to max(CeilingMin, DoorHeight+60). NormalizeToEntry (R1) and SnapLayoutToGrid run here unchanged (stairs exempt — R4).

--- PASS 1: DEPOSIT FACES ---
For each room R (skip degenerate W<=1 || D<=1):  T=BuiltWallT, H=EffH(R), FZ=FloorZ(R).
  VERTICAL faces — ALWAYS deposit (presence unconditional; no OpenEdgeMask gate):
    West : key(0, q(Min.X - T/2));  rect A=[Min.Y-T, Max.Y+T], B(Z)=[FZ, FZ+H]
    East : key(0, q(Max.X + T/2));  same family
    South: key(1, q(Min.Y - T/2));  rect A=[Min.X-T, Max.X+T], B(Z)=[FZ, FZ+H]
    North: key(1, q(Max.Y + T/2))
    The +/-T overhang on the in-plane extent is the corner-pier guarantee (preserved verbatim from current cpp:597-598). Note rect B (the Z span) is the room's OWN height; UnionOfRectangles in a shared wall bucket unions [FZ,FZ+H_short] with [FZ,FZ+H_tall] to the TALL height — the mismatched-ceiling clerestory is solid by construction (resolves stress 'different ceiling heights').
  HORIZONTAL faces — deposit per SurfaceFlags, KEYED BY INTERFACE (the COS graft):
    if FloorBit: key(2, q(FZ),     Interface = R.Level);     rect = footprint+2T in XY, Thick=SlabT
    if CeilBit : key(2, q(FZ+H),   Interface = R.Level+1);   rect = footprint+2T in XY, Thick=SlabT
    A lower room's ceiling (Interface = L+1) and the upper room's floor (Interface = (L+1).Level = L+1) land in the SAME bucket BY CONSTRUCTION (same interface index AND, via the BaseZ invariant, the same q(Z)). This is the stacked-slab dedup — structural, not a near-equal-Z heuristic. Two rooms at the same raw Z in unrelated stacks differ in Interface and never merge (false-merge impossible); a stack whose Height drifted can never split because Interface is the integer key, not Z (false-split impossible).

--- PASS 2: DEPOSIT APERTURES ---
For each threshold Th, resolve bucket(s):
  VERTICAL (Plane==Vertical): FaceConnection (tolerant, overlap-REQUIRED) returns BOTH facing planes (PlaneA, PlaneB) and overlap [SLo,SHi]; exterior uses ResolveLinkEdge. Zero-overlap/corner-touch => UNRESOLVED, UE_LOG loud, carve NOTHING (connection guarantee preserved; TACS grow-room-to-anchor NOT adopted). The aperture rect is computed ONCE and subtracted from BOTH buckets (the shared OpenLo/OpenHi guarantee, generalized — they can never diverge and re-seal).
    A-axis: Weff=min(Width,Span); Center=clamp(mid+Position, SLo+Weff/2, SHi-Weff/2) (opening clamp verbatim, cpp:799-804).
    B-axis (Z) from profile: Doorway B=[FZ, FZ+Height], cap above; Window solid below Sill + clear band + cap; Passage/FullClear B=[FZ, Top], no cap; Rail => the bucket's emitted height is capped to FZ+HalfWallHeight FOR THE FACE-RECT(S) THAT THE RAIL THRESHOLD OWNS ONLY (see rail rule below — fixes the bucket-level-cap break).
  HORIZONTAL (Plane==Horizontal): the DESIGNED stair-hole resolver (the COS/TACS graft). See vertical_shell for the full algorithm; in brief: target the interface bucket at Interface = upper room's Level; derive the 2D hole rect from the flight plan; CLAMP it inside the bucket's shared/own slab XY (the horizontal opening-clamp); round the well OUT to enclose the off-grid flight extent + landing margin; subtract; and queue the SAME flight — hole and flight from one derivation.

--- RAIL RULE (fixes the 'bucket-level cap is wrong for a shared wall' break) ---
A Rail is NOT a bucket-height cap. It is a per-FACE-RECT cap applied to the SOURCE ROOM's deposited vertical face rect only: when room R has a Rail threshold on edge E, R deposits its E face rect with B=[FZ, FZ+HalfWallHeight] instead of [FZ, FZ+H]. The facing room (if any) deposits its own full-height rect into the same bucket independently. UnionOfRectangles then yields: a balcony-over-void edge (no facing room) => emitted height = HalfWallHeight (a true low rail); a shared wall where only ONE side rails => union restores full height (you cannot accidentally rail-cap a structural shared wall — the other room's full rect wins). RailGap apertures (stair landings) are subtracted from the rail rect; their A-position is DERIVED from the paired flight's CrossV, not authored Position (closes the dormant rail-gap finding).

--- PASS 3: BOOLEAN + EMIT (the provably-watertight primitive; one routine, both orientations) ---
For each bucket B:
  solid = UnionOfRectangles(B.FaceRects)
  holes = UnionOfRectangles(B.Apertures)
  remainder = solid MINUS holes  (2D rectangle Boolean -> a set of axis-aligned rects)
  Emit one AddBox of thickness Thick on the plane axis per output rect.
  For each aperture with a cap/sill profile: emit the lintel/sill bands and proud frame jambs above/below the hole within the union footprint (windows/doors).
IMPLEMENTATION of the 2D Boolean (coordinate-compression sweep): collect all A-edges and B-edges of solids+holes; build the grid of sub-cells; mark a sub-cell SOLID iff covered by >=1 face-rect AND covered by 0 apertures; greedily merge adjacent solid sub-cells into maximal rectangles for emission. O(n^2) in edges per bucket (n tiny). EXACT: no tolerance, no 3cm closest-wall search, no integer-span equality. Walls of unequal width simply union; the door subtracts one rectangle from the union.

CRITICAL: there is NO sort-merge-into-one-span step. The current EmitWall bug (cpp:917-931, 'if (O.Lo <= Merged.Last().Hi) ... union') that silently eats the pier between two openings is DESIGNED OUT: two apertures are two distinct subtracted rectangles; the sweep marks the sub-cell between them SOLID (covered by a face, covered by 0 apertures) and emits it as a pier. A belt-and-suspenders gap<T pier check nudges-apart-or-warns if two apertures would touch.

## Vertical / multi-floor model
The horizontal plane family (Class=2) is the SAME ledger as walls, so floors/ceilings get dedup, opening-awareness and partial-coverage correctness for free — closing the system map's 'single biggest gap.' The COS BaseZ invariant and the designed stair-hole resolver make every vertical case structural.

=== LEVELS + BaseZ INVARIANT (replaces FloorZ-as-only-truth) ===
Levels[] is ordered. BaseZ[n+1] = BaseZ[n] + Height[n] + SlabT. SlabT = BuiltWallT rounded UP to a whole GridSnap.Z cell (the vertical analogue of BuiltWallT; keeps level spacing grid-coherent). A room's FloorZ is its level's BaseZ; its ceiling Z is BaseZ + EffH. The slab between stacked rooms is literally ONE interface bucket at q(BaseZ[n+1]).

=== SHARED FLOOR-CEILING SLAB DEDUP (stacked rooms) ===
Lower room (Level L) deposits its ceiling rect to bucket (2, q(BaseZ[L]+EffH_L), Interface=L+1). Upper room (Level L+1) deposits its floor rect to bucket (2, q(BaseZ[L+1]), Interface=L+1). The BaseZ invariant makes q(BaseZ[L]+EffH_L) == q(BaseZ[L+1]) when EffH_L+SlabT == the level delta (asserted in Pass 0), and the Interface index is L+1 for both. ONE bucket, UnionOfRectangles emits ONE slab. No double, no z-fight, no gap. Resolves stress 'two-storey stack sharing one slab.'

=== MISALIGNED / CANTILEVER STACKS ===
Over the XY intersection both rects union to one slab. Over the upper's overhang only the upper's floor rect exists (its own floor, underside exposed/sealed). Over the lower's surplus only the lower's ceiling rect exists (its own ceiling). UnionOfRectangles never leaves a covered sub-cell unemitted and never emits an uncovered one — one pass, no special case for the offset seam. Interface index is part of the bucket key, so a wall can NEVER mis-bind across floors (fixes the legacy 2D-only abutment bug). Resolves stress 'stacked misaligned footprints.'

=== VERTICAL CONNECTIONS AS SLAB OPENINGS — the DESIGNED stair-hole-from-flight resolver ===
A Stairwell/Ramp threshold (Plane==Horizontal, RoomB = the room on the OTHER level) resolves as follows:
  1. Identify Lo room (lower FloorZ) and Hi room (upper). Target interface bucket = (2, q(BaseZ[Hi.Level]), Interface = Hi.Level) — the slab the player breaks through to ARRIVE on Hi's floor.
  2. Derive the flight plan (reusing StepCount/TotalRun/RampRun verbatim, grid-EXEMPT R4): bAlongX from the larger room gap; CrossV = overlap-center + Position2; W = Width or max(CorridorWidth, overlap); StartU/Dir from which room is lower; TotalRun = StepCount(DZ)*StepRun.
  3. Compute the TRUE flight footprint rect in XY from (StartU, Dir, TotalRun, CrossV, W) — the off-grid extent (StepRun=30 is not a 50-grid multiple).
  4. OFF-GRID-OUT RECONCILIATION (the COS graft): snap the well rect to GridSnap but ROUND OUT — Lo floored to the cell at/below the flight start, Hi ceiled to the cell at/above (flight end + one landing margin cell). The hole fully ENCLOSES the off-grid flight plus a landing margin so the top tread always lands on solid floor and never rams a quantized-in sliver. NEVER round IN.
  5. HORIZONTAL OPENING-CLAMP (the missing analogue of the vertical clamp): clamp the rounded well rect to the bucket's shared/own slab XY (intersection of Lo.footprint and Hi.footprint, expanded to whichever room actually owns slab there). The aperture can never be subtracted outside the shared region (no level-leak into a solo floor/ceiling) and a strict-reject is replaced by clamp-then-warn (never a sealed stair — the supreme invariant). If after clamping the well cannot contain a landing, UE_LOG loud and leave the slab solid is FORBIDDEN; instead the clamp guarantees a minimal landing-sized hole at the arrival end.
  6. Subtract the well from the interface bucket AND queue the SAME flight (one derivation). The flight fills the hole from Lo.FloorZ to Hi.FloorZ; the rest of the slab stays solid. Resolves stress 'stairwell/floor-opening' and 'diagonal/horizontal aperture resolution.'

=== DOUBLE-HEIGHT / ATRIUM (made structural — fixes the conceded break) ===
An Atrium threshold (Plane==Horizontal, Kind=Atrium) between a tall lower room and the void at the upper level deposits a Void aperture into the upper interface bucket over the atrium footprint. RULE: the interface bucket subtracts the Atrium aperture FIRST, then any upper-room floor rect that would cover that sub-region is REJECTED with a loud warn (the CANTILEVER GUARD) — the upper room's floor can never silently roof the void. The tall room emits full-height walls and NO mid slab there; the upper room's atrium-facing edge is a Rail threshold (fall edge, see-over) whose rect caps to HalfWallHeight by the rail rule. Closure now holds BY CONSTRUCTION (interface-keyed void + cantilever guard), not 'if the author decomposed correctly.'

=== MEZZANINE (partial floor over a tall room — fixes the conceded break) ===
A mezzanine is an upper-level room whose footprint is the mezzanine sub-rect; its floor rect covers only that sub-rect of the interface bucket; the remaining interface area carries an Atrium/Void aperture (no slab) over the tall room below. The mezzanine drop-edge is a Rail threshold; a Stairwell threshold carves the landing gap and queues the flight. The partial slab EMERGES from the footprint + the Void aperture + the cantilever guard, all interface-keyed — not from author discipline. Double-height is preserved over the void; mezzanine is standable.

=== MISMATCHED CEILING HEIGHTS (side-by-side) ===
Two rooms, same level, different EffH share a wall bucket. Each deposits its own [FZ, FZ+EffH] rect; UnionOfRectangles unions to the TALL height; the clerestory band above the short ceiling is solid on the short side. The short room's ceiling slab (Interface keyed) butts the tall wall. The door below carves the lower shared band. No clerestory leak. (Dedicated battery test V5.)

=== CEILING-HEIGHT METRIC ===
Height is now a real field at two levels: FDdLevel.Height (storey default) and FDdRoom.Height (override), with CeilingMin preserved as the floor. Headroom is QUERYABLE per room/level, not implied. Roofless top room (SurfaceFlags clears CeilBit) emits no ceiling; a ceiled top room keeps its CeilBit; a pit (clears FloorBit) emits no floor but walls + ceiling stay, edges still sealed to neighbours via the wall buckets. Resolves stress 'top room sky vs ceiled' and 'bNoFloor pit.'

=== HATCH / SKYLIGHT ===
A bounded Horizontal aperture (Kind=Hatch) in an interface or roof bucket; the surround stays solid (capped) on all four sides via the Boolean. Skylight = the aperture left open to sky. Same family as the stairwell, no flight. Resolves stress 'ceiling hatch/skylight.'

## Closure invariant + validation
=== THE INVARIANT (structural in all 3 axes) ===
A room deposits a rectangle on EVERY one of its 6 planes (4 walls unconditionally, floor+ceiling per SurfaceFlags), and the ONLY thing that removes area from a plane is an APERTURE, which exists only where a threshold was authored. Therefore: solid-by-default, holes-only-where-declared. There is no code path that leaves a face partially or wholly unemitted by accident. This kills the OpenEdgeMask failure mode (a stray bit opening a wall to the outdoors) because there is no 'open this side' bit at all; an exterior breach requires an explicit exterior threshold.

=== THE VALIDATION (a build-time self-check, loud-failure style) ===
Run on every preset and every authored layout, BEFORE UE wiring (Phase 0 pure-data) and as a build gate (Phase 2+):

ASSERTION 1 — PER-FACE WATERTIGHT-AREA. For every room, for each of its 6 intended faces (4 walls always; floor/ceiling per SurfaceFlags), compute: (emitted solid area within the room's face footprint) + (emitted aperture area within the same footprint) == full face footprint area, within 0 tolerance on the grid (areas are integer multiples of Quantum^2). A shortfall = an accidental hole (FAIL LOUD); an excess = double-cover (FAIL LOUD).

ASSERTION 2 — CLOSED-MANIFOLD / SHARED-EDGE (the COS manifold graft). Build the set of all emitted face-rect edges across all buckets. Every emitted edge must be either (a) shared by exactly TWO co-planar/adjacent solid rects (an internal seam — watertight), or (b) a boundary of an INTENTIONAL aperture (a threshold-declared hole), or (c) an outer shell boundary. An edge shared by an ODD count, or a free edge not on an aperture boundary, = a leak (FAIL LOUD with room id + plane key).

ASSERTION 3 — INTERFACE COHERENCE (the COS BaseZ graft self-check). For every slab interface bucket fed by >1 room, assert all contributors agree on q(Z) (the BaseZ invariant held) and on Interface index. A contributor whose q(Z) disagrees = a desynced stack (FAIL LOUD, points at the Height override that broke BaseZ[n+1]).

ASSERTION 4 — CONNECTION GUARANTEE. Every authored threshold resolved to >=1 bucket and deposited >=1 aperture, OR logged UNRESOLVED. Zero silent seals. A threshold with positive overlap that produced no aperture = FAIL LOUD.

ASSERTION 5 — CANTILEVER / VOID GUARD. For every interface bucket carrying an Atrium/Void aperture, assert no floor rect covers the void sub-region (the partial-floor cases cannot silently roof a void).

PARTIAL-FLOOR FLAGGING: the conceded atrium/mezzanine cases are now structural (interface-keyed void + cantilever guard), but the validator still EXPLICITLY FLAGS any interface where a void aperture and a floor rect contest the same sub-cell — surfaced, never silently degraded. This is the load-bearing-Boolean-proven-before-UE-wiring gate the charter wants.

## Unified openings
Every opening is a threshold that subtracts a rectangle from a plane bucket. They differ ONLY by (a) plane family (Vertical wall vs Horizontal slab) and (b) the cap/sill profile. This is the charter's rails/windows-unify-as-openings made structural.

=== VERTICAL (wall apertures) ===
  Doorway  = clear band [FZ, FZ+Height] + lintel cap to Top + proud frame. Weff=DoorWidth default.
  Passage  = full-clear, no cap (corridor mouth / open arch). Weff=overlap span ('open corridor mouth' = full-overlap Passage). The +T corner overhang means even a full-overlap Passage leaves >=T corner piers.
  Window   = solid below Sill, clear band [Sill, Sill+Clear], cap above (the classic 3-band punched aperture). Watertight on all four sides by rectangle subtraction.
  Rail     = the SOURCE room's face rect is deposited at HalfWallHeight (per-face, not per-bucket — see rail rule); a RailGap aperture (derived from a paired flight's CrossV) carves stair-landing notches.
  Two openings on one wall keep a pier: distinct subtracted rectangles; the Boolean keeps the solid sub-cell between them; the gap<T check nudges-or-warns. The EmitWall sort-merge bug is designed out.

=== HORIZONTAL (slab apertures) ===
  Stairwell = a 2D well rect (off-grid-out + horizontal-clamp) subtracted from the upper interface bucket; the flight fills it (one derivation).
  Ramp      = same well, pitched slab fills it.
  Hatch     = a bounded hole, cap intact on all sides.
  Skylight  = a bounded hole left open to sky (roof bucket).
  Atrium    = a Void aperture suppressing the interface slab over the atrium footprint, guarded by the cantilever check; the upper edge over the void is a Rail.

=== EXTERIOR / ENTRY (RoomB == INDEX_NONE) ===
A Vertical aperture resolved from RoomA's named ExteriorEdge + Position (not FaceConnection). The bIsEntry threshold drives NormalizeToEntry (R1): its projected world point is the origin; PlayerStart spawns in that doorway (R2). Sync refuses to delete/merge it. Because the engine now projects the anchor onto the resolved exterior plane (ProjectAnchorToPlane), an entry marker dragged slightly off-plane projects to the plane (its perpendicular component is the Stage-B reshape signal, but for an ENTRY anchor Stage B is DISABLED — an entry only slides along its edge, never grows the exterior wall, fixing the 'projection silently moves the entry wall' minor break).

## Threshold-first / Stage B integration
=== ANCHOR-AS-STORAGE (the TACS graft) + Stage B ===
ADraftDeskThreshold becomes the authoring surface AND the data, ending the threshold_points() hand-mirror (the system map's 'most fragile coupling'). Build flow:
  1. Sync reads placed threshold actors -> each maps 1:1 to an FDdThreshold record.
  2. The engine projects each actor's WORLD TRANSFORM onto its resolved plane bucket to derive Position/Position2 (ProjectAnchorToPlane — single source of truth; NO Python re-derivation). This is the new MCP query the harness calls (replaces threshold_points()).
  3. Dragging a threshold ALONG its plane moves the aperture (Position changes, aperture re-subtracts on rebuild).
  4. Dragging it PERPENDICULAR off its plane is Stage B reshape — but GATED by PBL's overlap rule (NOT TACS grow-to-anchor): the room face follows the marker (Min/Max on that axis updates so the wall plane tracks) ONLY WHEN the perpendicular component is unambiguous AND, after moving, FaceConnection still resolves a POSITIVE overlap. A zero-overlap / corner-touch reshape FAILS LOUD and carves nothing — the connection guarantee is preserved while gaining the cleaner authoring loop.
  5. SHARED-FACE-FOLLOWS-BOTH rule (fixes the Stage-B abutment break): a reshape that drags a SHARED wall propagates the boundary move to BOTH abutting rooms (so the wall is not doubled and the abutment is not un-made); a reshape of an exterior wall moves only the one room.
  6. Because openings are relational rectangles re-subtracted every rebuild, Stage B is 'move the room boundary, re-deposit, re-Boolean' — a pure function, no incremental geometry surgery.

=== VERTICAL THRESHOLDS ===
A Stairwell/Hatch/Atrium anchor stores RoomA/RoomB across levels and Plane==Horizontal. Its world XY projects to (Position, Position2) on the interface bucket; dragging it moves the well (re-derives the flight and the off-grid-out well together). A Rail anchor spanning an edge stores the rail; its RailGaps are NOT separately authored — they derive from the paired Stairwell's CrossV at build, so moving the stair moves its rail gap (closes the 'rail gaps desync from stair CrossV' break and the dormant 'rail-edge openings only fire on hand-authored graphs' finding).

=== ENTRY / R1 ===
Unchanged in intent: exactly one bIsEntry threshold normalizes to origin. Stage B is disabled for the entry anchor (slides along edge only). dd_sync still refuses to delete the entry/exterior marker (would strand NormalizeToEntry). The new ProjectAnchorToPlane query must exist BEFORE the Python mirror is retired (sequenced in migration Phase 1) so R1 seeding / PlayerStart placement never go dark.

## Correctness battery
1. H1 Equal-width abutment -> ONE shared wall: two coincident face rects union; the Doorway subtracts one rect; no sliver, no double; watertight elsewhere. (Was the cpp:572 integer-key bug.)
2. H2 Unequal-width abutment (wide room <-> narrow corridor): both faces in one bucket; UnionOfRectangles merges to one full wall; aperture at the shared-overlap center sized to the corridor mouth; piers flank. (Was double-wall.)
3. H3 Offset/partial-overlap abutment: aperture = FaceConnection overlap [max(mins),min(maxs)], subtracted from BOTH buckets as the SAME rect; non-overlap tails keep full face rects.
4. H4 T-junction: stem opens via a full-overlap Passage; cross-bar keeps its union; single aperture subtracts the stem mouth; two piers; continuous floor slab.
5. H5 4-way crossing: four Passage apertures, one per node edge; the four +/-T corner sub-cells are covered by no aperture -> four corner posts survive; floor+ceiling continuous.
6. H6 Full-width Passage vs Doorway on same wall: Weff=overlap vs Weff=DoorWidth; both leave >=T corner piers.
7. H7 Two openings on one wall keep a pier: two distinct apertures; the sub-cell between is marked SOLID; gap<T nudge/warn. (Directly fixes the cpp:917-931 sort-merge bug — verify the pier survives when the two clamped intervals are adjacent.)
8. H8 Room edge with NO neighbour stays solid: walls solid by construction; no OpenEdgeMask to set by accident; FaceConnection invents no connection across empty space.
9. H9 Window (sill+cap): Windowed aperture carves only [SillZ,CapZ]; sub-cells below sill and above cap stay solid; flanking piers; watertight on all four sides.
10. H10 Opening flush to a corner: clamp [SLo+Weff/2, SHi-Weff/2] + the Lo=Min-T overhang leaves a >=T corner pier even at the overlap end; no spurious margin shift for in-bounds openings.
11. H11 Connection authored WIDER than overlap (toowide / Position=+/-100000): Weff=min(Width,Span); the SAME OpenLo/OpenHi subtracted from both buckets so they cannot diverge and re-seal; over-wide fills exactly the overlap; corner piers survive.
12. H12 Diagonal / zero-overlap link: positive overlap across a gap -> closest facing resolves and carves both walls; corner-touch / disjoint -> UNRESOLVED, UE_LOG loud, NO hole carved; shell stays watertight.
13. H13 Entry / exterior (RoomB=-1): carved in RoomA's named edge at edge-relative Position; entry sets origin (R1) + PlayerStart (R2); never merge-deleted; off-plane drag does NOT grow the exterior wall (entry Stage-B disabled).
14. H14 L/U footprint / courtyard ring: composed from abutting AABB boxes; courtyard-facing inner edges stay solid (no threshold); the validator's shared-edge assertion proves the REFLEX inside corner is watertight (no sliver, no overhang-into-room) — the previously-unverified reflex case is now a gated test.
15. V1 Two-storey stack sharing one slab: lower ceiling (Interface=L+1) and upper floor (Interface=L+1) share ONE interface bucket; one slab; no double, no gap. (The repaired Level spec.)
16. V2 Stairwell / floor-opening: well derived from the flight, rounded OUT to enclose the off-grid extent + landing margin, clamped inside the shared slab XY; flight fills it; rest of slab solid; top tread lands on solid floor (no quantized-in sliver).
17. V3 Double-height / atrium: Atrium/Void aperture suppresses the interface slab; cantilever guard rejects any upper floor rect over the void (FAIL LOUD if authored); tall room full-height; upper edge is a rail over the void.
18. V4 Mezzanine: partial floor = upper room footprint = mezzanine sub-rect; rest of interface carries the Void aperture; drop-edge rail + Stairwell landing gap; double-height preserved; mezzanine standable.
19. V5 Different ceiling heights side-by-side: shared wall bucket unions to the TALL height; clerestory band above the short ceiling solid on the short side; short ceiling slab butts the tall wall; door carved in the lower shared band.
20. V6 Ceiling hatch / skylight: bounded Horizontal aperture; cap solid on all four sides; skylight left open.
21. V7 Stacked MISALIGNED footprints (cantilever/overhang): shared slab over the XY intersection; upper's own floor over the overhang; lower's own ceiling over the surplus; seam watertight; Interface key prevents cross-level wall mis-bind.
22. V8 Top room sky vs ceiled + pit (bNoFloor): roofless clears CeilBit (no ceiling, walls full height); ceiled keeps CeilBit; pit clears FloorBit (no floor, walls+ceiling present, edges sealed to neighbours).
23. V9 BaseZ-desync guard (new, from the COS graft): a room whose Height override breaks BaseZ[n+1] under a stack -> Assertion 3 FAILS LOUD pointing at the offending room; the slab does NOT silently double or split.
24. V10 Off-grid stair landing (new, from the reconciliation graft): DZ=300 => StepCount=17 => TotalRun=510; the flight footprint endpoints are off the 50-grid; verify the well rounds OUT (encloses 510 + a landing cell) and the top tread arrives onto solid floor — no rejected sliver, no level-leak past the shared slab.

## Migration plan
Phased, with the correctness battery as the gate. RECOMMENDATION: in-place phased migration of the geometry core (Phases 0-4 below), NOT a from-scratch greenfield repo. Rationale: the connection-guarantee machinery (FaceConnection / ResolveLinkEdge / clamp / NormalizeToEntry), the stair math (StepCount/TotalRun/RampRun), AddBox/AddColumn, the Spec/Metrics asset, and the whole dd_author/dd_castle/dd_sync surface are correct and load-bearing; only the EMIT path (BuildEdgeLedger/CarveOpenings/EmitWall/EmitFloorsAndCeilings + the integer key + the FDraftDeskStair duplication) is wrong. Greenfield would throw away proven IO and re-litigate solved invariants. Greenfield the MODULE (the FDdPlaneKey Boolean core), phase the SYSTEM.

PHASE 0 (no behavior change): land FDdPlaneKey / FDdFaceRect / FDdAperture + UnionOfRectangles + the 2D-Boolean + the 5 validator assertions as a standalone, unit-tested module driven by the 24-case battery (H1-H14, V1-V10) as PURE-DATA tests, no UE. The load-bearing Boolean core is proven before any UE wiring. Add the COS BaseZ reconciler as pure data here too (V9).

PHASE 1 (walls only, behind a flag): re-implement BuildEdgeLedger+CarveOpenings+EmitWall as DepositFaces+DepositApertures+Boolean for Class 0/1 buckets ONLY; floors/ceilings stay on the old per-room path. Diff geometry against current presets for parity on equal-width abutment, then verify the cases the old system got WRONG (H2 unequal-width, H7 two-doors-one-wall) now pass. OpenEdgeMask is still READ this phase but TRANSLATED at load into 'no exterior threshold here' (a masked-open edge becomes a Passage threshold and the mask is cleared) so dd_castle still builds. ADD the ProjectAnchorToPlane MCP query in this phase, BEFORE retiring the Python mirror (so R1/sync never go dark).

PHASE 2 (slabs + levels): route floors/ceilings through Class=2 interface buckets; introduce FDdLevel[] and the BaseZ invariant; implement stacked-slab dedup (V1), misaligned/cantilever (V7), the validator Assertion 3 (V9). Retire EmitFloorsAndCeilings.

PHASE 3 (unify verticality + the stair-hole resolver): fold FDraftDeskStair AND the Stairs/Ramp link Kind into Stairwell/Ramp thresholds; implement the DESIGNED stair-hole-from-flight resolver (V2), off-grid-out reconciliation (V10), horizontal opening-clamp, atrium/mezzanine via Void apertures + cantilever guard (V3,V4), hatch/skylight (V6). The flight emitter is reused, now driven by the horizontal aperture it pairs with — hole and flight derived together (no independent stair queue vs slab; closes the cpp:723-763 'continue with no slab carve' gap).

PHASE 4 (kill the masks + the Python mirror for good): delete OpenEdgeMask/RailEdgeMask/bCeiling/bNoFloor/FDraftDeskStair; regenerate dd_author/dd_castle. room() loses open_edges/rail_edges/ceiling/no_floor and gains level=/surface=. 'open mouth' -> explicit Passage threshold; rail -> Rail threshold; stacked rooms -> level index; the two hand-authored castle stairs + two hand-authored rail gaps collapse into TWO Stair thresholds whose RailGaps DERIVE from CrossV. threshold_points() + the FaceConnection/ResolveLinkEdge/NormalizeToEntry Python mirror are DELETED — replaced by ProjectAnchorToPlane. dd_sync simplifies to 'read threshold transforms, ask engine for Positions, write back.' dd_apply still pushes Rooms + Thresholds (+ Levels) arrays; apply order (levels before rooms before thresholds) preserved.

=== dd_castle (the 50cm two-level castle) MAPPING ===
  - GRID=50, T=50 unchanged; Z=300 becomes Levels[1].BaseZ with SlabT on a 50-grid (SlabT=50). The ground hall is 800 tall and NOT under the balcony, so no stack-conflict; the balcony sits on its own Level 1 at 300, validated against any room actually stacked beneath it.
  - hall/app/guard/weapons (floor=0) => Level 0. bal/ch/ante/bed/bath/closet/wings (floor=Z) => Level 1.
  - bal rail_edges=bit(WEST) => a Rail threshold on bal WEST edge. The two L.exterior(bal,WEST,'Open',position=-750/+750,width=300) rail gaps + the two L.stair(...,cross_v=-750/+750) flights COLLAPSE into TWO Stair thresholds whose RailGaps on bal WEST rail DERIVE from each flight CrossV (=-750/+750). The author no longer hand-syncs gap position to stair position.
  - bed height=350 vs neighbours 300 (same Level 1) => mismatched-ceiling side-by-side (V5): shared walls union to 350; clerestory solid above the 300 neighbours.
  - open_edges on corridors/nodes => Passage thresholds at corridor mouths; the grid validator stays (structural coords on-grid; stairs exempt).

## Rewrite scope + phasing
Large but bounded; greenfield on the geometry core, surgical on graph/IO.

REWRITE (delete + replace): EmitFloorsAndCeilings, BuildEdgeLedger, CarveOpenings, EmitWall, the integer ledger Key (cpp:572-573), FDraftDeskEdgeRec/FDraftDeskOpening, the FDraftDeskStairJob-vs-FDraftDeskStair duplication, the cpp:917-931 sort-merge. NEW: the FDdPlaneKey-bucketed plane-sweep module (DepositFaces / DepositApertures / BooleanEmit / UnionOfRectangles / the 5 validator assertions) — ~600-900 lines of test-first geometry; plus the FDdLevel reconciler and the stair-hole resolver (~150-250 lines). The rest of Generator.cpp SHRINKS (Rebuild goes from ~8 passes to ~5 cleaner ones).

KEEP (reuse mostly verbatim): FaceConnection / ResolveLinkEdge tolerant facing (cpp:669-715 — the connection guarantee); the opening clamp Weff/Center (cpp:799-804); StepCount/TotalRun/RampRun (cpp:134-159); EmitStairFlight/EmitRamp (cpp:959+); NormalizeToEntry (R1); SnapLayoutToGrid (now snapping the new fields + Levels; Quantum = GridSnap); AddBox/AddRotatedBox/AddColumn; the Rebuild skeleton; the Spec/Metrics asset.

DATA MODEL CHANGES: FDraftDeskRoom loses OpenEdgeMask/RailEdgeMask/bCeiling/bNoFloor, gains Level(int) + SurfaceFlags + Height-as-override. NEW FDdLevel[]. FDraftDeskLink + FDraftDeskStair + Stairs/Ramp Kind COLLAPSE into FDdThreshold (Plane, Position2, Depth, bRamp). ADraftDeskThreshold gains the graph refs. EDraftDeskLinkKind -> EDdThresholdKind (+ Passage, Rail, Stairwell, Hatch, Atrium). Metrics unchanged (CeilingMin already the headroom floor; add SlabT derivation alongside BuiltWallT).

PYTHON: dd_author room()/link()/stair() signatures change (masks -> thresholds, floor -> level, explicit stair -> Stair threshold); threshold_points() + the FaceConnection/ResolveLinkEdge/NormalizeToEntry Python mirror are DELETED (engine owns projection via ProjectAnchorToPlane); dd_sync simplifies to 'read transforms, query engine, write back.' dd_castle re-authored. Net: less Python, no duplicated geometry math.

EFFORT: Phase 0 ~1 focused block (pure data, highest value, de-risks everything). Phases 1-4 each ~1 block, sequential, each gated by its battery slice. Bounded by test-first discipline: the Boolean core is provable in isolation, so UE wiring carries low geometry risk.

## Open questions
1. SlabT vs WallThickness: should horizontal slab thickness equal BuiltWallT, or be its own metric rounded to a whole GridSnap.Z cell? The BaseZ invariant needs SlabT fixed before Levels[] can be authored. Recommend SlabT = ceil(WallThickness/GridSnap.Z)*GridSnap.Z; confirm you want floor and ceiling slabs the same thickness.
2. Level authoring ergonomics: author Levels[] explicitly (Index/BaseZ/Height/SlabT) and reference by index, or keep authoring rooms by floor=Z and let the build INFER+snap Levels (with the BaseZ invariant as the validator)? Infer is friendlier for dd_castle but reintroduces a (now-validated) inference step. Recommend explicit Levels[] in Custom mode, inference only as a convenience with a loud validator.
3. Mismatched ceiling UNDER a stack: V5 (side-by-side different heights) is legal; a Height override that breaks BaseZ[n+1] UNDER a stack is an error (V9). Confirm: per-room Height override allowed freely on the TOP level and any non-stacked room, but on a room with a room above it, Height is locked to the level delta (warn+snap if violated). Is that the rule, or should an under-stack override silently re-space the level above?
4. Atrium as its-own-room (Level SPAN LevelLo..LevelHi on FDdRoom) vs as-a-threshold (explicit Atrium threshold between a single-level-tall room and the upper void). Affects whether FDdRoom needs a LevelHi field. Recommend the explicit-threshold model unless you want true multi-level room spans.
5. Greedy maximal-rectangle merge is order-dependent and not minimal, which can inflate ISM instance count and create benign T-junction seams on large levels. Acceptable for a blockout? Or do you want a deterministic strip-decomposition (predictable count, possibly more boxes) for cleaner modular-kit handoff to GAME356?
6. ProjectAnchorToPlane transport: the new single-source-of-truth query runs engine-side over MCP. Confirm the harness round-trip latency is acceptable for the drag-sync loop, or whether a thin cached projection (recomputed only on rebuild) is wanted to keep dragging responsive.
7. Validator policy: HARD-FAIL the build (refuse to emit) or emit-with-loud-warnings? Charter says rooms-never-unfinished and fail-loud. Recommend hard-fail in Phase 0 pure-data tests, loud-warn-but-emit in the live editor so an author mid-edit is never blocked from seeing partial geometry.

## Risks
1. The 2D rectangle Boolean is now load-bearing for EVERY surface — a bug breaches watertightness everywhere at once, not just walls. MITIGATION: land it test-first in Phase 0 against the full 24-case battery as pure data, before any UE wiring; the 5 validator assertions run on every preset.
2. Coordinate-compression sweep emits more, smaller boxes than the old interval walker (a unioned offset wall can split into several rects), inflating ISM instance count. MITIGATION: greedy maximal-rectangle merge; acceptable for a blockout, but watch ISM counts on large levels (open question on merge policy).
3. The BaseZ invariant moves the failure from near-equal-Z mis-bucket to authored-level-spacing-must-be-coherent. An author setting a per-room Height that contradicts the level delta under a stack triggers warn+snap — a surprised author may not notice the snap. MITIGATION: Assertion 3 names the offending room loudly; lock under-stack Height to the level delta (open question).
4. The stair-hole-from-flight resolver couples the well to the flight; if off-grid-out rounding or the horizontal clamp is wrong, a stair either leaks past the shared slab (level breach) or lands on a sliver. MITIGATION: V2 + V10 dedicated battery tests; round OUT never IN; clamp guarantees a minimal landing-sized hole rather than rejecting (never a sealed stair).
5. Atrium/mezzanine are structural via Void apertures + cantilever guard, but require the author to model the tall volume + partial upper footprint correctly (level span or explicit Atrium threshold). A wrong footprint is caught by Assertion 5 (loud), not silently roofed — still an authoring step, not zero-effort. MITIGATION: the validator flags any void/floor contest at the same sub-cell.
6. Stage B reshape of a SHARED wall must propagate to BOTH rooms or it doubles/un-abuts. MITIGATION: the explicit shared-face-follows-both rule; a reshape that would drop overlap to zero fails loud (connection never sealed).
7. Migration window (Phase 1): OpenEdgeMask is translated while the new threshold model coexists; a layout mixing an authored mask and a new threshold on the same edge could double-open. MITIGATION: translation is EXCLUSIVE (a masked-open edge becomes a Passage and the mask is cleared at load); Phase 4 deletes the mask entirely.
8. Deleting threshold_points()/the dd_sync mirror removes the harness offline position prediction; the engine must expose ProjectAnchorToPlane over MCP or sync breaks. MITIGATION: add that query in Phase 1 BEFORE retiring the Python mirror; R1 seeding / PlayerStart placement depend on it being live first.
9. Reflex/inside-corner watertightness (L/U, courtyard) was previously unverified for the +/-T overhang rule. Assertion 2 (shared-edge) now PROVES it per-build, but if the overhang points the wrong way at a reflex corner a sliver could intrude — caught loud by the validator, but it is the subtlest convex-vs-reflex case and deserves H14 as a dedicated gate before shipping presets with concave plans.
