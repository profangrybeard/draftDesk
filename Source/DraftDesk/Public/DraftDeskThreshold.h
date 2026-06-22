#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DraftDeskLayout.h"
#include "DraftDeskThreshold.generated.h"

class UBillboardComponent;

/**
 * An author-movable threshold anchor — the authoring surface for draftDesk.
 *
 * Drag it at edit time to place a connection between spaces; on "sync" the generator reads the
 * placed thresholds and builds the layout around them (threshold = input, geometry = output).
 * Editor-only: a camera-facing icon with the standard move/rotate gizmo, no collision, not present
 * in game, no light or shadow. It draws over geometry, so it stays visible once the kit has roofs.
 */
UCLASS()
class DRAFTDESK_API ADraftDeskThreshold : public AActor
{
	GENERATED_BODY()

public:
	ADraftDeskThreshold();

	/** What kind of connection this threshold is. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Threshold")
	EDdThresholdKind Kind = EDdThresholdKind::Doorway;

	/** Vertical (wall) or Horizontal (slab). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Threshold")
	EDdPlaneClass Plane = EDdPlaneClass::Vertical;

	/** Owner room (index into the generator's rooms). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Threshold")
	int32 RoomA = INDEX_NONE;

	/** Other room; INDEX_NONE => exterior. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Threshold")
	int32 RoomB = INDEX_NONE;

	/** Clear width of the opening (cm); 0 => the spec's metric default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Threshold", meta = (ClampMin = "0", Units = "cm"))
	float Width = 0.f;

	/** Horizontal-only run dimension of the hole (cm); 0 => derived from the flight run. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Threshold", meta = (ClampMin = "0", Units = "cm"))
	float Depth = 0.f;

	/** Clear height of the opening (cm); 0 => the spec's metric default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Threshold", meta = (ClampMin = "0", Units = "cm"))
	float Height = 0.f;

	/** Window sill above the floor (cm); 0 => HalfWallHeight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Threshold", meta = (ClampMin = "0", Units = "cm"))
	float Sill = 0.f;

	/** The single R1 entry threshold (its projected point becomes the actor origin). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Threshold")
	bool bIsEntry = false;

	/** Author-facing label, e.g. "great hall -> guard". Helps read the flow graph at a glance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Threshold")
	FName Label;

	// --- engine-owned reconcile/fold bookkeeping (set by the reconciler; do not hand-edit) ---

	/** The AuthoredThresholds index this marker folds a drag into; -1 => flight/unstamped => never folded. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Threshold")
	int32 SourceThreshold = -1;

	/** Source flight index, or -1; flights are derived (grid-exempt) and never folded. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Threshold")
	int32 SourceFlight = -1;

	/** Engine-written home (actor-local cm) after the last reconcile — the baseline a fresh drag is measured from. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Threshold")
	FVector ReconciledLocation = FVector::ZeroVector;

	/** The generator ReconcileSerial that last stamped this marker (detects stale markers across rebuilds). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Threshold")
	int32 ReconcileSerial = 0;

protected:
	/** Camera-facing editor icon; the actor's movable root (select it for the gizmo). */
	UPROPERTY(VisibleAnywhere, Category = "Threshold")
	TObjectPtr<UBillboardComponent> Icon;
};
