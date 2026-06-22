#include "DraftDeskGenerator.h"

#include "DraftDeskSpec.h"
#include "Shell/DdShellCore.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

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

// ---- low-level primitives (unchanged) ------------------------------------

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

// ---- stair math (single source of truth for presets + emitter; unchanged) -

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

// ---- graph helpers (presets author through these) ------------------------

int32 ADraftDeskGenerator::AddLevel(float BaseZ, float Height, float SlabT)
{
	FDdLevel L;
	L.Index = Levels.Num();
	L.BaseZ = BaseZ;
	L.Height = Height;
	L.SlabT = SlabT;
	return Levels.Add(L);
}

int32 ADraftDeskGenerator::AddRoom(float MinX, float MinY, float MaxX, float MaxY, int32 Level)
{
	FDdRoom R;
	R.Min = FVector2D(MinX, MinY);
	R.Max = FVector2D(MaxX, MaxY);
	R.Level = Level;
	return Rooms.Add(R);
}

void ADraftDeskGenerator::AddDoor(int32 A, int32 B, float Width, float Height, float Position)
{
	FDdThreshold T;
	T.RoomA = A; T.RoomB = B;
	T.Kind = EDdThresholdKind::Doorway; T.Plane = EDdPlaneClass::Vertical;
	T.Width = Width; T.Height = Height; T.Position = Position;
	Thresholds.Add(T);
}

void ADraftDeskGenerator::AddPassage(int32 A, int32 B)
{
	FDdThreshold T;
	T.RoomA = A; T.RoomB = B;
	T.Kind = EDdThresholdKind::Passage; T.Plane = EDdPlaneClass::Vertical;
	Thresholds.Add(T);
}

void ADraftDeskGenerator::AddExteriorDoor(int32 A, EDraftDeskEdge Edge, bool bEntry, float Width, float Height)
{
	FDdThreshold T;
	T.RoomA = A; T.RoomB = INDEX_NONE;
	T.Kind = EDdThresholdKind::Doorway; T.Plane = EDdPlaneClass::Vertical;
	T.bIsEntry = bEntry; T.ExteriorEdge = Edge;
	T.Width = Width; T.Height = Height;
	Thresholds.Add(T);
}

void ADraftDeskGenerator::AddRail(int32 A, int32 B, EDraftDeskEdge Edge)
{
	FDdThreshold T;
	T.RoomA = A; T.RoomB = B;
	T.Kind = EDdThresholdKind::Rail; T.Plane = EDdPlaneClass::Vertical;
	T.ExteriorEdge = Edge;
	Thresholds.Add(T);
}

void ADraftDeskGenerator::AddStairwell(int32 A, int32 B, float Width, float Position, float Position2, bool bRamp)
{
	FDdThreshold T;
	T.RoomA = A; T.RoomB = B;
	T.Kind = bRamp ? EDdThresholdKind::Ramp : EDdThresholdKind::Stairwell;
	T.Plane = EDdPlaneClass::Horizontal;
	T.Width = Width; T.Position = Position; T.Position2 = Position2; T.bRamp = bRamp;
	Thresholds.Add(T);
}

int32 ADraftDeskGenerator::BuildStackedLevels(int32 Count, float StoreyH, float SlabT)
{
	for (int32 K = 0; K < Count; ++K)
	{
		AddLevel(K * (StoreyH + SlabT), StoreyH, SlabT);
	}
	return Count;
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
	const float T = BuiltWallT;
	const float ED = CellSize * 0.5f;
	const float MD = CellSize;
	const float HL = HallLength;
	const float CW = M.CorridorWidth;
	AddLevel(0.f, M.CeilingMin, T);

	const int32 A = AddRoom(0.f, -ED * 0.5f, ED, ED * 0.5f);
	const int32 H = AddRoom(ED + T, -CW * 0.5f, ED + T + HL, CW * 0.5f);
	const float Cx0 = ED + T + HL + T;
	const int32 C = AddRoom(Cx0, -MD * 0.5f, Cx0 + MD, MD * 0.5f);
	Rooms[C].bColumns = true;

	AddExteriorDoor(A, EDraftDeskEdge::West, /*bEntry*/ true);
	AddDoor(A, H);
	AddDoor(H, C);

	// raised dais near the back of the main room (legacy character piece)
	const float DaisH = 80.f;
	FDraftDeskBlock Dais;
	Dais.Center = FVector(Cx0 + MD * 0.78f, 0.f, DaisH * 0.5f);
	Dais.Size = FVector(MD * 0.4f, MD * 0.4f, DaisH);
	ExtraBoxes.Add(Dais);
}

