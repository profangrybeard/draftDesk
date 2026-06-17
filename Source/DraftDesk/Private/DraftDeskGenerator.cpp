#include "DraftDeskGenerator.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

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
}

void ADraftDeskGenerator::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Rebuild();
}

void ADraftDeskGenerator::AddBox(const FVector& Center, const FVector& Size)
{
	if (!Blocks)
	{
		return;
	}
	const FTransform T(FRotator::ZeroRotator, Center, Size / 100.f);
	Blocks->AddInstance(T);
}

void ADraftDeskGenerator::AddXWallWithDoor(float X, float YMin, float YMax, float Height)
{
	const float HalfDoor = Metrics.DoorWidth * 0.5f;
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

	const float LintelH = Height - Metrics.DoorHeight;
	if (LintelH > 1.f)
	{
		AddBox(FVector(X, 0.f, Metrics.DoorHeight + LintelH * 0.5f), FVector(Th, Metrics.DoorWidth, LintelH));
	}
}

void ADraftDeskGenerator::Rebuild()
{
	if (!Blocks)
	{
		return;
	}

	Blocks->ClearInstances();
	Blocks->SetMaterial(0, GridMaterial); // null -> mesh default material

	const float Th = WallThickness;
	const float RX = RoomDepth;
	const float RY = RoomWidth;
	const float HalfY = RY * 0.5f;
	// Wall height honours the ceiling minimum and always clears the door + a lintel.
	const float H = FMath::Max(Metrics.CeilingMin, Metrics.DoorHeight + 60.f);

	// Floor (top face at Z=0). Room extends +X from the actor origin (the entry threshold).
	AddBox(FVector(RX * 0.5f, 0.f, -Th * 0.5f), FVector(RX + 2.f * Th, RY + 2.f * Th, Th));

	// Entry wall (-X) with a metric door opening — the player enters here.
	AddXWallWithDoor(-Th * 0.5f, -(HalfY + Th), (HalfY + Th), H);

	// Far wall (+X), solid.
	AddBox(FVector(RX + Th * 0.5f, 0.f, H * 0.5f), FVector(Th, RY + 2.f * Th, H));

	// Side walls (+/-Y), solid, overlapping the corners.
	AddBox(FVector(RX * 0.5f,  (HalfY + Th * 0.5f), H * 0.5f), FVector(RX + 2.f * Th, Th, H));
	AddBox(FVector(RX * 0.5f, -(HalfY + Th * 0.5f), H * 0.5f), FVector(RX + 2.f * Th, Th, H));
}
