#include "DraftDeskGenerator.h"

#include "DraftDeskSpec.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

ADraftDeskGenerator::ADraftDeskGenerator()
{
	PrimaryActorTick.bCanEverTick = false;

	Blocks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Blocks"));
	SetRootComponent(Blocks);
	Blocks->SetMobility(EComponentMobility::Movable);

	// Engine unit cube is 100uu; instances scale it to each box size.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Blocks->SetStaticMesh(CubeMesh.Object);
	}

	// Default to the plugin's world-aligned grid (ships with draftDesk).
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GridMat(TEXT("/DraftDesk/Materials/M_DraftDeskGrid.M_DraftDeskGrid"));
	if (GridMat.Succeeded())
	{
		GridMaterial = GridMat.Object;
	}
}

void ADraftDeskGenerator::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Rebuild();
}

#if WITH_EDITOR
void ADraftDeskGenerator::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	// Live-refresh: rebuild whenever our spec asset (the single source of truth) is edited.
	if (GIsEditor && !PropertyChangedHandle.IsValid())
	{
		PropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddUObject(
			this, &ADraftDeskGenerator::HandleObjectPropertyChanged);
	}
}

void ADraftDeskGenerator::BeginDestroy()
{
	if (PropertyChangedHandle.IsValid())
	{
		FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(PropertyChangedHandle);
		PropertyChangedHandle.Reset();
	}
	Super::BeginDestroy();
}

void ADraftDeskGenerator::HandleObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent)
{
	if (Object && Object == Spec)
	{
		RerunConstructionScripts();
	}
}
#endif

void ADraftDeskGenerator::AddBox(const FVector& Center, const FVector& Size)
{
	if (!Blocks)
	{
		return;
	}
	Blocks->AddInstance(FTransform(FRotator::ZeroRotator, Center, Size / 100.f));
}

void ADraftDeskGenerator::AddXWallWithDoor(float X, float YMin, float YMax, float Height, float DoorWidth, float DoorHeight)
{
	const float HalfDoor = DoorWidth * 0.5f;
	const float Th = WallThickness;

	const float LeftLen = (-HalfDoor) - YMin;
	if (LeftLen > 1.f)
	{
		AddBox(FVector(X, (YMin + (-HalfDoor)) * 0.5f, Height * 0.5f), FVector(Th, LeftLen, Height));
	}

	const float RightLen = YMax - HalfDoor;
	if (RightLen > 1.f)
	{
		AddBox(FVector(X, (HalfDoor + YMax) * 0.5f, Height * 0.5f), FVector(Th, RightLen, Height));
	}

	const float LintelH = Height - DoorHeight;
	if (LintelH > 1.f)
	{
		AddBox(FVector(X, 0.f, DoorHeight + LintelH * 0.5f), FVector(Th, DoorWidth, LintelH));
	}
}

void ADraftDeskGenerator::Rebuild()
{
	if (!Blocks)
	{
		return;
	}

	Blocks->ClearInstances();
	Blocks->SetMaterial(0, GridMaterial);

	if (!Spec)
	{
		return; // no spec assigned -> nothing to build
	}

	const FDraftDeskMetrics& M = Spec->Metrics;
	const float Th = WallThickness;
	const float RX = RoomDepth;
	const float RY = RoomWidth;
	const float HalfY = RY * 0.5f;
	const float H = FMath::Max(M.CeilingMin, M.DoorHeight + 60.f);

	// Floor (top face at Z=0). Room extends +X from the actor origin (the entry threshold).
	AddBox(FVector(RX * 0.5f, 0.f, -Th * 0.5f), FVector(RX + 2.f * Th, RY + 2.f * Th, Th));

	// Entry wall (-X) with a metric door opening.
	AddXWallWithDoor(-Th * 0.5f, -(HalfY + Th), (HalfY + Th), H, M.DoorWidth, M.DoorHeight);

	// Far wall (+X), solid.
	AddBox(FVector(RX + Th * 0.5f, 0.f, H * 0.5f), FVector(Th, RY + 2.f * Th, H));

	// Side walls (+/-Y), solid, overlapping the corners.
	AddBox(FVector(RX * 0.5f,  (HalfY + Th * 0.5f), H * 0.5f), FVector(RX + 2.f * Th, Th, H));
	AddBox(FVector(RX * 0.5f, -(HalfY + Th * 0.5f), H * 0.5f), FVector(RX + 2.f * Th, Th, H));
}
