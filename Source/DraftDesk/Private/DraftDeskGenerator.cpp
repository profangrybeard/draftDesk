#include "DraftDeskGenerator.h"

#include "DraftDeskSpec.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

// ---- file-local helpers --------------------------------------------------

namespace
{
	/** Resolved interior height of a room: explicit, else clamped to ceiling / door clearance. */
	float EffH(const FDraftDeskRoom& R, const FDraftDeskMetrics& M)
	{
		return R.Height > 0.f ? R.Height : FMath::Max(M.CeilingMin, M.DoorHeight + 60.f);
	}

	/** Mask bit for an edge. */
	int32 EdgeBit(EDraftDeskEdge E)
	{
		return 1 << static_cast<int32>(E);
	}
}

// ---- ctor / construction / editor plumbing (unchanged) -------------------

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

// ---- low-level primitives ------------------------------------------------

void ADraftDeskGenerator::AddBox(const FVector& Center, const FVector& Size)
{
	if (!Blocks || Size.X < 1.f || Size.Y < 1.f || Size.Z < 1.f)
	{
		return;
	}
	Blocks->AddInstance(FTransform(FRotator::ZeroRotator, Center, Size / 100.f));
}

void ADraftDeskGenerator::AddRotatedBox(const FVector& Center, const FVector& Size, const FRotator& Rotation)
{
	if (!Blocks || Size.X < 1.f || Size.Y < 1.f || Size.Z < 1.f)
	{
		return;
	}
	Blocks->AddInstance(FTransform(Rotation, Center, Size / 100.f));
}

void ADraftDeskGenerator::AddColumn(float X, float Y, float BaseZ, float Height, float Diameter)
{
	if (Columns && Height > 1.f)
	{
		Columns->AddInstance(FTransform(FRotator::ZeroRotator,
			FVector(X, Y, BaseZ + Height * 0.5f),
			FVector(Diameter / 100.f, Diameter / 100.f, Height / 100.f)));
	}
}

// ---- stair math (single source of truth for presets + emitter) -----------

int32 ADraftDeskGenerator::StepCount(float DZ, const FDraftDeskMetrics& M)
{
	if (DZ <= 1.f)
	{
		return 0;
	}
	const int32 Nr = FMath::CeilToInt(DZ / FMath::Max(M.StepRise, 1.f));
	const float TanMax = FMath::Tan(FMath::DegreesToRadians(FMath::Min(M.MaxStepTraversalAngle, 89.f)));
	const int32 Na = (TanMax > KINDA_SMALL_NUMBER) ? FMath::CeilToInt(DZ / (FMath::Max(M.StepRun, 1.f) * TanMax)) : Nr;
	return FMath::Max3(1, Nr, Na);
}

float ADraftDeskGenerator::TotalRun(float DZ, const FDraftDeskMetrics& M)
{
	return StepCount(DZ, M) * M.StepRun;
}

float ADraftDeskGenerator::RampRun(float DZ, const FDraftDeskMetrics& M)
{
	if (DZ <= 1.f)
	{
		return 0.f;
	}
	const float TanA = FMath::Tan(FMath::DegreesToRadians(FMath::Min(M.MaxStepTraversalAngle, 89.f)));
	return (TanA > KINDA_SMALL_NUMBER) ? DZ / TanA : DZ;
}

// ---- graph helpers -------------------------------------------------------

int32 ADraftDeskGenerator::AddRoom(float MinX, float MinY, float MaxX, float MaxY, float FloorZ, float Height)
{
	FDraftDeskRoom R;
	R.Min = FVector2D(MinX, MinY);
	R.Max = FVector2D(MaxX, MaxY);
	R.FloorZ = FloorZ;
	R.Height = Height;
	return Rooms.Add(R);
}

void ADraftDeskGenerator::AddLink(int32 A, int32 B, EDraftDeskLinkKind Kind, float Position, float Width, float Height)
{
	FDraftDeskLink L;
	L.RoomA = A;
	L.RoomB = B;
	L.Kind = Kind;
	L.Position = Position;
	L.Width = Width;
	L.Height = Height;
	Links.Add(L);
}

void ADraftDeskGenerator::AddEntry(int32 A, EDraftDeskEdge Edge, EDraftDeskLinkKind Kind, bool bEntry, float Position)
{
	FDraftDeskLink L;
	L.RoomA = A;
	L.RoomB = INDEX_NONE;
	L.Kind = Kind;
	L.Position = Position;
	L.bIsEntry = bEntry;
	L.ExteriorEdge = Edge;
	Links.Add(L);
}

// ---- presets -------------------------------------------------------------

