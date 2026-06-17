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
	Blocks->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Blocks->SetCollisionProfileName(TEXT("BlockAll"));

	Columns = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Columns"));
	Columns->SetupAttachment(Blocks);
	Columns->SetMobility(EComponentMobility::Movable);
	Columns->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Columns->SetCollisionProfileName(TEXT("BlockAll"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Blocks->SetStaticMesh(CubeMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylMesh.Succeeded())
	{
		Columns->SetStaticMesh(CylMesh.Object);
	}

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
	if (Blocks)
	{
		Blocks->AddInstance(FTransform(FRotator::ZeroRotator, Center, Size / 100.f));
	}
}

void ADraftDeskGenerator::AddColumn(float X, float Y, float Height, float Diameter)
{
	if (Columns)
	{
		Columns->AddInstance(FTransform(FRotator::ZeroRotator,
			FVector(X, Y, Height * 0.5f),
			FVector(Diameter / 100.f, Diameter / 100.f, Height / 100.f)));
	}
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

void ADraftDeskGenerator::AddDoorFrame(float X, float DoorWidth, float DoorHeight)
{
	const float PostY = DoorWidth * 0.5f + 25.f;
	const float Depth = WallThickness + 40.f;
	AddBox(FVector(X, -PostY, (DoorHeight + 20.f) * 0.5f), FVector(Depth, 50.f, DoorHeight + 20.f));
	AddBox(FVector(X,  PostY, (DoorHeight + 20.f) * 0.5f), FVector(Depth, 50.f, DoorHeight + 20.f));
	AddBox(FVector(X, 0.f, DoorHeight + 25.f), FVector(Depth, DoorWidth + 100.f, 50.f));
}

void ADraftDeskGenerator::Rebuild()
{
	if (Blocks)  { Blocks->ClearInstances();  Blocks->SetMaterial(0, GridMaterial); }
	if (Columns) { Columns->ClearInstances(); Columns->SetMaterial(0, GridMaterial); }

	if (!Spec)
	{
		return; // no spec assigned -> nothing to build
	}

	const FDraftDeskMetrics& M = Spec->Metrics;
	const float T  = WallThickness;
	const float DW = M.DoorWidth;
	const float DH = M.DoorHeight;
	const float CW = M.CorridorWidth;
	const float H1 = FMath::Max(M.CeilingMin, DH + 60.f);
	const float H3 = FMath::Max(MainRoomHeight, DH + 60.f);

	const float AD = EntryRoomDepth, AW = EntryRoomWidth, AH = AW * 0.5f;
	const float HL = HallLength,     CH = CW * 0.5f;
	const float CD = MainRoomDepth,  CWd = MainRoomWidth, CH2 = CWd * 0.5f;

	const float Ax0 = 0.f,   Ax1 = AD;
	const float Hx0 = Ax1,   Hx1 = Ax1 + HL;
	const float Cx0 = Hx1,   Cx1 = Hx1 + CD;

	// ---- ENTRY ROOM (A) ----  origin (0,0,0) = entry threshold (R1)
	AddBox(FVector((Ax0 + Ax1) * 0.5f, 0.f, -T * 0.5f), FVector(AD + 2.f * T, AW + 2.f * T, T)); // floor
	AddXWallWithDoor(Ax0 - T * 0.5f, -(AH + T), AH + T, H1, DW, DH); // entry wall (-X)
	AddDoorFrame(Ax0 - T * 0.5f, DW, DH);
	AddXWallWithDoor(Ax1 + T * 0.5f, -(AH + T), AH + T, H1, DW, DH); // wall to hall (+X)
	AddDoorFrame(Ax1 + T * 0.5f, DW, DH);
	AddBox(FVector((Ax0 + Ax1) * 0.5f,  (AH + T * 0.5f), H1 * 0.5f), FVector(AD + 2.f * T, T, H1)); // +Y side
	AddBox(FVector((Ax0 + Ax1) * 0.5f, -(AH + T * 0.5f), H1 * 0.5f), FVector(AD + 2.f * T, T, H1)); // -Y side

	// ---- HALL ----
	AddBox(FVector((Hx0 + Hx1) * 0.5f, 0.f, -T * 0.5f), FVector(HL, CW + 2.f * T, T)); // floor
	AddBox(FVector((Hx0 + Hx1) * 0.5f,  (CH + T * 0.5f), H1 * 0.5f), FVector(HL, T, H1)); // +Y wall
	AddBox(FVector((Hx0 + Hx1) * 0.5f, -(CH + T * 0.5f), H1 * 0.5f), FVector(HL, T, H1)); // -Y wall

	// ---- MAIN ROOM (C) ----
	AddBox(FVector((Cx0 + Cx1) * 0.5f, 0.f, -T * 0.5f), FVector(CD + 2.f * T, CWd + 2.f * T, T)); // floor
	AddXWallWithDoor(Cx0 - T * 0.5f, -(CH2 + T), CH2 + T, H3, DW, DH); // entry from hall (-X)
	AddDoorFrame(Cx0 - T * 0.5f, DW, DH);
	AddBox(FVector(Cx1 + T * 0.5f, 0.f, H3 * 0.5f), FVector(T, CWd + 2.f * T, H3)); // back wall (+X)
	AddBox(FVector((Cx0 + Cx1) * 0.5f,  (CH2 + T * 0.5f), H3 * 0.5f), FVector(CD + 2.f * T, T, H3)); // +Y side
	AddBox(FVector((Cx0 + Cx1) * 0.5f, -(CH2 + T * 0.5f), H3 * 0.5f), FVector(CD + 2.f * T, T, H3)); // -Y side

	if (bColumns)
	{
		const float Cols[3] = { Cx0 + CD * 0.28f, Cx0 + CD * 0.5f, Cx0 + CD * 0.72f };
		const float Rows[2] = { -CWd * 0.3f, CWd * 0.3f };
		for (float cx : Cols)
		{
			for (float cy : Rows)
			{
				AddColumn(cx, cy, H3, ColumnDiameter);
			}
		}
	}

	// raised dais near the back of the main room
	const float DaisH = 80.f;
	AddBox(FVector(Cx0 + CD * 0.78f, 0.f, DaisH * 0.5f), FVector(CWd * 0.4f, CWd * 0.4f, DaisH));
}