void ADraftDeskGenerator::BuildPreset_SingleRoom(const FDraftDeskMetrics& M)
{
	AddLevel(0.f, M.CeilingMin, BuiltWallT);
	const int32 R = AddRoom(0.f, -CellSize * 0.5f, CellSize, CellSize * 0.5f);
	AddExteriorDoor(R, EDraftDeskEdge::West, true);
}

void ADraftDeskGenerator::BuildPreset_Corridor(const FDraftDeskMetrics& M)
{
	const float CW = M.CorridorWidth;
	AddLevel(0.f, M.CeilingMin, BuiltWallT);
	const int32 R = AddRoom(0.f, -CW * 0.5f, 3.f * CellSize, CW * 0.5f);
	AddExteriorDoor(R, EDraftDeskEdge::West, true);
	AddExteriorDoor(R, EDraftDeskEdge::East, false);
}

void ADraftDeskGenerator::BuildPreset_LBend(const FDraftDeskMetrics& M)
{
	const float C = CellSize;
	const float T = BuiltWallT;
	const float CW = M.CorridorWidth;
	AddLevel(0.f, M.CeilingMin, T);

	const int32 HArm = AddRoom(0.f, -CW * 0.5f, 2.f * C, CW * 0.5f);
	const int32 Node = AddRoom(2.f * C + T, -CW * 0.5f, 2.f * C + T + CW, CW * 0.5f);
	const int32 VArm = AddRoom(2.f * C + T, CW * 0.5f + T, 2.f * C + T + CW, CW * 0.5f + T + 2.f * C);

	AddPassage(HArm, Node); // HArm east <-> Node west
	AddPassage(Node, VArm); // Node north <-> VArm south
	AddExteriorDoor(HArm, EDraftDeskEdge::West, true);
	AddExteriorDoor(VArm, EDraftDeskEdge::North, false);
}

void ADraftDeskGenerator::BuildPreset_TJunction(const FDraftDeskMetrics& M)
{
	const float C = CellSize;
	const float T = BuiltWallT;
	const float CW = M.CorridorWidth;
	AddLevel(0.f, M.CeilingMin, T);

	const int32 Main = AddRoom(0.f, -CW * 0.5f, 3.f * C, CW * 0.5f);
	const int32 Branch = AddRoom(1.5f * C - CW * 0.5f, CW * 0.5f + T, 1.5f * C + CW * 0.5f, CW * 0.5f + T + 2.f * C);

	AddExteriorDoor(Main, EDraftDeskEdge::West, true);
	AddExteriorDoor(Main, EDraftDeskEdge::East, false);
	AddExteriorDoor(Branch, EDraftDeskEdge::North, false);
	AddPassage(Main, Branch); // full-clear mouth into the corridor's north wall
}

void ADraftDeskGenerator::BuildPreset_Cross(const FDraftDeskMetrics& M)
{
	const float C = CellSize;
	const float T = BuiltWallT;
	const float CW = M.CorridorWidth;
	const float Hh = CW * 0.5f;
	AddLevel(0.f, M.CeilingMin, T);

	const int32 WArm = AddRoom(0.f, -Hh, 2.f * C, Hh);
	const float Nx0 = 2.f * C + T;
	const int32 Node = AddRoom(Nx0, -Hh, Nx0 + CW, Hh);
	const float Ex0 = Nx0 + CW + T;
	const int32 EArm = AddRoom(Ex0, -Hh, Ex0 + 2.f * C, Hh);
	const int32 SArm = AddRoom(Nx0, -Hh - T - 2.f * C, Nx0 + CW, -Hh - T);
	const int32 NArm = AddRoom(Nx0, Hh + T, Nx0 + CW, Hh + T + 2.f * C);

	AddPassage(WArm, Node);
	AddPassage(Node, EArm);
	AddPassage(SArm, Node);
	AddPassage(Node, NArm);

	AddExteriorDoor(WArm, EDraftDeskEdge::West, true);
	AddExteriorDoor(EArm, EDraftDeskEdge::East, false);
	AddExteriorDoor(SArm, EDraftDeskEdge::South, false);
	AddExteriorDoor(NArm, EDraftDeskEdge::North, false);
}