void ADraftDeskGenerator::BuildPreset(const FDraftDeskMetrics& M)
{
	switch (Preset)
	{
	case EDraftDeskPreset::RoomHallRoom: BuildPreset_RoomHallRoom(M); break;
	case EDraftDeskPreset::SingleRoom:   BuildPreset_SingleRoom(M);   break;
	case EDraftDeskPreset::Corridor:     BuildPreset_Corridor(M);     break;
	case EDraftDeskPreset::LBend:        BuildPreset_LBend(M);        break;
	case EDraftDeskPreset::TJunction:    BuildPreset_TJunction(M);    break;
	case EDraftDeskPreset::Cross:        BuildPreset_Cross(M);        break;
	case EDraftDeskPreset::Grid2x2:      BuildPreset_Grid2x2(M);      break;
	case EDraftDeskPreset::SplitLevel:   BuildPreset_SplitLevel(M);   break;
	case EDraftDeskPreset::Tower:        BuildPreset_Tower(M);        break;
	case EDraftDeskPreset::Ramp:         BuildPreset_Ramp(M);         break;
	case EDraftDeskPreset::Mezzanine:    BuildPreset_Mezzanine(M);    break;
	case EDraftDeskPreset::Custom:       BuildPreset_Custom(M);       break;
	default:                             BuildPreset_RoomHallRoom(M); break;
	}
}

void ADraftDeskGenerator::BuildPreset_RoomHallRoom(const FDraftDeskMetrics& M)
{
	const float T = WallThickness;
	const float ED = CellSize * 0.5f; // entry room depth/width (legacy 600 at CellSize 1200)
	const float MD = CellSize;        // main room depth/width (legacy 1200)
	const float HL = HallLength;
	const float CW = M.CorridorWidth;

	const int32 A = AddRoom(0.f, -ED * 0.5f, ED, ED * 0.5f);
	const int32 H = AddRoom(ED + T, -CW * 0.5f, ED + T + HL, CW * 0.5f);
	Rooms[H].OpenEdgeMask = EdgeBit(EDraftDeskEdge::West) | EdgeBit(EDraftDeskEdge::East);
	const float Cx0 = ED + T + HL + T;
	const int32 C = AddRoom(Cx0, -MD * 0.5f, Cx0 + MD, MD * 0.5f);
	Rooms[C].bColumns = true;

	AddEntry(A, EDraftDeskEdge::West, EDraftDeskLinkKind::Doorway, /*bEntry*/ true);
	AddLink(A, H, EDraftDeskLinkKind::Doorway);
	AddLink(H, C, EDraftDeskLinkKind::Doorway);

	// raised dais near the back of the main room (legacy character piece)
	const float DaisH = 80.f;
	FDraftDeskBlock Dais;
	Dais.Center = FVector(Cx0 + MD * 0.78f, 0.f, DaisH * 0.5f);
	Dais.Size = FVector(MD * 0.4f, MD * 0.4f, DaisH);
	ExtraBoxes.Add(Dais);
}

void ADraftDeskGenerator::BuildPreset_SingleRoom(const FDraftDeskMetrics& M)
{
	const int32 R = AddRoom(0.f, -CellSize * 0.5f, CellSize, CellSize * 0.5f);
	AddEntry(R, EDraftDeskEdge::West, EDraftDeskLinkKind::Doorway, true);
}

void ADraftDeskGenerator::BuildPreset_Corridor(const FDraftDeskMetrics& M)
{
	const float CW = M.CorridorWidth;
	const int32 R = AddRoom(0.f, -CW * 0.5f, 3.f * CellSize, CW * 0.5f);
	AddEntry(R, EDraftDeskEdge::West, EDraftDeskLinkKind::Doorway, true);
	AddEntry(R, EDraftDeskEdge::East, EDraftDeskLinkKind::Doorway, false);
}

void ADraftDeskGenerator::BuildPreset_LBend(const FDraftDeskMetrics& M)
{
	const float C = CellSize;
	const float T = WallThickness;
	const float CW = M.CorridorWidth;

	const int32 HArm = AddRoom(0.f, -CW * 0.5f, 2.f * C, CW * 0.5f);
	const int32 Node = AddRoom(2.f * C + T, -CW * 0.5f, 2.f * C + T + CW, CW * 0.5f);
	const int32 VArm = AddRoom(2.f * C + T, CW * 0.5f + T, 2.f * C + T + CW, CW * 0.5f + T + 2.f * C);

	Rooms[HArm].OpenEdgeMask |= EdgeBit(EDraftDeskEdge::East);
	Rooms[Node].OpenEdgeMask |= EdgeBit(EDraftDeskEdge::West) | EdgeBit(EDraftDeskEdge::North);
	Rooms[VArm].OpenEdgeMask |= EdgeBit(EDraftDeskEdge::South);

	AddEntry(HArm, EDraftDeskEdge::West, EDraftDeskLinkKind::Doorway, true);
	AddEntry(VArm, EDraftDeskEdge::North, EDraftDeskLinkKind::Doorway, false);
}

