#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DraftDeskGenerator.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UDraftDeskSpec;

/**
 * draftDesk blockout generator.
 *
 * Driven by a shared UDraftDeskSpec data asset (the single source of truth). Editing the
 * spec rebuilds the greybox in place (live-refresh listener). The actor's origin is the
 * entry threshold (R1); the room extends along +X.
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

	/** Interior depth of the room (along +X), cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk|Layout", meta = (ClampMin = "100", Units = "cm"))
	float RoomDepth = 600.f;

	/** Interior width of the room (along Y), cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk|Layout", meta = (ClampMin = "100", Units = "cm"))
	float RoomWidth = 600.f;

	/** Wall / floor thickness, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk|Layout", meta = (ClampMin = "5", Units = "cm"))
	float WallThickness = 30.f;

	/** Grid material applied to all blocking meshes (world-aligned). Defaults to the plugin grid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk|Material")
	TObjectPtr<UMaterialInterface> GridMaterial;

	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual void PostRegisterAllComponents() override;
	virtual void BeginDestroy() override;
#endif

protected:
	/** All greybox boxes are instances of one cube mesh, cleared and rebuilt each pass. */
	UPROPERTY(VisibleAnywhere, Category = "draftDesk")
	TObjectPtr<UInstancedStaticMeshComponent> Blocks;

private:
	void Rebuild();
	void AddBox(const FVector& Center, const FVector& Size);
	void AddXWallWithDoor(float X, float YMin, float YMax, float Height, float DoorWidth, float DoorHeight);

#if WITH_EDITOR
	void HandleObjectPropertyChanged(UObject* Object, struct FPropertyChangedEvent& PropertyChangedEvent);
	FDelegateHandle PropertyChangedHandle;
#endif
};
