#pragma once

#include "CoreMinimal.h"
#include "DraftDeskLayout.generated.h"

/**
 * SHELL v1 layout primitives for the draftDesk room-graph engine.
 *
 * A layout is a set of Levels (ordered storeys), Rooms (axis-aligned interior footprints on a Level),
 * and Thresholds (the SINGLE opening primitive: door = passage = window = rail = stairwell = hatch =
 * atrium). Geometry is a pure function of (Levels, Rooms, Thresholds, Metrics): every room deposits a
 * solid rectangle on each of its 6 faces; a face is solid BY CONSTRUCTION and opens ONLY where a
 * threshold proves a connection. Walls grow OUTWARD from the interior extents by WallThickness/2, so
 * authored metrics (corridor width, ceiling, door) are the true clear dimensions (R4).
 *
 * There is no OpenEdgeMask and no RailEdgeMask: openness is RELATIONAL (a threshold), never a unary
 * bit. The watertight Boolean + 5 validator assertions live in the portable core (DdShellCore.h); the
 * generator converts these reflected structs into the core's plain structs at the build boundary.
 */

/** The kind of opening a Threshold carves. One primitive; differs only by plane family + cap/sill. */
UENUM(BlueprintType)
enum class EDdThresholdKind : uint8
{
	/** Walled opening with a lintel (a door). */
	Doorway   UMETA(DisplayName = "Doorway"),
	/** Full-clear gap, no lintel (open archway / corridor mouth). Width defaults to the full overlap. */
	Passage   UMETA(DisplayName = "Passage"),
	/** Sill below + clear band + lintel above (a window / embrasure / firing slit). */
	Window    UMETA(DisplayName = "Window"),
	/** The source room's face is capped to HalfWallHeight on this edge (a guard rail over a drop). */
	Rail      UMETA(DisplayName = "Rail"),
	/** A vertical shaft: carves a hole in the slab(s) between two levels and fills it with a stair flight. */
	Stairwell UMETA(DisplayName = "Stairwell"),
	/** Like Stairwell but a single pitched slab instead of treads (also settable via bRamp). */
	Ramp      UMETA(DisplayName = "Ramp"),
	/** A bounded hole in this room's ceiling slab (capped on all four sides). */
	Hatch     UMETA(DisplayName = "Hatch"),
	/** A hatch left open to the sky (roof bucket). */
	Skylight  UMETA(DisplayName = "Skylight"),
	/** Suppresses the slab above a tall room over the void footprint (double-height / mezzanine void). */
	Atrium    UMETA(DisplayName = "Atrium")
};

/** Which plane family a Threshold carves. */
UENUM(BlueprintType)
enum class EDdPlaneClass : uint8
{
	/** Carves a WALL between RoomA and RoomB (or the exterior). */
	Vertical   UMETA(DisplayName = "Vertical (wall)"),
	/** Carves the floor/ceiling SLAB between a lower and an upper room. */
	Horizontal UMETA(DisplayName = "Horizontal (slab)")
};

/** Edge identity. Used for an exterior threshold's wall edge and (legacy) presets. */
UENUM(BlueprintType)
enum class EDraftDeskEdge : uint8
{
	West  = 0, // -X
	East  = 1, // +X
	South = 2, // -Y
	North = 3  // +Y
};

/** The built-in layout the generator expands into Levels + Rooms + Thresholds. */
UENUM(BlueprintType)
enum class EDraftDeskPreset : uint8
{
	RoomHallRoom UMETA(DisplayName = "Room - Hall - Room (legacy)"),
	SingleRoom   UMETA(DisplayName = "Single Room"),
	Corridor     UMETA(DisplayName = "Corridor"),
	LBend        UMETA(DisplayName = "L-Bend"),
	TJunction    UMETA(DisplayName = "T-Junction"),
	Cross        UMETA(DisplayName = "Cross (4-way)"),
	Grid2x2      UMETA(DisplayName = "2x2 Room Grid"),
	SplitLevel   UMETA(DisplayName = "Split Level (stairwell)"),
	Tower        UMETA(DisplayName = "Tower (3-level climb)"),
	Ramp         UMETA(DisplayName = "Ramp"),
	Mezzanine    UMETA(DisplayName = "Mezzanine (atrium + balcony)"),
	/** Build from the AuthoredLevels / AuthoredRooms / AuthoredThresholds / AuthoredBoxes arrays. */
	Custom       UMETA(DisplayName = "Custom (authored)")
};

/**
 * A first-class storey. Levels are the discrete vertical truth; a room references one by index.
 * INVARIANT (reconciled + asserted at build): BaseZ[n+1] == BaseZ[n] + Height[n] + SlabT.
 */
USTRUCT(BlueprintType)
struct FDdLevel
{
	GENERATED_BODY()

	/** 0..N, contiguous, ordered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	int32 Index = 0;

	/** Top-of-floor of this level (actor-local cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk", meta = (Units = "cm"))
	float BaseZ = 0.f;

	/** Nominal clear storey height; a room's Height overrides this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk", meta = (Units = "cm"))
	float Height = 300.f;

	/** Floor/ceiling slab thickness for this level's ceiling interface. 0 => the generator's BuiltWallT. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk", meta = (Units = "cm"))
	float SlabT = 0.f;
};

/** An axis-aligned interior footprint on a Level. A full 3D shell (4 walls + floor + ceiling). */
USTRUCT(BlueprintType)
struct FDdRoom
{
	GENERATED_BODY()

