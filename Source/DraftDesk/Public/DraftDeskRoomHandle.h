#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DraftDeskRoomHandle.generated.h"

class UBillboardComponent;

/**
 * An author-movable ROOM handle — grab the SPACE, not a connection.
 *
 * Exactly one per room (owns a room by index into the generator's AuthoredRooms). Drag it to TRANSLATE
 * the whole room: sideways moves it on the grid; up/down snaps it to the nearest existing Level. The
 * room's connections re-resolve to wherever it lands (detach + reconnect). A DISTINCT icon from a
 * threshold marker, so "move the room" reads apart from "move the door" at a glance.
 *
 * Editor-only: camera-facing icon with the move gizmo, no collision, not present in game. ALWAYS
 * author-draggable (never locked) — unlike flight markers, which are derived geometry and locked.
 */
UCLASS()
class DRAFTDESK_API ADraftDeskRoomHandle : public AActor
{
	GENERATED_BODY()

public:
	ADraftDeskRoomHandle();

	/** The room this handle owns (index into the generator's AuthoredRooms). The bijection key. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RoomHandle")
	int32 RoomIndex = INDEX_NONE;

	/** Author-facing label, e.g. "great hall". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoomHandle")
	FName Label;

	// --- engine-owned reconcile bookkeeping (set by the reconciler; do not hand-edit) ---

	/** Engine-written home (actor-local cm) after the last reconcile — the baseline a fresh drag measures from. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RoomHandle")
	FVector ReconciledLocation = FVector::ZeroVector;

	/** The generator ReconcileSerial that last stamped this handle (detects stale handles across rebuilds). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RoomHandle")
	int32 ReconcileSerial = 0;

protected:
	/** Camera-facing editor icon; the actor's movable root (select it for the gizmo). */
	UPROPERTY(VisibleAnywhere, Category = "RoomHandle")
	TObjectPtr<UBillboardComponent> Icon;
};