void ADraftDeskGenerator::BuildPreset_TJunction(const FDraftDeskMetrics& M)
{
	const float C = CellSize;
	const float T = WallThickness;
	const float CW = M.CorridorWidth;

	const int32 Main = AddRoom(0.f, -CW * 0.5f, 3.f * C, CW * 0.5f);
	const int32 Branch = AddRoom(1.5f * C - CW * 0.5f, CW * 0.5f + T, 1.5f * C + CW * 0.5f, CW * 0.5f + T + 2.f * C);
	Rooms[Branch].OpenEdgeMask |= EdgeBit(EDraftDeskEdge::South);

	AddEntry(Main, EDraftDeskEdge::West, EDraftDeskLinkKind::Doorway, true);
	AddEntry(Main, EDraftDeskEdge::East, EDraftDeskLinkKind::Doorway, false);
	AddEntry(Branch, EDraftDeskEdge::North, EDraftDeskLinkKind::Doorway, false);
	// full-clear mouth carved into the main corridor's north wall
	AddLink(Main, Branch, EDraftDeskLinkKind::Open);
}

void ADraftDeskGenerator::BuildPreset_Cross(const FDraftDeskMetrics& M)
{
	const float C = CellSize;
	const float T = WallThickness;
	const float CW = M.CorridorWidth;
	const float Hh = CW * 0.5f;

	const int32 WArm = AddRoom(0.f, -Hh, 2.f * C, Hh);
	const float Nx0 = 2.f * C + T;
	const int32 Node = AddRoom(Nx0, -Hh, Nx0 + CW, Hh);
	const float Ex0 = Nx0 + CW + T;
	const int32 EArm = AddRoom(Ex0, -Hh, Ex0 + 2.f * C, Hh);
	const int32 SArm = AddRoom(Nx0, -Hh - T - 2.f * C, Nx0 + CW, -Hh - T);
	const int32 NArm = AddRoom(Nx0, Hh + T, Nx0 + CW, Hh + T + 2.f * C);

	Rooms[WArm].OpenEdgeMask |= EdgeBit(EDraftDeskEdge::East);
	Rooms[EArm].OpenEdgeMask |= EdgeBit(EDraftDeskEdge::West);
	Rooms[SArm].OpenEdgeMask |= EdgeBit(EDraftDeskEdge::North);
	Rooms[NArm].OpenEdgeMask |= EdgeBit(EDraftDeskEdge::South);
	Rooms[Node].OpenEdgeMask |= EdgeBit(EDraftDeskEdge::West) | EdgeBit(EDraftDeskEdge::East)
		| EdgeBit(EDraftDeskEdge::South) | EdgeBit(EDraftDeskEdge::North);

	AddEntry(WArm, EDraftDeskEdge::West, EDraftDeskLinkKind::Doorway, true);
	AddEntry(EArm, EDraftDeskEdge::East, EDraftDeskLinkKind::Doorway, false);
	AddEntry(SArm, EDraftDeskEdge::South, EDraftDeskLinkKind::Doorway, false);
	AddEntry(NArm, EDraftDeskEdge::North, EDraftDeskLinkKind::Doorway, false);
}

void ADraftDeskGenerator::BuildPreset_Grid2x2(const FDraftDeskMetrics& M)
{
	const float C = CellSize;
	const float T = WallThickness;

	const int32 SW = AddRoom(0.f, -C, C, 0.f);
	const int32 SE = AddRoom(C + T, -C, 2.f * C + T, 0.f);
	const int32 NW = AddRoom(0.f, T, C, C + T);
	const int32 NE = AddRoom(C + T, T, 2.f * C + T, C + T);

	AddEntry(SW, EDraftDeskEdge::West, EDraftDeskLinkKind::Doorway, true);
	AddLink(SW, SE, EDraftDeskLinkKind::Doorway); // shared X plane -> one wall
	AddLink(NW, NE, EDraftDeskLinkKind::Doorway);
	AddLink(SW, NW, EDraftDeskLinkKind::Doorway); // shared Y plane -> one wall
	AddLink(SE, NE, EDraftDeskLinkKind::Doorway);
}

void ADraftDeskGenerator::BuildPreset_SplitLevel(const FDraftDeskMetrics& M)
{
	const float C = CellSize;
	const float T = WallThickness;
	const float Run = TotalRun(FloorDelta, M);

	const int32 R1 = AddRoom(0.f, -C * 0.5f, C, C * 0.5f, 0.f);
	const float R2x0 = C + T + Run; // gap = T + Run so the upper floor is the implicit flush landing
	const int32 R2 = AddRoom(R2x0, -C * 0.5f, R2x0 + C, C * 0.5f, FloorDelta);

	Rooms[R1].OpenEdgeMask |= EdgeBit(EDraftDeskEdge::East);
	Rooms[R2].OpenEdgeMask |= EdgeBit(EDraftDeskEdge::West);

	AddEntry(R1, EDraftDeskEdge::West, EDraftDeskLinkKind::Doorway, true);
	AddLink(R1, R2, EDraftDeskLinkKind::Stairs);
}

