#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DraftDeskLayout.h"
#include "DraftDeskGenerator.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UDraftDeskSpec;
struct FDraftDeskMetrics;

/**
 * draftDesk SHELL v1 blockout generator.
 *
 * Expands an EDraftDeskPreset (or the Authored* arrays) into Levels + Rooms + Thresholds, then defers
 * ALL geometry to the portable watertight core (DdShellCore.h): every room deposits 6 face rectangles,
 * every threshold deposits an aperture, and each per-plane bucket emits union(faces) - union(apertures)
 * via an exact 2D Boolean. The core hands back ready-to-place boxes + stair flight plans; this actor
 * just renders them. There is no per-edge wall emit, no OpenEdgeMask: a face is solid by construction
 * and opens only where a threshold proves a connection. The actor origin is the entry threshold (R1).
 */
UCLASS()
class DRAFTDESK_API ADraftDeskGenerator : public AActor
{
	GENERATED_BODY()

public:
	ADraftDeskGenerator();

	/** Single source of truth. Edit this asset's metrics and the blockout rebuilds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	TObjectPtr<UDraftDeskSpec> Spec;

	// --- Layout (level-design choices; metrics come from the Spec) ---

	/** Which built-in layout to build. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk|Layout")
	EDraftDeskPreset Preset = EDraftDeskPreset::RoomHallRoom;

	/** Nominal room footprint the presets scale from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk|Layout", meta = (ClampMin = "200", Units = "cm"))
	float CellSize = 1200.f;

	/** Length of the connecting hall in Room-Hall-Room. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk|Layout", meta = (ClampMin = "100", Units = "cm"))
	float HallLength = 1000.f;

	/** Minimum Z gained per stacked level in the vertical presets (clamped up to CeilingMin + a slab). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk|Layout", meta = (ClampMin = "50", Units = "cm"))
	float FloorDelta = 350.f;

	/** Show ceiling/roof slabs. Off by default so a top-down editor cam reads the plan; the watertight
	 *  VALIDATION always runs on the full shell regardless (this only filters what is rendered). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk|Layout")
	bool bPlaceCeilings = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk|Layout", meta = (ClampMin = "5", Units = "cm"))
	float WallThickness = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk|Layout")
	bool bColumns = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk|Layout", meta = (ClampMin = "20", Units = "cm"))
	float ColumnDiameter = 90.f;

	/** Grid material applied to all blocking meshes (world-aligned). Defaults to the plugin grid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk|Material")
	TObjectPtr<UMaterialInterface> GridMaterial;

	// --- Authored layout (used when Preset == Custom) ---
	// The room-graph as editable data: dictate a layout by filling these. Indices in Thresholds refer to
	// AuthoredRooms order; Room.Level indexes AuthoredLevels. Exactly one Threshold sets bIsEntry (R1).

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk|Author", meta = (EditCondition = "Preset == EDraftDeskPreset::Custom"))
	TArray<FDdLevel> AuthoredLevels;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk|Author", meta = (EditCondition = "Preset == EDraftDeskPreset::Custom"))
	TArray<FDdRoom> AuthoredRooms;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk|Author", meta = (EditCondition = "Preset == EDraftDeskPreset::Custom"))
	TArray<FDdThreshold> AuthoredThresholds;

	/** Explicit grand-staircase / ramp flights (fill that lands at an edge; does NOT pierce a slab). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk|Author", meta = (EditCondition = "Preset == EDraftDeskPreset::Custom"))
	TArray<FDdFlight> AuthoredFlights;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk|Author", meta = (EditCondition = "Preset == EDraftDeskPreset::Custom"))
	TArray<FDraftDeskBlock> AuthoredBoxes;

	/** The engine's emitted openings (one per resolved threshold + per flight), recorded each rebuild.
	 *  PUBLIC so the editor-side reconciler reads it. The single source of truth for where markers belong. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "draftDesk")
	TArray<FDdOpening> Openings;

	/** One anchor per room (normalized-local frame), recorded each rebuild — where the room HANDLE belongs.
	 *  PUBLIC so the editor-side reconciler reads it; index-aligned to AuthoredRooms (Custom only, else empty).
	 *  Computed here in the engine frame so the handle never re-derives normalize+snap (the frame-skew trap). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "draftDesk")
	TArray<FVector> RoomAnchors;

	/** Where a DORMANT (invalid/red-X) threshold's marker parks when its connection no longer resolves: inside
	 *  the owner room (RoomA), partway toward RoomB, so it is always on-screen + grabbable. Index-aligned to
	 *  AuthoredThresholds (normalized-local frame, Custom only). Zero = not anchorable (leave the marker put). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "draftDesk")
	TArray<FVector> DormantAnchors;

	/** Bumped once per ReconcileMarkers pass; the loop-closed proof the iterative gate asserts advanced. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "draftDesk")
	int32 ReconcileSerial = 0;

	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual void PostRegisterAllComponents() override;
	virtual void BeginDestroy() override;
#endif

	/** Number of treads needed to climb DZ within StepRise and MaxStepTraversalAngle (>= 1, or 0 if flat). */
	static int32 StepCount(float DZ, const FDraftDeskMetrics& M);
	/** Total horizontal run of a flight that climbs DZ. Single source of truth for presets + emitter. */
	static float TotalRun(float DZ, const FDraftDeskMetrics& M);
	/** Horizontal run of a ramp that climbs DZ at MaxStepTraversalAngle. Shared by preset + emitter. */
	static float RampRun(float DZ, const FDraftDeskMetrics& M);