	/** Interior min corner (actor-local cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	FVector2D Min = FVector2D::ZeroVector;

	/** Interior max corner; Max > Min on both axes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	FVector2D Max = FVector2D::ZeroVector;

	/** Storey index into the layout's Levels[]. The discrete key for stack dedup + leak-proof separation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	int32 Level = 0;

	/** OVERRIDE only. < 0 (default) => FloorZ := Levels[Level].BaseZ. If >= 0, reconciled to the level (warn+snap). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk", meta = (Units = "cm"))
	float FloorZ = -1.f;

	/** OVERRIDE clear interior height; 0 => Levels[Level].Height (clamped to CeilingMin). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk", meta = (Units = "cm"))
	float Height = 0.f;

	/** Emit a floor slab (clear for a pit / open drop). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	bool bFloor = true;

	/** Emit a ceiling slab (clear for a courtyard / open-to-above / roofless mezzanine). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	bool bCeil = true;

	/** Emit the legacy column grid in this room. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	bool bColumns = false;

	// --- inline helpers (not reflected) ---
	float W()  const { return Max.X - Min.X; }
	float D()  const { return Max.Y - Min.Y; }
	float CX() const { return (Min.X + Max.X) * 0.5f; }
	float CY() const { return (Min.Y + Max.Y) * 0.5f; }
};

/**
 * The single opening primitive between RoomA and RoomB (RoomB == INDEX_NONE => the exterior).
 * door = passage = window = rail = stairwell = hatch = atrium, differing only by Kind + Plane.
 */
USTRUCT(BlueprintType)
struct FDdThreshold
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	int32 RoomA = INDEX_NONE;

	/** INDEX_NONE => exterior (then ExteriorEdge selects RoomA's edge for a Vertical threshold). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	int32 RoomB = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	EDdThresholdKind Kind = EDdThresholdKind::Doorway;

	/** Vertical (wall) or Horizontal (slab). Stairwell/Ramp/Hatch/Skylight/Atrium imply Horizontal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	EDdPlaneClass Plane = EDdPlaneClass::Vertical;

	/** Signed offset along the shared interval (vertical drag axis; horizontal U axis). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	float Position = 0.f;

	/** Signed offset on the 2nd in-plane axis (horizontal openings are 2D). Unused for vertical. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	float Position2 = 0.f;

	/** Opening width; 0 => kind default (DoorWidth / full overlap for Passage / stair footprint). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk", meta = (Units = "cm"))
	float Width = 0.f;

	/** Horizontal-only: the run dimension of the hole. 0 => derived from the flight run (off-grid-out). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk", meta = (Units = "cm"))
	float Depth = 0.f;

	/** Vertical clear height; 0 => DoorHeight / WindowClearHeight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk", meta = (Units = "cm"))
	float Height = 0.f;

	/** Window sill above the floor; 0 => HalfWallHeight. (Rail uses HalfWallHeight as its top.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk", meta = (Units = "cm"))
	float Sill = 0.f;

	/** Exactly one threshold is the entry: its projected point is the actor origin (R1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	bool bIsEntry = false;

	/** Stairwell/Atrium with bRamp => a pitched slab instead of treads. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	bool bRamp = false;

	/** Which edge of RoomA the opening sits on when RoomB == INDEX_NONE and Plane == Vertical. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	EDraftDeskEdge ExteriorEdge = EDraftDeskEdge::West;
};

/**
 * An explicit stair / ramp flight — solid stepped FILL that climbs from FromZ to ToZ and LANDS at an
 * edge (a grand staircase up to a balcony), as opposed to a Stairwell threshold which pierces a slab.
 * Grid-EXEMPT (treads are StepRun-spaced, off the authoring grid — R4). Emitted via EmitStairFlight.
 */
USTRUCT(BlueprintType)
struct FDdFlight
{
	GENERATED_BODY()

	/** Flight runs along X (true) or Y (false). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	bool bAlongX = true;

	/** The U coordinate (X if bAlongX else Y) where step 0 begins (the lower edge). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk", meta = (Units = "cm"))
	float StartU = 0.f;

	/** Climb direction along U: +1 or -1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	int32 Dir = 1;

	/** Centre of the flight across its width. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk", meta = (Units = "cm"))
	float CrossV = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk", meta = (Units = "cm"))
	float FromZ = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk", meta = (Units = "cm"))
	float ToZ = 0.f;

	/** Tread width; 0 => CorridorWidth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk", meta = (Units = "cm"))
	float Width = 0.f;

	/** Emit one pitched slab instead of stacked treads. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	bool bRamp = false;
};

/** A raw solid block: dais / podium, crate, cover block, pillar, ledge lip, bridge segment. */
USTRUCT(BlueprintType)
struct FDraftDeskBlock
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	FVector Center = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	FVector Size = FVector::ZeroVector;
};