void ADraftDeskGenerator::BuildPreset_Tower(const FDraftDeskMetrics& M)
{
	const float C = CellSize;
	const float T = WallThickness;
	const float Run = TotalRun(FloorDelta, M);
	const int32 NLevels = 3;

	float X = 0.f;
	float Z = 0.f;
	int32 Prev = INDEX_NONE;
	for (int32 K = 0; K < NLevels; ++K)
	{
		const int32 R = AddRoom(X, -C * 0.5f, X + C, C * 0.5f, Z);
		if (Prev != INDEX_NONE)
		{
			Rooms[R].OpenEdgeMask |= EdgeBit(EDraftDeskEdge::West);
		}
		if (K < NLevels - 1)
		{
			Rooms[R].OpenEdgeMask |= EdgeBit(EDraftDeskEdge::East);
		}

		if (Prev == INDEX_NONE)
		{
			AddEntry(R, EDraftDeskEdge::West, EDraftDeskLinkKind::Doorway, true);
		}
		else
		{
			AddLink(Prev, R, EDraftDeskLinkKind::Stairs);
		}

		Prev = R;
		X += C + T + Run;
		Z += FloorDelta;
	}
}

void ADraftDeskGenerator::BuildPreset_Ramp(const FDraftDeskMetrics& M)
{
	const float C = CellSize;
	const float T = WallThickness;
	const float Run = RampRun(FloorDelta, M);

	const int32 R1 = AddRoom(0.f, -C * 0.5f, C, C * 0.5f, 0.f);
	const float R2x0 = C + T + Run; // gap sized to the ramp run so the upper floor is the flush top
	const int32 R2 = AddRoom(R2x0, -C * 0.5f, R2x0 + C, C * 0.5f, FloorDelta);

	Rooms[R1].OpenEdgeMask |= EdgeBit(EDraftDeskEdge::East);
	Rooms[R2].OpenEdgeMask |= EdgeBit(EDraftDeskEdge::West);

	AddEntry(R1, EDraftDeskEdge::West, EDraftDeskLinkKind::Doorway, true);
	AddLink(R1, R2, EDraftDeskLinkKind::Ramp);
}

void ADraftDeskGenerator::BuildPreset_Mezzanine(const FDraftDeskMetrics& M)
{
	const float C = CellSize;
	const float T = WallThickness;
	const float CW = M.CorridorWidth;
	const float Run = TotalRun(FloorDelta, M);

	// tall main room
	const int32 Big = AddRoom(0.f, -C, 2.f * C, C, 0.f, FMath::Max(2.f * M.CeilingMin, FloorDelta + M.CeilingMin));

	// balcony over the back third, raised; its West edge overlooks the room (guard rail),
	// its E/N/S edges coincide with the big room's outer walls (no double wall).
	const float BalX0 = 1.4f * C;
	const int32 Bal = AddRoom(BalX0, -C, 2.f * C, C, FloorDelta);
	Rooms[Bal].OpenEdgeMask |= EdgeBit(EDraftDeskEdge::East) | EdgeBit(EDraftDeskEdge::North) | EdgeBit(EDraftDeskEdge::South);
	Rooms[Bal].RailEdgeMask |= EdgeBit(EDraftDeskEdge::West);

	AddEntry(Big, EDraftDeskEdge::West, EDraftDeskLinkKind::Doorway, true);
	// a centred gap in the balcony's West rail where the stair lands
	AddEntry(Bal, EDraftDeskEdge::West, EDraftDeskLinkKind::Open, false);

	// explicit stair climbing +X to the balcony front, centred on Y
	FDraftDeskStairJob J;
	J.bAlongX = true; J.Dir = 1;
	J.StartU = BalX0 - Run;
	J.CrossV = 0.f;
	J.Z0 = 0.f; J.Z1 = FloorDelta;
	J.W = CW;
	StairQueue.Add(J);
}

void ADraftDeskGenerator::BuildPreset_Custom(const FDraftDeskMetrics& M)
{
	Rooms = AuthoredRooms;
	Links = AuthoredLinks;
	ExtraBoxes = AuthoredBoxes;
	for (const FDraftDeskStair& S : AuthoredStairs)
	{
		FDraftDeskStairJob J;
		J.bAlongX = S.bAlongX;
		J.StartU = S.StartU;
		J.Dir = (S.Dir >= 0 ? 1 : -1);
		J.CrossV = S.CrossV;
		J.Z0 = S.FromZ;
		J.Z1 = S.ToZ;
		J.W = S.Width > 0.f ? S.Width : M.CorridorWidth;
		J.bRamp = S.bRamp;
		StairQueue.Add(J);
	}
}

// ---- emission ------------------------------------------------------------

