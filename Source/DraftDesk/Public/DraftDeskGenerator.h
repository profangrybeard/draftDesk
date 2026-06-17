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
 * Builds a room -> hall -> room greybox driven by a shared UDraftDeskSpec (the single
 * source of truth: door / corridor / ceiling metrics). Editing the spec rebuilds in place.
 * The actor origin is the entry threshold (R1); the space extends along +X. Geometry has
 * collision so it is walkable in PIE.
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk|Layout", meta = (ClampMin = "200", Units = "cm"))
	float EntryRoomDepth = 600.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk|Layout", meta = (ClampMin = "200", Units = "cm"))
	float EntryRoomWidth = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk|Layout", meta = (ClampMin = "100", Units = "cm"))
	float HallLength = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk|Layout", meta = (ClampMin = "200", Units = "cm"))
	float MainRoomDepth = 1200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk|Layout", meta = (ClampMin = "200", Units = "cm"))
	float MainRoomWidth = 1200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk|Layout", meta = (ClampMin = "100", Units = "cm"))
	float MainRoomHeight = 600.f;

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

protected:
	/** Boxes (walls/floors/frames/dais) — instances of one cube mesh. */
	UPROPERTY(VisibleAnywhere, Category = "draftDesk")
	TObjectPtr<UInstancedStaticMeshComponent> Blocks;

	/** Columns — instances of one cylinder mesh. */
	UPROPERTY(VisibleAnywhere, Category = "draftDesk")
	TObjectPtr<UInstancedStaticMeshComponent> Columns;

private:
	void Rebuild();

	void AddBox(const FVector& Center, const FVector& Size);
	void AddColumn(float X, float Y, float Height, float Diameter);
	/** Wall on a plane of constant X spanning [YMin,YMax] with a centered metric door + frame. */
	void AddXWallWithDoor(float X, float YMin, float YMax, float Height, float DoorWidth, float DoorHeight);
	void AddDoorFrame(float X, float DoorWidth, float DoorHeight);

#if WITH_EDITOR
	void HandleObjectPropertyChanged(UObject* Object, struct FPropertyChangedEvent& PropertyChangedEvent);
	FDelegateHandle PropertyChangedHandle;
#endif
};