void ADraftDeskGenerator::BuildPreset_Grid2x2(const FDraftDeskMetrics& M)
{
	const float C = CellSize;
	const float T = BuiltWallT;
	AddLevel(0.f, M.CeilingMin, T);

	const int32 SW = AddRoom(0.f, -C, C, 0.f);
	const int32 SE = AddRoom(C + T, -C, 2.f * C + T, 0.f);
	const int32 NW = AddRoom(0.f, T, C, C + T);
	const int32 NE = AddRoom(C + T, T, 2.f * C + T, C + T);

	AddExteriorDoor(SW, EDraftDeskEdge::West, true);
	AddDoor(SW, SE); // shared X plane -> one wall
	AddDoor(NW, NE);
	AddDoor(SW, NW); // shared Y plane -> one wall
	AddDoor(SE, NE);
}

void ADraftDeskGenerator::BuildPreset_SplitLevel(const FDraftDeskMetrics& M)
{
	const float C = CellSize;
	const float T = BuiltWallT;
	const float StoreyH = FMath::Max(M.CeilingMin, FloorDelta - T);
	BuildStackedLevels(2, StoreyH, T);

	const int32 R1 = AddRoom(0.f, -C * 0.5f, C, C * 0.5f, /*Level*/ 0);
	const int32 R2 = AddRoom(0.f, -C * 0.5f, C, C * 0.5f, /*Level*/ 1); // stacked above R1

	AddExteriorDoor(R1, EDraftDeskEdge::West, true);
	AddStairwell(R1, R2, M.CorridorWidth); // shaft carves the upper floor; flight fills it
}

void ADraftDeskGenerator::BuildPreset_Tower(const FDraftDeskMetrics& M)
{
	const float C = CellSize;
	const float T = BuiltWallT;
	const float StoreyH = FMath::Max(M.CeilingMin, FloorDelta - T);
	const int32 NLevels = 3;
	BuildStackedLevels(NLevels, StoreyH, T);

	int32 Prev = INDEX_NONE;
	for (int32 K = 0; K < NLevels; ++K)
	{
		const int32 R = AddRoom(0.f, -C * 0.5f, C, C * 0.5f, K);
		if (Prev == INDEX_NONE)
		{
			AddExteriorDoor(R, EDraftDeskEdge::West, true);
		}
		else
		{
			// offset alternate flights across Y so the shafts don't stack on one spot
			const float Off = (K % 2 == 0) ? -C * 0.25f : C * 0.25f;
			AddStairwell(Prev, R, M.CorridorWidth, 0.f, Off);
		}
		Prev = R;
	}
}

void ADraftDeskGenerator::BuildPreset_Ramp(const FDraftDeskMetrics& M)
{
	const float C = CellSize;
	const float T = BuiltWallT;
	const float StoreyH = FMath::Max(M.CeilingMin, FloorDelta - T);
	BuildStackedLevels(2, StoreyH, T);

	const int32 R1 = AddRoom(0.f, -C * 0.5f, C, C * 0.5f, 0);
	const int32 R2 = AddRoom(0.f, -C * 0.5f, C, C * 0.5f, 1);

	AddExteriorDoor(R1, EDraftDeskEdge::West, true);
	AddStairwell(R1, R2, M.CorridorWidth, 0.f, 0.f, /*bRamp*/ true);
}

void ADraftDeskGenerator::BuildPreset_Mezzanine(const FDraftDeskMetrics& M)
{
	const float C = CellSize;
	const float T = BuiltWallT;
	const float StoreyH = FMath::Max(M.CeilingMin, FloorDelta - T);
	BuildStackedLevels(2, StoreyH, T);

	// Tall main room spanning both storeys: Height override = two storeys + the mid slab.
	const float TallH = 2.f * StoreyH + T;
	const int32 Big = AddRoom(0.f, -C, 2.f * C, C, /*Level*/ 0);
	Rooms[Big].Height = TallH;

	// Mezzanine over the back third at level 1; its floor covers only that sub-rect.
	const float MezX0 = 1.4f * C;
	const int32 Mez = AddRoom(MezX0, -C, 2.f * C, C, /*Level*/ 1);
	Rooms[Mez].bCeil = false; // open to the tall volume above

	// Atrium void suppresses the level-1 slab over the FRONT (where there is no mezzanine).
	// Void footprint x[0, MezX0], full depth. Centre it on the Big room with a Position offset.
	const float VoidW = MezX0;                 // 0 .. MezX0
	const float VoidPos = -(2.f * C - VoidW) * 0.5f; // shift left so the void hugs the front
	FDdThreshold Atr;
	Atr.RoomA = Big; Atr.RoomB = INDEX_NONE;
	Atr.Kind = EDdThresholdKind::Atrium; Atr.Plane = EDdPlaneClass::Horizontal;
	Atr.Width = VoidW; Atr.Depth = 2.f * C; Atr.Position = VoidPos;
	Thresholds.Add(Atr);

	AddRail(Mez, INDEX_NONE, EDraftDeskEdge::West); // guard rail along the mezzanine drop
	AddExteriorDoor(Big, EDraftDeskEdge::West, true);
	AddStairwell(Big, Mez, M.CorridorWidth); // flight up into the mezzanine
}