void ADraftDeskGenerator::NormalizeToEntry(const FDraftDeskMetrics& M)
{
	if (Rooms.Num() == 0)
	{
		return;
	}

	float Dx = 0.f;
	float Dy = 0.f;
	for (const FDraftDeskLink& L : Links)
	{
		if (!L.bIsEntry)
		{
			continue;
		}
		uint8 Axis = 0;
		float Plane = 0.f, SLo = 0.f, SHi = 0.f;
		if (ResolveLinkEdge(L, M, Axis, Plane, SLo, SHi))
		{
			const float Along = (SLo + SHi) * 0.5f + L.Position;
			if (Axis == 0) { Dx = Plane; Dy = Along; }
			else { Dx = Along; Dy = Plane; }
			break; // keep scanning if this entry link didn't resolve
		}
	}

	float MinZ = Rooms[0].FloorZ;
	for (const FDraftDeskRoom& R : Rooms)
	{
		MinZ = FMath::Min(MinZ, R.FloorZ);
	}

	for (FDraftDeskRoom& R : Rooms)
	{
		R.Min.X -= Dx; R.Max.X -= Dx;
		R.Min.Y -= Dy; R.Max.Y -= Dy;
		R.FloorZ -= MinZ;
	}
	for (FDraftDeskBlock& B : ExtraBoxes)
	{
		B.Center.X -= Dx; B.Center.Y -= Dy; B.Center.Z -= MinZ;
	}
	// explicit stairs queued by a preset (Mezzanine / Custom) are pre-normalize; auto-stairs from
	// links are added later in normalized space, so only translate what is already queued here.
	for (FDraftDeskStairJob& J : StairQueue)
	{
		if (J.bAlongX) { J.StartU -= Dx; J.CrossV -= Dy; }
		else { J.StartU -= Dy; J.CrossV -= Dx; }
		J.Z0 -= MinZ; J.Z1 -= MinZ;
	}
}

void ADraftDeskGenerator::EmitFloorsAndCeilings(const FDraftDeskMetrics& M)
{
	const float T = WallThickness;
	for (const FDraftDeskRoom& R : Rooms)
	{
		if (R.W() <= 1.f || R.D() <= 1.f)
		{
			continue;
		}
		if (!R.bNoFloor)
		{
			AddBox(FVector(R.CX(), R.CY(), R.FloorZ - T * 0.5f), FVector(R.W() + 2.f * T, R.D() + 2.f * T, T));
		}
		if (R.bCeiling || bPlaceCeilings)
		{
			const float H = EffH(R, M);
			AddBox(FVector(R.CX(), R.CY(), R.FloorZ + H + T * 0.5f), FVector(R.W() + 2.f * T, R.D() + 2.f * T, T));
		}
	}
}

void ADraftDeskGenerator::BuildEdgeLedger(TMap<FString, FDraftDeskEdgeRec>& Ledger, const FDraftDeskMetrics& M)
{
	const float T = WallThickness;

	auto Reg = [&](uint8 Axis, float Plane, float Lo, float Hi, float BaseZ, float WallH, bool bRail)
	{
		const FString Key = FString::Printf(TEXT("%d|%d|%d|%d"), static_cast<int32>(Axis),
			FMath::RoundToInt(Plane), FMath::RoundToInt(Lo), FMath::RoundToInt(Hi));
		if (FDraftDeskEdgeRec* Ex = Ledger.Find(Key))
		{
			const float Top = FMath::Max(Ex->BaseZ + Ex->WallH, BaseZ + WallH);
			Ex->BaseZ = FMath::Min(Ex->BaseZ, BaseZ);
			Ex->WallH = Top - Ex->BaseZ;
			Ex->bRail = Ex->bRail && bRail; // a shared wall is solid unless BOTH sides want a rail
		}
		else
		{
			FDraftDeskEdgeRec E;
			E.Axis = Axis; E.Plane = Plane; E.Lo = Lo; E.Hi = Hi;
			E.BaseZ = BaseZ; E.WallH = WallH; E.bRail = bRail;
			Ledger.Add(Key, E);
		}
	};

	for (const FDraftDeskRoom& R : Rooms)
	{
		if (R.W() <= 1.f || R.D() <= 1.f)
		{
			continue;
		}
		const float H = EffH(R, M);
		const float YLo = R.Min.Y - T, YHi = R.Max.Y + T;
		const float XLo = R.Min.X - T, XHi = R.Max.X + T;

		if (!(R.OpenEdgeMask & EdgeBit(EDraftDeskEdge::West)))
		{
			Reg(0, R.Min.X - T * 0.5f, YLo, YHi, R.FloorZ, H, (R.RailEdgeMask & EdgeBit(EDraftDeskEdge::West)) != 0);
		}
		if (!(R.OpenEdgeMask & EdgeBit(EDraftDeskEdge::East)))
		{
			Reg(0, R.Max.X + T * 0.5f, YLo, YHi, R.FloorZ, H, (R.RailEdgeMask & EdgeBit(EDraftDeskEdge::East)) != 0);
		}
		if (!(R.OpenEdgeMask & EdgeBit(EDraftDeskEdge::South)))
		{
			Reg(1, R.Min.Y - T * 0.5f, XLo, XHi, R.FloorZ, H, (R.RailEdgeMask & EdgeBit(EDraftDeskEdge::South)) != 0);
		}
		if (!(R.OpenEdgeMask & EdgeBit(EDraftDeskEdge::North)))
		{
			Reg(1, R.Max.Y + T * 0.5f, XLo, XHi, R.FloorZ, H, (R.RailEdgeMask & EdgeBit(EDraftDeskEdge::North)) != 0);
		}
	}
}

