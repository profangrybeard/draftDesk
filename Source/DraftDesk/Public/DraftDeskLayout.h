#pragma once

#include "CoreMinimal.h"
#include "DraftDeskLayout.generated.h"

/**
 * Layout primitives for the draftDesk room-graph engine.
 *
 * A layout is a set of Rooms (axis-aligned interior footprints at a FloorZ) joined by Links
 * (openings or vertical flights on a shared edge). Geometry is a pure function of (Rooms, Links,
 * Metrics): walls grow OUTWARD from the interior extents by WallThickness/2, so authored metrics
 * (corridor width, ceiling, door) are the true clear dimensions (R4). A corridor is just a thin room.
 *
 * For now the editable surface is the EDraftDeskPreset enum; the structs are transient build buffers,
 * trivially promotable to authored arrays later.
 */

/** How a Link is realised on the shared edge between two rooms (or a room and the exterior). */
UENUM(BlueprintType)
enum class EDraftDeskLinkKind : uint8
{
	/** Walled opening with a lintel (a door). */
	Doorway  UMETA(DisplayName = "Doorway"),
	/** Full-clear gap, no lintel (an open archway / corridor mouth). */
	Open     UMETA(DisplayName = "Open Arch"),
	/** Opening with a sill below and a lintel above (a window / embrasure / firing slit). */
	Window   UMETA(DisplayName = "Window"),
	/** Stacked-slab stair flight (requires a FloorZ delta between the rooms). */
	Stairs   UMETA(DisplayName = "Stairs"),
	/** Single pitched slab (requires a FloorZ delta). */
	Ramp     UMETA(DisplayName = "Ramp")
};

/** The built-in layout the generator expands into Rooms + Links. */
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
	SplitLevel   UMETA(DisplayName = "Split Level (stairs)"),
	Tower        UMETA(DisplayName = "Tower (3-level climb)"),
	Ramp         UMETA(DisplayName = "Ramp"),
	Mezzanine    UMETA(DisplayName = "Mezzanine (balcony)"),
	/** Build from the AuthoredRooms / AuthoredLinks / AuthoredStairs / AuthoredBoxes arrays. */
	Custom       UMETA(DisplayName = "Custom (authored)")
};

/** Edge identity, as a bit index. Bits compose into OpenEdgeMask / RailEdgeMask. */
UENUM()
enum class EDraftDeskEdge : uint8
{
	West  = 0, // -X
	East  = 1, // +X
	South = 2, // -Y
	North = 3  // +Y
};

/** An axis-aligned interior footprint at a floor height. Walls are derived OUTSIDE these extents. */
USTRUCT(BlueprintType)
struct FDraftDeskRoom
{
	GENERATED_BODY()

	/** Interior min corner (actor-local cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	FVector2D Min = FVector2D::ZeroVector;

	/** Interior max corner; Max > Min on both axes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	FVector2D Max = FVector2D::ZeroVector;

	/** Top-of-floor height (actor-local cm). Drives verticality. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	float FloorZ = 0.f;

	/** Clear interior height; 0 => max(CeilingMin, DoorHeight + 60). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	float Height = 0.f;

	/** Emit a ceiling slab for this room. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	bool bCeiling = false;

	/** Suppress the floor slab: a true pit / chasm / kill-floor / open drop. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	bool bNoFloor = false;

	/** Emit the legacy column grid in this room. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	bool bColumns = false;

	/** Bits (1<<EDraftDeskEdge): these edges register NO exterior wall (open corridor mouths, voids). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	int32 OpenEdgeMask = 0;

	/** Bits (1<<EDraftDeskEdge): these edges emit a HalfWallHeight guard rail instead of a full wall. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	int32 RailEdgeMask = 0;

	// --- inline helpers (not reflected) ---
	float W()  const { return Max.X - Min.X; }
	float D()  const { return Max.Y - Min.Y; }
	float CX() const { return (Min.X + Max.X) * 0.5f; }
	float CY() const { return (Min.Y + Max.Y) * 0.5f; }
};

/** A connection between RoomA and RoomB (RoomB == INDEX_NONE => the exterior). */
USTRUCT(BlueprintType)
struct FDraftDeskLink
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	int32 RoomA = INDEX_NONE;

	/** INDEX_NONE => exterior; then ExteriorEdge selects which edge of RoomA carries the opening. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	int32 RoomB = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	EDraftDeskLinkKind Kind = EDraftDeskLinkKind::Doorway;

	/** Signed offset of the opening along the shared edge from the shared-interval centre; 0 = centred. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	float Position = 0.f;

	/** Opening width; 0 => DoorWidth (Doorway) or CorridorWidth (Open/Stairs/Ramp). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	float Width = 0.f;

	/** Opening clear height; 0 => DoorHeight (Doorway) or WindowClearHeight (Window). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	float Height = 0.f;

	/** Window sill height above the floor; 0 => HalfWallHeight. Only used by Kind == Window. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	float Sill = 0.f;

	/** Exactly one Link is the entry: its threshold is translated to the actor origin (R1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	bool bIsEntry = false;

	/** Which edge of RoomA the opening sits on when RoomB == INDEX_NONE (exterior). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	EDraftDeskEdge ExteriorEdge = EDraftDeskEdge::West;
};

/**
 * An explicitly-placed stair / ramp flight, for verticality that auto-stairs-from-a-link can't
 * derive (a mezzanine over a room, a switchback, a flight that doesn't span two abutting rooms).
 * Steps are built to StepRise / StepRun within MaxStepTraversalAngle (R4).
 */
USTRUCT(BlueprintType)
struct FDraftDeskStair
{
	GENERATED_BODY()

	/** Flight runs along X (true) or Y (false). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	bool bAlongX = true;

	/** The U coordinate (X if bAlongX else Y) where step 0 begins (the lower edge). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	float StartU = 0.f;

	/** Climb direction along U: +1 or -1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	int32 Dir = 1;

	/** Centre of the flight across its width. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	float CrossV = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	float FromZ = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	float ToZ = 0.f;

	/** Tread width; 0 => CorridorWidth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
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
