#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DraftDeskMetrics.h"
#include "DraftDeskGenerator.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;

/**
 * draftDesk blockout generator.
 *
 * Drop in a level, edit Metrics in the Details panel, and the greybox rebuilds in place
 * on every property change (OnConstruction). The actor's origin is the entry threshold
 * (R1: the player enters at the origin); the room extends along +X.
 */
UCLASS()
class DRAFTDESK_API ADraftDeskGenerator : public AActor
{
	GENERATED_BODY()

public:
	ADraftDeskGenerator();

	/** Player-movement metrics that drive the layout. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk", meta = (ShowOnlyInnerProperties))
	FDraftDeskMetrics Metrics;

	/** Interior depth of the room (along +X), cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk|Layout", meta = (ClampMin = "100", Units = "cm"))
	float RoomDepth = 600.f;

	/** Interior width of the room (along Y), cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk|Layout", meta = (ClampMin = "100", Units = "cm"))
	float RoomWidth = 600.f;

	/** Wall / floor thickness, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk|Layout", meta = (ClampMin = "5", Units = "cm"))
	float WallThickness = 30.f;

	/** Grid material applied to all blocking meshes. Use a world-aligned grid so scale stays true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk|Material")
	TObjectPtr<UMaterialInterface> GridMaterial;

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	/** All greybox boxes are instances of one cube mesh, cleared and rebuilt each pass. */
	UPROPERTY(VisibleAnywhere, Category = "draftDesk")
	TObjectPtr<UInstancedStaticMeshComponent> Blocks;

private:
	void Rebuild();

	/** Add an axis-aligned box (center + full size, cm) as a cube instance. */
	void AddBox(const FVector& Center, const FVector& Size);

	/** Wall on a plane of constant X, spanning [YMin,YMax], with a centered metric door opening. */
	void AddXWallWithDoor(float X, float YMin, float YMax, float Height);
};