bool ADraftDeskGenerator::ResolveLinkEdge(const FDraftDeskLink& L, const FDraftDeskMetrics& M,
	uint8& OutAxis, float& OutPlane, float& OutSharedLo, float& OutSharedHi) const
{
	const float T = WallThickness;
	const float Eps = 1.f;
	// Authored data can be momentarily inconsistent (the property system applies arrays one at a
	// time, each triggering a rebuild), so never trust a link's room indices without bounds-checking.
	if (L.RoomA < 0 || L.RoomA >= Rooms.Num())
	{
		return false;
	}
	const FDraftDeskRoom& A = Rooms[L.RoomA];

	if (L.RoomB == INDEX_NONE)
	{
		switch (L.ExteriorEdge)
		{
		case EDraftDeskEdge::West:  OutAxis = 0; OutPlane = A.Min.X - T * 0.5f; OutSharedLo = A.Min.Y; OutSharedHi = A.Max.Y; break;
		case EDraftDeskEdge::East:  OutAxis = 0; OutPlane = A.Max.X + T * 0.5f; OutSharedLo = A.Min.Y; OutSharedHi = A.Max.Y; break;
		case EDraftDeskEdge::South: OutAxis = 1; OutPlane = A.Min.Y - T * 0.5f; OutSharedLo = A.Min.X; OutSharedHi = A.Max.X; break;
		case EDraftDeskEdge::North: OutAxis = 1; OutPlane = A.Max.Y + T * 0.5f; OutSharedLo = A.Min.X; OutSharedHi = A.Max.X; break;
		default: return false;
		}
		return true;
	}

	if (L.RoomB < 0 || L.RoomB >= Rooms.Num())
	{
		return false;
	}
	const FDraftDeskRoom& B = Rooms[L.RoomB];
	if (FMath::Abs(A.Max.X + T - B.Min.X) < Eps) { OutAxis = 0; OutPlane = A.Max.X + T * 0.5f; }
	else if (FMath::Abs(B.Max.X + T - A.Min.X) < Eps) { OutAxis = 0; OutPlane = B.Max.X + T * 0.5f; }
	else if (FMath::Abs(A.Max.Y + T - B.Min.Y) < Eps) { OutAxis = 1; OutPlane = A.Max.Y + T * 0.5f; }
	else if (FMath::Abs(B.Max.Y + T - A.Min.Y) < Eps) { OutAxis = 1; OutPlane = B.Max.Y + T * 0.5f; }
	else { return false; }

	if (OutAxis == 0)
	{
		OutSharedLo = FMath::Max(A.Min.Y, B.Min.Y);
		OutSharedHi = FMath::Min(A.Max.Y, B.Max.Y);
	}
	else
	{
		OutSharedLo = FMath::Max(A.Min.X, B.Min.X);
		OutSharedHi = FMath::Min(A.Max.X, B.Max.X);
	}
	return true;
}