void ADraftDeskGenerator::BuildPreset_Custom(const FDraftDeskMetrics& M)
{
	Levels = AuthoredLevels;
	Rooms = AuthoredRooms;
	Thresholds = AuthoredThresholds;
	Flights = AuthoredFlights;
	ExtraBoxes = AuthoredBoxes;
	if (Levels.Num() == 0)
	{
		AddLevel(0.f, M.CeilingMin, BuiltWallT); // single-level fallback
	}
}

// ---- emission ------------------------------------------------------------

void ADraftDeskGenerator::NormalizeToEntry(const FDraftDeskMetrics& M)
{
	if (Rooms.Num() == 0)
	{
		return;
	}
	const float T = BuiltWallT;

	float Dx = 0.f, Dy = 0.f;
	for (const FDdThreshold& Th : Thresholds)
	{
		if (!Th.bIsEntry || Th.Plane != EDdPlaneClass::Vertical)
		{
			continue;
		}
		if (Th.RoomA < 0 || Th.RoomA >= Rooms.Num())
		{
			continue;
		}
		const FDdRoom& R = Rooms[Th.RoomA];
		switch (Th.ExteriorEdge)
		{
		case EDraftDeskEdge::West:  Dx = R.Min.X - T * 0.5f; Dy = R.CY() + Th.Position; break;
		case EDraftDeskEdge::East:  Dx = R.Max.X + T * 0.5f; Dy = R.CY() + Th.Position; break;
		case EDraftDeskEdge::South: Dx = R.CX() + Th.Position; Dy = R.Min.Y - T * 0.5f; break;
		case EDraftDeskEdge::North: Dx = R.CX() + Th.Position; Dy = R.Max.Y + T * 0.5f; break;
		default: break;
		}
		break;
	}

	float MinZ = Levels.Num() > 0 ? Levels[0].BaseZ : 0.f;
	for (const FDdLevel& L : Levels)
	{
		MinZ = FMath::Min(MinZ, L.BaseZ);
	}

	for (FDdLevel& L : Levels)
	{
		L.BaseZ -= MinZ;
	}
	for (FDdRoom& R : Rooms)
	{
		R.Min.X -= Dx; R.Max.X -= Dx;
		R.Min.Y -= Dy; R.Max.Y -= Dy;
		if (R.FloorZ >= 0.f) { R.FloorZ -= MinZ; }
	}
	for (FDraftDeskBlock& B : ExtraBoxes)
	{
		B.Center.X -= Dx; B.Center.Y -= Dy; B.Center.Z -= MinZ;
	}
	for (FDdFlight& F : Flights)
	{
		if (F.bAlongX) { F.StartU -= Dx; F.CrossV -= Dy; }
		else           { F.StartU -= Dy; F.CrossV -= Dx; }
		F.FromZ -= MinZ; F.ToZ -= MinZ;
	}
}

void ADraftDeskGenerator::SnapLayoutToGrid(const FDraftDeskMetrics& M)
{
	auto Snap = [](float V, float Step) { return Step > KINDA_SMALL_NUMBER ? FMath::GridSnap(V, Step) : V; };
	const float SX = M.GridSnap.X, SY = M.GridSnap.Y, SZ = M.GridSnap.Z;

	for (FDdLevel& L : Levels)
	{
		L.BaseZ = Snap(L.BaseZ, SZ);
		L.Height = Snap(L.Height, SZ);
		if (L.SlabT > 0.f) { L.SlabT = Snap(L.SlabT, SZ); }
	}
	for (FDdRoom& R : Rooms)
	{
		R.Min.X = Snap(R.Min.X, SX); R.Max.X = Snap(R.Max.X, SX);
		R.Min.Y = Snap(R.Min.Y, SY); R.Max.Y = Snap(R.Max.Y, SY);
		if (R.FloorZ >= 0.f) { R.FloorZ = Snap(R.FloorZ, SZ); }
		if (R.Height > 0.f)  { R.Height = Snap(R.Height, SZ); }
	}
	for (FDraftDeskBlock& B : ExtraBoxes)
	{
		B.Center.X = Snap(B.Center.X, SX);
		B.Center.Y = Snap(B.Center.Y, SY);
		B.Center.Z = Snap(B.Center.Z, SZ);
	}
	// Stairs/ramps are EXEMPT (the core derives flights grid-EXEMPT — R4); their wells round OUT to grid.
}

