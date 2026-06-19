#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DraftDeskLayout.h"
#include "DraftDeskGenerator.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UDraftDeskSpec;
struct FDraftDeskMetrics;

/** An interval-with-openings carved into one wall plane. Internal build state (not reflected). */
struct FDraftDeskOpening
{
	float Lo = 0.f;          // span on the wall's axis (cm)
	float Hi = 0.f;
	float Height = 0.f;      // clear height of the opening
	bool  bFullClear = false; // true => no lintel (open arch / stair mouth)
};

/** One wall plane in the edge ledger: spans [Lo,Hi] on a constant-plane line, carries its openings. */
struct FDraftDeskEdgeRec
{
	uint8 Axis = 0;          // 0 => constant-X (wall faces ±X), 1 => constant-Y
	float Plane = 0.f;       // the constant coordinate of the wall centreline
	float Lo = 0.f;          // wall extent on the other axis
	float Hi = 0.f;
	float BaseZ = 0.f;       // bottom of the wall
	float WallH = 0.f;       // height of the wall
	bool  bRail = false;     // emit a half-height guard rail instead
	TArray<FDraftDeskOpening> Openings;
};

/** A resolved stair / ramp flight, queued during CarveOpenings and emitted afterwards. */
struct FDraftDeskStairJob
{
	bool  bAlongX = true;    // flight runs along X (true) or Y (false)
	float StartU = 0.f;      // U coordinate where step 0 begins (the lower room's edge)
	int32 Dir = 1;           // +1 / -1 climb direction along U
	float CrossV = 0.f;      // centre across the flight
	float Z0 = 0.f;          // lower floor Z
	float Z1 = 0.f;          // upper floor Z
	float W = 0.f;           // tread width
	bool  bRamp = false;
};

/** A one-off solid box a preset wants emitted verbatim (e.g. the legacy dais). */
struct FDraftDeskBox
{
	FVector Center = FVector::ZeroVector;
	FVector Size = FVector::ZeroVector;
};

/**
 * draftDesk blockout generator.
 *
 * Expands an EDraftDeskPreset into a rooms-and-links graph, then emits a greybox: per-room floors,
 * shared-wall-deduped perimeter walls with metric openings, and metric-correct stair flights for
 * any vertical link. Driven by a shared UDraftDeskSpec (the single source of truth for metrics);
 * editing the spec or the preset rebuilds in place. The actor origin is the entry threshold (R1).
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

	/** Z gained per level in the vertical presets (Split Level, Tower). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk|Layout", meta = (ClampMin = "50", Units = "cm"))
	float FloorDelta = 300.f;

	/** Emit ceiling slabs (off by default so a top-down editor cam reads the plan). */
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

	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual void PostRegisterAllComponents() override;
	virtual void BeginDestroy() override;
#endif

	/** Number of treads needed to climb DZ within StepRise and MaxStepTraversalAngle (>= 1, or 0 if flat). */
	static int32 StepCount(float DZ, const FDraftDeskMetrics& M);
	/** Total horizontal run of a flight that climbs DZ. Single source of truth for presets + emitter. */
	static float TotalRun(float DZ, const FDraftDeskMetrics& M);

protected:
	/** Boxes (walls/floors/stairs/dais) — instances of one cube mesh. */
	UPROPERTY(VisibleAnywhere, Category = "draftDesk")
	TObjectPtr<UInstancedStaticMeshComponent> Blocks;

	/** Columns — instances of one cylinder mesh. */
	UPROPERTY(VisibleAnywhere, Category = "draftDesk")
	TObjectPtr<UInstancedStaticMeshComponent> Columns;

private:
	// --- transient build buffers (not reflected) ---
	TArray<FDraftDeskRoom> Rooms;
	TArray<FDraftDeskLink> Links;
	TArray<FDraftDeskStairJob> StairQueue;
	TArray<FDraftDeskBox> ExtraBoxes;

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

	int32 AddRoom(float MinX, float MinY, float MaxX, float MaxY, float FloorZ = 0.f, float Height = 0.f);
	void  AddLink(int32 A, int32 B, EDraftDeskLinkKind Kind, float Position = 0.f, float Width = 0.f, float Height = 0.f);
	void  AddEntry(int32 A, EDraftDeskEdge Edge, EDraftDeskLinkKind Kind = EDraftDeskLinkKind::Doorway, bool bEntry = true, float Position = 0.f);

	// --- emission passes ---
	void NormalizeToEntry(const FDraftDeskMetrics& M);
	void EmitFloorsAndCeilings(const FDraftDeskMetrics& M);
	void BuildEdgeLedger(TMap<FString, FDraftDeskEdgeRec>& Ledger, const FDraftDeskMetrics& M);
	void CarveOpenings(TMap<FString, FDraftDeskEdgeRec>& Ledger, const FDraftDeskMetrics& M);
	void EmitWall(const FDraftDeskEdgeRec& E, const FDraftDeskMetrics& M);
	void EmitStairFlight(const FDraftDeskStairJob& J, const FDraftDeskMetrics& M);
	void EmitRamp(const FDraftDeskStairJob& J, const FDraftDeskMetrics& M);
	void EmitColumns(const FDraftDeskRoom& R, const FDraftDeskMetrics& M);

	/** Resolve the (axis, plane, sharedLo, sharedHi) a link sits on. Returns false if the rooms do not abut. */
	bool ResolveLinkEdge(const FDraftDeskLink& L, const FDraftDeskMetrics& M,
		uint8& OutAxis, float& OutPlane, float& OutSharedLo, float& OutSharedHi) const;

	// --- low-level primitives ---
	void AddBox(const FVector& Center, const FVector& Size);
	void AddRotatedBox(const FVector& Center, const FVector& Size, const FRotator& Rotation);
	void AddColumn(float X, float Y, float BaseZ, float Height, float Diameter);

#if WITH_EDITOR
	void HandleObjectPropertyChanged(UObject* Object, struct FPropertyChangedEvent& PropertyChangedEvent);
	FDelegateHandle PropertyChangedHandle;
#endif
};