void ADraftDeskGenerator::CarveOpenings(TMap<FString, FDraftDeskEdgeRec>& Ledger, const FDraftDeskMetrics& M)
{
	for (const FDraftDeskLink& L : Links)
	{
		// Vertical links: queue a flight from the room geometry directly (the rooms are
		// separated by the run, so they don't abut — ResolveLinkEdge would reject them).
		if ((L.Kind == EDraftDeskLinkKind::Stairs || L.Kind == EDraftDeskLinkKind::Ramp) && L.RoomB != INDEX_NONE)
		{
			if (L.RoomA < 0 || L.RoomA >= Rooms.Num() || L.RoomB < 0 || L.RoomB >= Rooms.Num())
			{
				continue; // skip a link with stale/out-of-range room indices (mid-edit authored data)
			}
			const FDraftDeskRoom& A = Rooms[L.RoomA];
			const FDraftDeskRoom& B = Rooms[L.RoomB];
			const FDraftDeskRoom& Lo = (A.FloorZ <= B.FloorZ) ? A : B;
			const FDraftDeskRoom& Hi = (A.FloorZ <= B.FloorZ) ? B : A;

			FDraftDeskStairJob J;
			J.bRamp = (L.Kind == EDraftDeskLinkKind::Ramp);
			J.Z0 = Lo.FloorZ;
			J.Z1 = Hi.FloorZ;

			const float GapX = FMath::Max(B.Min.X - A.Max.X, A.Min.X - B.Max.X);
			const float GapY = FMath::Max(B.Min.Y - A.Max.Y, A.Min.Y - B.Max.Y);
			if (GapX >= GapY)
			{
				J.bAlongX = true;
				const float CLo = FMath::Max(A.Min.Y, B.Min.Y);
				const float CHi = FMath::Min(A.Max.Y, B.Max.Y);
				J.CrossV = (CLo + CHi) * 0.5f + L.Position;
				J.W = L.Width > 0.f ? L.Width : FMath::Max(M.CorridorWidth, CHi - CLo); // fill the opening
				if (Hi.Min.X > Lo.Max.X) { J.StartU = Lo.Max.X; J.Dir = 1; }
				else { J.StartU = Lo.Min.X; J.Dir = -1; }
			}
			else
			{
				J.bAlongX = false;
				const float CLo = FMath::Max(A.Min.X, B.Min.X);
				const float CHi = FMath::Min(A.Max.X, B.Max.X);
				J.CrossV = (CLo + CHi) * 0.5f + L.Position;
				J.W = L.Width > 0.f ? L.Width : FMath::Max(M.CorridorWidth, CHi - CLo); // fill the opening
				if (Hi.Min.Y > Lo.Max.Y) { J.StartU = Lo.Max.Y; J.Dir = 1; }
				else { J.StartU = Lo.Min.Y; J.Dir = -1; }
			}
			StairQueue.Add(J);
			continue;
		}

		// Horizontal links: carve an opening into the wall at the shared edge.
		uint8 Axis = 0;
		float Plane = 0.f, SLo = 0.f, SHi = 0.f;
		if (!ResolveLinkEdge(L, M, Axis, Plane, SLo, SHi))
		{
			continue;
		}

		FDraftDeskEdgeRec* Edge = nullptr;
		for (auto& KV : Ledger)
		{
			FDraftDeskEdgeRec& E = KV.Value;
			if (E.Axis != Axis || FMath::Abs(E.Plane - Plane) > 2.f)
			{
				continue;
			}
			if (E.Hi <= SLo || E.Lo >= SHi) // require positive overlap (reject coplanar walls that merely touch)
			{
				continue;
			}
			Edge = &E;
			break;
		}
		if (!Edge)
		{
			continue; // open joint with no wall to carve
		}

		const bool bWindow = (L.Kind == EDraftDeskLinkKind::Window);
		const float W = L.Width > 0.f ? L.Width
			: (L.Kind == EDraftDeskLinkKind::Doorway || bWindow ? M.DoorWidth : M.CorridorWidth);

		float Center = (SLo + SHi) * 0.5f + L.Position;
		Center = FMath::Clamp(Center, Edge->Lo + WallThickness + W * 0.5f, Edge->Hi - WallThickness - W * 0.5f);

		FDraftDeskOpening O;
		O.Lo = Center - W * 0.5f;
		O.Hi = Center + W * 0.5f;
		if (bWindow && !Edge->bRail) // a window on a half-height rail is incoherent -> plain gap (else branch)
		{
			O.SillZ = L.Sill > 0.f ? L.Sill : M.HalfWallHeight;
			const float Clear = L.Height > 0.f ? L.Height : M.WindowClearHeight;
			O.Height = O.SillZ + Clear; // top of the clear band; lintel sits above
			O.bFullClear = false;       // sill below + lintel above
		}
		else
		{
			O.Height = L.Height > 0.f ? L.Height : M.DoorHeight;
			O.bFullClear = (L.Kind != EDraftDeskLinkKind::Doorway); // Open / window-on-rail / stair mouths are full-clear
		}
		Edge->Openings.Add(O);
	}
}

void ADraftDeskGenerator::EmitWall(const FDraftDeskEdgeRec& E, const FDraftDeskMetrics& M)
{
	const float T = WallThickness;

	auto Solid = [&](float ULo, float UHi, float ZLo, float ZHi)
	{
		const float Len = UHi - ULo;
		const float H = ZHi - ZLo;
		if (Len <= 1.f || H <= 1.f)
		{
			return;
		}
		const FVector C = (E.Axis == 0)
			? FVector(E.Plane, (ULo + UHi) * 0.5f, (ZLo + ZHi) * 0.5f)
			: FVector((ULo + UHi) * 0.5f, E.Plane, (ZLo + ZHi) * 0.5f);
		const FVector S = (E.Axis == 0) ? FVector(T, Len, H) : FVector(Len, T, H);
		AddBox(C, S);
	};

	// A rail is just a short wall (height = HalfWallHeight, no lintels); it still honors openings.
	const float Top = E.bRail ? (E.BaseZ + M.HalfWallHeight) : (E.BaseZ + E.WallH);

	// sort + merge openings on this plane
	TArray<FDraftDeskOpening> Ops = E.Openings;
	Ops.Sort([](const FDraftDeskOpening& X, const FDraftDeskOpening& Y) { return X.Lo < Y.Lo; });

	TArray<FDraftDeskOpening> Merged;
	for (const FDraftDeskOpening& O : Ops)
	{
		if (Merged.Num() > 0 && O.Lo <= Merged.Last().Hi)
		{
			Merged.Last().Hi = FMath::Max(Merged.Last().Hi, O.Hi);
			Merged.Last().Height = FMath::Max(Merged.Last().Height, O.Height);
			Merged.Last().SillZ = FMath::Max(Merged.Last().SillZ, O.SillZ); // keep any window's sill
			Merged.Last().bFullClear = Merged.Last().bFullClear || O.bFullClear;
		}
		else
		{
			Merged.Add(O);
		}
	}

	float Cursor = E.Lo;
	for (const FDraftDeskOpening& O : Merged)
	{
		const float OL = FMath::Clamp(O.Lo, E.Lo, E.Hi);
		const float OR = FMath::Clamp(O.Hi, E.Lo, E.Hi);
		Solid(Cursor, OL, E.BaseZ, Top); // pier before the opening
		if (O.SillZ > 1.f)
		{
			Solid(OL, OR, E.BaseZ, E.BaseZ + O.SillZ); // sill band below a window
		}
		if (!E.bRail && !O.bFullClear)
		{
			Solid(OL, OR, E.BaseZ + O.Height, Top); // lintel above a doorway / window
		}
		Cursor = FMath::Max(Cursor, OR);
	}
	Solid(Cursor, E.Hi, E.BaseZ, Top); // final pier
}