void ADraftDeskGenerator::EmitStairFlight(bool bAlongX, float StartU, int32 Dir, float CrossV, float Z0, float Z1, float W, const FDraftDeskMetrics& M)
{
	const float DZ = Z1 - Z0;
	if (DZ <= 1.f)
	{
		return;
	}
	const int32 N = StepCount(DZ, M);
	const float R = DZ / N; // effective rise per step (<= StepRise); keeps treads even, top flush

	for (int32 K = 0; K < N; ++K)
	{
		const float H = (K + 1) * R; // solid box from the lower floor up to this tread's top
		const float UC = StartU + Dir * (K + 0.5f) * M.StepRun;
		const FVector C = bAlongX
			? FVector(UC, CrossV, Z0 + H * 0.5f)
			: FVector(CrossV, UC, Z0 + H * 0.5f);
		const FVector S = bAlongX
			? FVector(M.StepRun, W, H)
			: FVector(W, M.StepRun, H);
		AddBox(C, S);
	}
}

void ADraftDeskGenerator::EmitRamp(bool bAlongX, float StartU, int32 Dir, float CrossV, float Z0, float Z1, float W, const FDraftDeskMetrics& M)
{
	const float T = BuiltWallT;
	const float DZ = Z1 - Z0;
	if (DZ <= 1.f)
	{
		return;
	}
	const float AngleDeg = FMath::Min(M.MaxStepTraversalAngle, 89.f);
	const float TanA = FMath::Tan(FMath::DegreesToRadians(AngleDeg));
	const float Run = (TanA > KINDA_SMALL_NUMBER) ? DZ / TanA : DZ;
	const float L = FMath::Sqrt(DZ * DZ + Run * Run);
	const float UMid = StartU + Dir * Run * 0.5f;

	const FVector C = bAlongX
		? FVector(UMid, CrossV, Z0 + DZ * 0.5f)
		: FVector(CrossV, UMid, Z0 + DZ * 0.5f);
	const FVector S = bAlongX ? FVector(L, W, T) : FVector(W, L, T);

	FRotator Rot = FRotator::ZeroRotator;
	if (bAlongX) { Rot.Pitch = -AngleDeg * Dir; }
	else { Rot.Roll = AngleDeg * Dir; }
	AddRotatedBox(C, S, Rot);
}

void ADraftDeskGenerator::EmitColumns(const FDdRoom& R, float FloorZ, float Height, const FDraftDeskMetrics& M)
{
	const float ColsX[3] = { R.Min.X + R.W() * 0.28f, R.CX(), R.Min.X + R.W() * 0.72f };
	const float RowsY[2] = { R.CY() - R.D() * 0.3f, R.CY() + R.D() * 0.3f };
	for (float Cx : ColsX)
	{
		for (float Cy : RowsY)
		{
			AddColumn(Cx, Cy, FloorZ, Height, ColumnDiameter);
		}
	}
}

// ---- orchestrator --------------------------------------------------------