	/** Reshape gate: true if rooms A,B still resolve a non-degenerate connection AFTER normalize+snap
	 *  (validates the geometry the rebuild actually emits, not raw AuthoredRooms). Used by the editor
	 *  reshape to accept/revert a wall move. */
	bool ReshapeGatePasses(int32 A, int32 B) const;
	/** The SAME-LEVEL rooms RoomIdx currently abuts (gap ~ T via the clamped face_connection, real overlap),
	 *  computed on raw AuthoredRooms (== post-snap on a uniform grid, like ReshapeGatePasses). The auto-gain
	 *  oracle: it agrees bit-for-bit with how a real threshold will resolve on the rebuild. */
	void AbuttingRooms(int32 RoomIdx, TArray<int32>& Out) const;
	/** True if a WALL connection (Doorway/Passage/Window) already exists between A and B (unordered pair).
	 *  The auto-gain existence guard, so re-abutting an already-connected pair adds nothing. */
	bool VerticalLinkExists(int32 A, int32 B) const;
	/** WallThickness rounded up to a whole grid cell (the built wall gap T); for the editor reshape math. */
	float GetBuiltWallT() const { return BuiltWallT; }
	/** Re-derive the frozen normalize origin on the next build (explicit reset only). */
	void ResetOrigin() { bOriginCached = false; }

protected:
	/** Boxes (walls/floors/ceilings/stairs/dais) — instances of one cube mesh. */
	UPROPERTY(VisibleAnywhere, Category = "draftDesk")
	TObjectPtr<UInstancedStaticMeshComponent> Blocks;

	/** Columns — instances of one cylinder mesh. */
	UPROPERTY(VisibleAnywhere, Category = "draftDesk")
	TObjectPtr<UInstancedStaticMeshComponent> Columns;

private:
	// --- transient build buffers (not reflected) ---
	TArray<FDdLevel> Levels;
	TArray<FDdRoom> Rooms;
	TArray<FDdThreshold> Thresholds;
	TArray<FDdFlight> Flights;
	TArray<FDraftDeskBlock> ExtraBoxes;

	// Effective per-build values (set at the top of Rebuild from the Spec's GridSnap + WallThickness):
	// BuiltWallT is WallThickness rounded UP to a whole grid cell (>= one cell); BuiltSnap is the grid.
	float BuiltWallT = 50.f;
	FVector BuiltSnap = FVector(50.f, 50.f, 50.f);

	// Frozen normalize origin (transient — not serialized): computed once per session in NormalizeToEntry,
	// then reused so edits don't translate the world. ResetOrigin() clears it.
	bool bOriginCached = false;
	float CachedDx = 0.f, CachedDy = 0.f, CachedMinZ = 0.f;

	void Rebuild();

	// --- graph construction ---
	void BuildPreset(const FDraftDeskMetrics& M);
	void BuildPreset_RoomHallRoom(const FDraftDeskMetrics& M);
	void BuildPreset_SingleRoom(const FDraftDeskMetrics& M);
	void BuildPreset_Corridor(const FDraftDeskMetrics& M);
	void BuildPreset_LBend(const FDraftDeskMetrics& M);
	void BuildPreset_TJunction(const FDraftDeskMetrics& M);
	void BuildPreset_Cross(const FDraftDeskMetrics& M);
	void BuildPreset_Grid2x2(const FDraftDeskMetrics& M);
	void BuildPreset_SplitLevel(const FDraftDeskMetrics& M);
	void BuildPreset_Tower(const FDraftDeskMetrics& M);
	void BuildPreset_Ramp(const FDraftDeskMetrics& M);
	void BuildPreset_Mezzanine(const FDraftDeskMetrics& M);
	void BuildPreset_Custom(const FDraftDeskMetrics& M);

	// graph helpers (presets author through these)
	int32 AddLevel(float BaseZ, float Height, float SlabT = 0.f);
	int32 AddRoom(float MinX, float MinY, float MaxX, float MaxY, int32 Level = 0);
	void  AddDoor(int32 A, int32 B, float Width = 0.f, float Height = 0.f, float Position = 0.f);
	void  AddPassage(int32 A, int32 B);
	void  AddExteriorDoor(int32 A, EDraftDeskEdge Edge, bool bEntry, float Width = 0.f, float Height = 0.f);
	void  AddRail(int32 A, int32 B, EDraftDeskEdge Edge);
	void  AddStairwell(int32 A, int32 B, float Width = 0.f, float Position = 0.f, float Position2 = 0.f, bool bRamp = false);
	/** Build N contiguous stacked levels (BaseZ = k*(StoreyH + SlabT)); returns level count. */
	int32 BuildStackedLevels(int32 Count, float StoreyH, float SlabT);

	// --- emission ---
	void NormalizeToEntry(const FDraftDeskMetrics& M);
	/** Snap room footprints, level/floor heights, and solids onto BuiltSnap (stairs are exempt — R4). */
	void SnapLayoutToGrid(const FDraftDeskMetrics& M);
	void EmitStairFlight(bool bAlongX, float StartU, int32 Dir, float CrossV, float Z0, float Z1, float W, const FDraftDeskMetrics& M);
	void EmitRamp(bool bAlongX, float StartU, int32 Dir, float CrossV, float Z0, float Z1, float W, const FDraftDeskMetrics& M);
	void EmitColumns(const FDdRoom& R, float FloorZ, float Height, const FDraftDeskMetrics& M);

	// --- low-level primitives ---
	void AddBox(const FVector& Center, const FVector& Size);
	void AddRotatedBox(const FVector& Center, const FVector& Size, const FRotator& Rotation);
	void AddColumn(float X, float Y, float BaseZ, float Height, float Diameter);

#if WITH_EDITOR
	void HandleObjectPropertyChanged(UObject* Object, struct FPropertyChangedEvent& PropertyChangedEvent);
	FDelegateHandle PropertyChangedHandle;
#endif
};
