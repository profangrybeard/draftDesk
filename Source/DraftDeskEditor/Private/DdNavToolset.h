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
};