void ADraftDeskGenerator::Rebuild()
{
	if (Blocks)  { Blocks->ClearInstances();  Blocks->SetMaterial(0, GridMaterial); }
	if (Columns) { Columns->ClearInstances(); Columns->SetMaterial(0, GridMaterial); }

	Levels.Reset();
	Rooms.Reset();
	Thresholds.Reset();
	Flights.Reset();
	ExtraBoxes.Reset();

	if (!Spec)
	{
		return; // no spec assigned -> nothing to build
	}
	const FDraftDeskMetrics& M = Spec->Metrics;

	// Per-build grid + wall thickness (unchanged rule): WallThickness rounds UP to a whole XY cell so
	// abutting room faces stay one wall-gap apart and share a plane bucket.
	BuiltSnap = M.GridSnap;
	const float GridXY = (BuiltSnap.X > KINDA_SMALL_NUMBER && BuiltSnap.Y > KINDA_SMALL_NUMBER)
		? FMath::Min(BuiltSnap.X, BuiltSnap.Y)
		: FMath::Max(BuiltSnap.X, BuiltSnap.Y);
	BuiltWallT = (GridXY > KINDA_SMALL_NUMBER)
		? FMath::Max(GridXY, FMath::CeilToFloat(WallThickness / GridXY) * GridXY)
		: WallThickness;

	BuildPreset(M);
	if (Rooms.Num() == 0 && ExtraBoxes.Num() == 0)
	{
		return;
	}
	NormalizeToEntry(M);
	SnapLayoutToGrid(M);

	// --- convert reflected structs into the portable watertight core ---
	dd::Metrics dm;
	dm.grid = GridXY > KINDA_SMALL_NUMBER ? GridXY : 0.0;
	dm.wall_thickness = WallThickness;
	dm.door_width = M.DoorWidth;
	dm.door_height = M.DoorHeight;
	dm.corridor_width = M.CorridorWidth;
	dm.ceiling_min = M.CeilingMin;
	dm.half_wall = M.HalfWallHeight;
	dm.window_clear = M.WindowClearHeight;
	dm.window_sill = M.HalfWallHeight;
	dm.step_rise = M.StepRise;
	dm.step_run = M.StepRun;
	dm.max_step_angle = M.MaxStepTraversalAngle;

	std::vector<dd::Level> dlevels;
	for (const FDdLevel& L : Levels)
	{
		dlevels.emplace_back(L.Index, L.BaseZ, L.Height > 0.f ? L.Height : M.CeilingMin,
			L.SlabT > 0.f ? L.SlabT : BuiltWallT);
	}
	std::vector<dd::Room> drooms;
	for (const FDdRoom& R : Rooms)
	{
		dd::Room r(R.Min.X, R.Min.Y, R.Max.X, R.Max.Y);
		r.level = R.Level;
		if (R.FloorZ >= 0.f) { r.has_floor_z = true; r.floor_z = R.FloorZ; }
		r.height = R.Height;
		r.floor = R.bFloor;
		r.ceil = R.bCeil;
		drooms.push_back(r);
	}
	std::vector<dd::Threshold> dthr;
	for (const FDdThreshold& Th : Thresholds)
	{
		dd::Threshold t(Th.RoomA, Th.RoomB, static_cast<dd::Kind>(static_cast<uint8>(Th.Kind)));
		t.plane = (Th.Plane == EDdPlaneClass::Horizontal) ? dd::HORIZONTAL : dd::VERTICAL;
		t.position = Th.Position; t.position2 = Th.Position2;
		t.width = Th.Width; t.depth = Th.Depth; t.height = Th.Height; t.sill = Th.Sill;
		t.is_entry = Th.bIsEntry; t.bRamp = Th.bRamp;
		t.edge = static_cast<int>(Th.ExteriorEdge);
		dthr.push_back(t);
	}

	// explicit flights are passed to the core so it can DERIVE rail gaps where they land (the generator
	// still renders the treads below via EmitStairFlight). One flight drives both the gap and the steps.
	std::vector<dd::Flight> dflights;
	for (const FDdFlight& F : Flights)
	{
		dflights.push_back({F.bAlongX, F.StartU, F.CrossV, F.Width, F.FromZ, F.ToZ, F.Dir, F.bRamp, -1});
	}

	dd::Shell Shell(std::move(drooms), std::move(dthr), std::move(dlevels), dm, std::move(dflights));
	Shell.build();
	const std::vector<std::string> Fails = Shell.validate();

	// Validator policy (owner decision): loud-warn-but-EMIT in the editor so an author mid-edit is
	// never blocked from seeing partial geometry. Hard-fail lives in the Phase-0 pure-data tests.
	for (const std::string& W : Shell.warnings)
	{
		UE_LOG(LogTemp, Warning, TEXT("draftDesk: %s"), *FString(UTF8_TO_TCHAR(W.c_str())));
	}
	for (const std::string& F : Fails)
	{
		UE_LOG(LogTemp, Error, TEXT("draftDesk VALIDATOR: %s"), *FString(UTF8_TO_TCHAR(F.c_str())));
	}

	// walls / floors / ceilings / lintels / sills / piers — all solid output rects, placed by the core
	for (const dd::Box& B : Shell.emit_boxes())
	{
		if (B.kind == 2 && !bPlaceCeilings)
		{
			continue; // hide ceiling/roof-only slabs in the top-down editor view (still validated)
		}
		AddBox(FVector(B.cx, B.cy, B.cz), FVector(B.sx, B.sy, B.sz));
	}

	// stair / ramp flights (the fill of each carved well)
	for (const dd::Flight& Fl : Shell.flights)
	{
		if (Fl.ramp)
		{
			EmitRamp(Fl.along_x, Fl.start_u, Fl.dir, Fl.cross_v, Fl.z0, Fl.z1, Fl.w, M);
		}
		else
		{
			EmitStairFlight(Fl.along_x, Fl.start_u, Fl.dir, Fl.cross_v, Fl.z0, Fl.z1, Fl.w, M);
		}
	}

	// explicit authored flights (grand staircases that land at an edge; grid-EXEMPT fill, no slab carve)
	for (const FDdFlight& F : Flights)
	{
		const float W = F.Width > 0.f ? F.Width : M.CorridorWidth;
		if (F.bRamp)
		{
			EmitRamp(F.bAlongX, F.StartU, F.Dir, F.CrossV, F.FromZ, F.ToZ, W, M);
		}
		else
		{
			EmitStairFlight(F.bAlongX, F.StartU, F.Dir, F.CrossV, F.FromZ, F.ToZ, W, M);
		}
	}

	// columns (per room that opts in) — floor + height come from the reconciled core
	for (int32 i = 0; i < Rooms.Num(); ++i)
	{
		if (bColumns && Rooms[i].bColumns)
		{
			const std::pair<double, double> E = Shell.eff(i);
			EmitColumns(Rooms[i], static_cast<float>(E.first), static_cast<float>(E.second), M);
		}
	}

	// decorative solids (dais / cover / pillar / ledge)
	for (const FDraftDeskBlock& B : ExtraBoxes)
	{
		AddBox(B.Center, B.Size);
	}

	// --- record the engine's openings: one marker anchor per resolved threshold + per flight. This is
	//     the source of truth tools read for "where does a marker belong"; the marker reconciler (next
	//     slice) places exactly one marker at each of these, making marker<->opening bijective by build.
	Openings.Reset();
	for (const dd::OpeningPt& P : Shell.opening_points())
	{
		if (!P.resolved || P.thr < 0 || P.thr >= Thresholds.Num())
		{
			continue; // an unresolved threshold carved nothing -> no marker (matches the seed logic)
		}
		const FDdThreshold& Th = Thresholds[P.thr];
		FDdOpening O;
		O.Label = (Th.RoomB != INDEX_NONE)
			? FString::Printf(TEXT("%d-%d"), Th.RoomA, Th.RoomB)
			: (Th.bIsEntry ? FString(TEXT("entry")) : FString::Printf(TEXT("ext%d"), P.thr));
		O.Kind = Th.Kind;
		O.Position = FVector(P.x, P.y, P.z + 50.0); // +50 hover, matching the seeded marker height
		O.RoomA = Th.RoomA; O.RoomB = Th.RoomB; O.Plane = Th.Plane;
		O.Width = Th.Width; O.Height = Th.Height; O.Sill = Th.Sill; O.bIsEntry = Th.bIsEntry;
		O.SourceThreshold = P.thr; O.SourceFlight = -1;
		Openings.Add(O);
	}
	for (int32 N = 0; N < Flights.Num(); ++N)
	{
		const FDdFlight& F = Flights[N];
		const float Run = TotalRun(FMath::Abs(F.ToZ - F.FromZ), M);
		const float UTop = F.StartU + F.Dir * Run; // where the top tread lands
		FDdOpening O;
		O.Label = FString::Printf(TEXT("flight%d"), N);
		O.Kind = F.bRamp ? EDdThresholdKind::Ramp : EDdThresholdKind::Stairwell;
		O.Position = FVector(F.bAlongX ? UTop : F.CrossV, F.bAlongX ? F.CrossV : UTop, F.ToZ + 50.0);
		O.RoomA = INDEX_NONE; O.RoomB = INDEX_NONE; O.Plane = EDdPlaneClass::Horizontal;
		O.Width = F.Width; O.bIsEntry = false;
		O.SourceThreshold = -1; O.SourceFlight = N;
		Openings.Add(O);
	}
}