void ADraftDeskGenerator::EmitStairFlight(const FDraftDeskStairJob& J, const FDraftDeskMetrics& M)
{
	const float DZ = J.Z1 - J.Z0;
	if (DZ <= 1.f)
	{
		return;
	}
	const int32 N = StepCount(DZ, M);
	const float R = DZ / N; // effective rise per step (<= StepRise); keeps treads even and the top flush

	for (int32 K = 0; K < N; ++K)
	{
		const float H = (K + 1) * R; // solid box from the lower floor up to this tread's top
		const float UC = J.StartU + J.Dir * (K + 0.5f) * M.StepRun;
		const FVector C = J.bAlongX
			? FVector(UC, J.CrossV, J.Z0 + H * 0.5f)
			: FVector(J.CrossV, UC, J.Z0 + H * 0.5f);
		const FVector S = J.bAlongX
			? FVector(M.StepRun, J.W, H)
			: FVector(J.W, M.StepRun, H);
		AddBox(C, S);
	}
}

void ADraftDeskGenerator::EmitRamp(const FDraftDeskStairJob& J, const FDraftDeskMetrics& M)
{
	const float T = WallThickness;
	const float DZ = J.Z1 - J.Z0;
	if (DZ <= 1.f)
	{
		return;
	}
	const float AngleDeg = FMath::Min(M.MaxStepTraversalAngle, 89.f);
	const float TanA = FMath::Tan(FMath::DegreesToRadians(AngleDeg));
	const float Run = (TanA > KINDA_SMALL_NUMBER) ? DZ / TanA : DZ;
	const float L = FMath::Sqrt(DZ * DZ + Run * Run);
	const float UMid = J.StartU + J.Dir * Run * 0.5f;

	const FVector C = J.bAlongX
		? FVector(UMid, J.CrossV, J.Z0 + DZ * 0.5f)
		: FVector(J.CrossV, UMid, J.Z0 + DZ * 0.5f);
	const FVector S = J.bAlongX ? FVector(L, J.W, T) : FVector(J.W, L, T);

	FRotator Rot = FRotator::ZeroRotator;
	if (J.bAlongX) { Rot.Pitch = -AngleDeg * J.Dir; }
	else { Rot.Roll = AngleDeg * J.Dir; }
	AddRotatedBox(C, S, Rot);
}

void ADraftDeskGenerator::EmitColumns(const FDraftDeskRoom& R, const FDraftDeskMetrics& M)
{
	const float H = EffH(R, M);
	const float ColsX[3] = { R.Min.X + R.W() * 0.28f, R.CX(), R.Min.X + R.W() * 0.72f };
	const float RowsY[2] = { R.CY() - R.D() * 0.3f, R.CY() + R.D() * 0.3f };
	for (float Cx : ColsX)
	{
		for (float Cy : RowsY)
		{
			AddColumn(Cx, Cy, R.FloorZ, H, ColumnDiameter);
		}
	}
}

// ---- orchestrator --------------------------------------------------------

void ADraftDeskGenerator::Rebuild()
{
	if (Blocks)  { Blocks->ClearInstances();  Blocks->SetMaterial(0, GridMaterial); }
	if (Columns) { Columns->ClearInstances(); Columns->SetMaterial(0, GridMaterial); }

	Rooms.Reset();
	Links.Reset();
	StairQueue.Reset();
	ExtraBoxes.Reset();

	if (!Spec)
	{
		return; // no spec assigned -> nothing to build
	}
	const FDraftDeskMetrics& M = Spec->Metrics;

	BuildPreset(M);
	if (Rooms.Num() == 0 && StairQueue.Num() == 0 && ExtraBoxes.Num() == 0)
	{
		return;
	}
	NormalizeToEntry(M);

	EmitFloorsAndCeilings(M);

	TMap<FString, FDraftDeskEdgeRec> Ledger;
	BuildEdgeLedger(Ledger, M);
	CarveOpenings(Ledger, M);
	for (auto& KV : Ledger)
	{
		EmitWall(KV.Value, M);
	}

	for (const FDraftDeskStairJob& J : StairQueue)
	{
		if (J.bRamp) { EmitRamp(J, M); }
		else { EmitStairFlight(J, M); }
	}

	for (const FDraftDeskRoom& R : Rooms)
	{
		if (bColumns && R.bColumns)
		{
			EmitColumns(R, M);
		}
	}

	for (const FDraftDeskBlock& B : ExtraBoxes)
	{
		AddBox(B.Center, B.Size);
	}
}
