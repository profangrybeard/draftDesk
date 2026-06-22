// Copyright draftDesk.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "DdNavToolset.generated.h"

/** One reachability result: can the nav agent get from the queried start to this target, and how far. */
USTRUCT(BlueprintType)
struct FDdNavResult
{
	GENERATED_BODY()

	/** The queried target point (world-space cm). */
	UPROPERTY(BlueprintReadOnly, Category = "DdNav")
	FVector Target = FVector::ZeroVector;

	/** True only if a COMPLETE (non-partial) nav path exists from the start to this target. */
	UPROPERTY(BlueprintReadOnly, Category = "DdNav")
	bool bReachable = false;

	/** True if the navmesh produced only a partial path (target unreachable; path stops short). */
	UPROPERTY(BlueprintReadOnly, Category = "DdNav")
	bool bPartial = false;

	/** Path length along the navmesh (cm); -1 if no valid path. */
	UPROPERTY(BlueprintReadOnly, Category = "DdNav")
	float Length = -1.f;
};

/** Counts from a marker<->opening reconcile pass. Total = the generator's Openings count (the target). */
USTRUCT(BlueprintType)
struct FDdReconcileReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DdNav") int32 Spawned = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DdNav") int32 Moved = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DdNav") int32 Deleted = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DdNav") int32 Kept = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DdNav") int32 Total = 0;
	/** Colliding opening labels kept-first (two thresholds share a wall) — a loud build smell; 0 normally. */
	UPROPERTY(BlueprintReadOnly, Category = "DdNav") int32 Duplicates = 0;
	// SyncDrags-only buckets:
	UPROPERTY(BlueprintReadOnly, Category = "DdNav") int32 Folded = 0;    // drags folded into the model (slide)
	UPROPERTY(BlueprintReadOnly, Category = "DdNav") int32 Reshaped = 0;  // perpendicular drags deferred to 3b
	UPROPERTY(BlueprintReadOnly, Category = "DdNav") int32 Merged = 0;    // deleted markers -> threshold dissolved to Passage
	UPROPERTY(BlueprintReadOnly, Category = "DdNav") int32 PastWall = 0;  // along-drag past the wall (stored raw; carve clamps)
	UPROPERTY(BlueprintReadOnly, Category = "DdNav") int32 Rejected = 0;  // a reshape that broke the connection -> reverted
	// Room<->handle reconcile buckets (one ADraftDeskRoomHandle per AuthoredRooms entry; RoomTotal = target):
	UPROPERTY(BlueprintReadOnly, Category = "DdNav") int32 RoomSpawned = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DdNav") int32 RoomMoved = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DdNav") int32 RoomKept = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DdNav") int32 RoomDeleted = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DdNav") int32 RoomTotal = 0;
};

class ADraftDeskGenerator;

/**
 * draftDesk navigation toolset — query the LIVE navmesh, no screenshots.
 *
 * The real acceptance test for a blockout is "can the player actually walk everywhere it should?".
 * This asks the navigation system directly (the same nav the game runs on) instead of eyeballing a
 * green overlay, so reachability becomes a one-call, automatable gate.
 */
UCLASS()
class UDdNavToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/*
	 * Tests navmesh reachability from one start point to each target point, using the editor world's
	 * live navigation data. For each target: whether a COMPLETE (non-partial) path exists and its
	 * length in cm. Use this to assert a blockout is walkable end-to-end — e.g. PlayerStart -> every
	 * room centre — and to catch unreachable spaces (a stair that does not connect, an island room)
	 * that watertight geometry alone will not reveal.
	 * @param Start The world-space start point (cm), e.g. the PlayerStart / entrance.
	 * @param Targets The world-space target points (cm), e.g. each room centre.
	 * @return One FDdNavResult per target, in the same order: {Target, bReachable, bPartial, Length}.
	 */
	UFUNCTION(meta = (AICallable))
	static TArray<FDdNavResult> CheckReachability(FVector Start, const TArray<FVector>& Targets);

	/*
	 * Reconcile the editor-world ADraftDeskThreshold markers to EXACTLY match the generator's Openings
	 * array (the engine's own truth, refilled each rebuild): spawn a marker for every opening that lacks
	 * one, move any marker that drifted off its opening onto it (this erases the ~25cm pre-snap seed
	 * drift), and delete orphan/duplicate markers — leaving a strict label bijection between markers and
	 * openings. The entry marker and rail markers are never deleted on a transient unresolve (R1 / caps
	 * oscillate). Editor-world only (refuses to run during PIE). Whole pass is one undo transaction.
	 * @param GeneratorPath Full object path of the ADraftDeskGenerator (e.g. dd_config.GEN).
	 * @return {Spawned, Moved, Deleted, Kept, Total, Duplicates}.
	 */
	UFUNCTION(meta = (AICallable))
	static FDdReconcileReport ReconcileMarkers(const FString& GeneratorPath);

	/*
	 * The bidirectional drag loop. For each threshold marker the author moved (slid along its wall),
	 * fold the move into the generator's authored model as a RELATIVE delta from the marker's last
	 * reconciled home, then rebuild and re-reconcile so the marker holds and the bijection is restored.
	 * A marker dragged PERPENDICULAR off its wall is deferred (Stage-B reshape, 3b). A deleted marker
	 * dissolves its (interior) threshold to a full-clear Passage; the entry + rails + exterior openings
	 * are refused (R1). The entry is never folded (it is the normalize origin). Custom preset only;
	 * editor-world only. Whole pass is one undo transaction; ends by bumping the generator ReconcileSerial.
	 * @param GeneratorPath Full object path of the ADraftDeskGenerator (e.g. dd_config.GEN).
	 * @return {Spawned, Moved, Deleted, Kept, Total, Duplicates, Folded, Reshaped, Merged, PastWall}.
	 */
	UFUNCTION(meta = (AICallable))
	static FDdReconcileReport SyncDrags(const FString& GeneratorPath);
};
