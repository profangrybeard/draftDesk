// Copyright draftDesk.

#include "DdNavToolset.h"

#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"            // TActorIterator
#include "ScopedTransaction.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "DraftDeskGenerator.h"
#include "DraftDeskThreshold.h"
#include "DraftDeskRoomHandle.h"
#include "DraftDeskSpec.h"

TArray<FDdNavResult> UDdNavToolset::CheckReachability(FVector Start, const TArray<FVector>& Targets)
{
	TArray<FDdNavResult> Out;
	Out.Reserve(Targets.Num());

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;

	for (const FVector& Target : Targets)
	{
		FDdNavResult R;
		R.Target = Target;
		if (World)
		{
			UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, Start, Target);
			if (Path && Path->IsValid())
			{
				R.bPartial = Path->IsPartial();
				R.bReachable = !Path->IsPartial();
				R.Length = Path->GetPathLength();
			}
		}
		Out.Add(R);
	}
	return Out;
}

#define LOCTEXT_NAMESPACE "draftDesk"

namespace
{
	// Editor-world ONLY — never spawn/delete/fold into a PIE-duplicated world.
	UWorld* EditorWorldOrNull()
	{
		if (!GEditor || GEditor->PlayWorld != nullptr || GIsPlayInEditorWorld) { return nullptr; }
		UWorld* W = GEditor->GetEditorWorldContext().World();
		return (W && W->WorldType == EWorldType::Editor) ? W : nullptr;
	}

	ADraftDeskGenerator* ResolveGen(const FString& Path, UWorld* World)
	{
		ADraftDeskGenerator* Gen = FindObject<ADraftDeskGenerator>(nullptr, *Path);
		if (!Gen)
		{
			for (TActorIterator<ADraftDeskGenerator> It(World); It; ++It) { Gen = *It; break; }
		}
		return Gen;
	}

	// The reconcile core (NO transaction — the caller owns it): make the live markers EXACTLY match the
	// generator's Openings (spawn missing, move drifted, delete orphans/dups), stamp each survivor's
	// identity + reconciled home, and bump the generator's ReconcileSerial once. Strict label bijection.
	void ReconcileInternal(ADraftDeskGenerator* Gen, UWorld* World, FDdReconcileReport& Report)
	{
		ULevel* GenLevel = Gen->GetLevel();
		const TArray<FDdOpening>& Openings = Gen->Openings;
		const FTransform GenXform = Gen->GetActorTransform();
		Report.Total = Openings.Num();

		Gen->Modify();
		++Gen->ReconcileSerial;   // this pass's serial; stamped onto every survivor below

		// Desired keyed by Label. (Castle has no collisions; SourceThreshold re-key for door+window walls
		// is the deferred robustness follow-up — a collision is logged + reads RED in the multiset gate.)
		TMap<FName, const FDdOpening*> Desired;
		for (const FDdOpening& O : Openings)
		{
			const FName Key(*O.Label);
			if (Desired.Contains(Key))
			{
				++Report.Duplicates;
				UE_LOG(LogTemp, Error, TEXT("draftDesk reconcile: duplicate opening label '%s' kept-first (SourceId re-key deferred)."), *O.Label);
				continue;
			}
			Desired.Add(Key, &O);
		}

		// Every threshold that resolved this rebuild (has an opening). A marker whose SourceThreshold is NOT
		// here but still names a live interior threshold is DORMANT (red-X invalid), not a true orphan.
		TSet<int32> ResolvedSrc;
		for (const FDdOpening& O : Openings) { if (O.SourceThreshold >= 0) { ResolvedSrc.Add(O.SourceThreshold); } }

		TArray<ADraftDeskThreshold*> Markers;
		for (TActorIterator<ADraftDeskThreshold> It(World); It; ++It)
		{
			if (It->GetLevel() == GenLevel) { Markers.Add(*It); }
		}

		const double Eps = 1.0;
		auto Home = [&](ADraftDeskThreshold* M)
		{
			M->ReconciledLocation = GenXform.InverseTransformPosition(M->GetActorLocation());
			M->ReconcileSerial = Gen->ReconcileSerial;
		};

		TSet<FName> Claimed;
		for (ADraftDeskThreshold* M : Markers)
		{
			const FName L = M->Label;
			const FDdOpening* const* Found = Desired.Find(L);
			if (!Found)
			{
				if (L == FName(TEXT("entry")) || M->Kind == EDdThresholdKind::Rail)
				{
					++Report.Kept; continue; // entry/rail oscillate; never deleted on a transient miss
				}
				// DORMANT -> red-X INVALID: its threshold still exists but didn't resolve this rebuild (rooms
				// moved apart). KEEP the marker on screen, flag it, park it inside the owner room so it's always
				// grabbable -- a broken connection never silently vanishes. (Was an unconditional delete.)
				const int32 St = M->SourceThreshold;
				if (St >= 0 && St < Gen->AuthoredThresholds.Num() && !ResolvedSrc.Contains(St))
				{
					const FDdThreshold& Th = Gen->AuthoredThresholds[St];
					if (Th.RoomB != INDEX_NONE && !Th.bIsEntry && Th.Kind != EDdThresholdKind::Rail)
					{
						M->Modify();
						if (!M->bInvalid) { M->bInvalid = true; M->RefreshInvalidVisual(); }
						if (Gen->DormantAnchors.IsValidIndex(St) && !Gen->DormantAnchors[St].IsZero())
						{
							M->SetActorLocation(GenXform.TransformPosition(Gen->DormantAnchors[St]));
						}
						Home(M);
						++Report.Invalid; continue;
					}
				}
				M->Modify(); World->EditorDestroyActor(M, true); ++Report.Deleted; continue; // true orphan (threshold gone)
			}
			if (Claimed.Contains(L)) { M->Modify(); World->EditorDestroyActor(M, true); ++Report.Deleted; continue; }
			Claimed.Add(L);
			const FDdOpening& O = **Found;
			M->Modify();
			if (M->bInvalid) { M->bInvalid = false; M->RefreshInvalidVisual(); } // resolved again -> back to valid
			M->SourceThreshold = O.SourceThreshold; M->SourceFlight = O.SourceFlight;
			M->SetLockLocation(O.SourceFlight >= 0); // flights are DERIVED geometry — lock their markers (not draggable)
			const FVector CurLocal = GenXform.InverseTransformPosition(M->GetActorLocation());
			if (FVector::Dist(CurLocal, O.Position) > Eps)
			{
				M->SetActorLocation(GenXform.TransformPosition(O.Position));
				M->Kind = O.Kind; ++Report.Moved;
			}
			else { ++Report.Kept; }
			Home(M);
		}

		for (const TPair<FName, const FDdOpening*>& Pair : Desired)
		{
			if (Claimed.Contains(Pair.Key)) { continue; }
			const FDdOpening& O = *Pair.Value;
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Params.ObjectFlags |= RF_Transactional;
			Params.OverrideLevel = GenLevel;
			ADraftDeskThreshold* M = World->SpawnActor<ADraftDeskThreshold>(
				GenXform.TransformPosition(O.Position), FRotator::ZeroRotator, Params);
			if (!M) { continue; }
			M->SetActorLabel(FString(TEXT("DD_TH_")) + O.Label);
			M->Label = Pair.Key;
			M->Kind = O.Kind; M->Plane = O.Plane; M->RoomA = O.RoomA; M->RoomB = O.RoomB;
			M->Width = O.Width; M->Height = O.Height; M->Sill = O.Sill; M->bIsEntry = O.bIsEntry;
			M->SourceThreshold = O.SourceThreshold; M->SourceFlight = O.SourceFlight;
			M->SetLockLocation(O.SourceFlight >= 0); // flights are DERIVED geometry — lock their markers (not draggable)
			Home(M);
			++Report.Spawned;
		}

		// --- ROOM HANDLES: exactly one draggable handle per room, at the engine-computed RoomAnchors (same
		//     normalized frame as Openings). Keyed by RoomIndex, index-aligned to AuthoredRooms. Spawn
		//     missing, move drifted, destroy orphans/dups. Handles are NEVER locked (always author-draggable,
		//     unlike derived flight markers). Shares THIS pass's ReconcileSerial (bumped once above). ---
		const TArray<FVector>& Anchors = Gen->RoomAnchors;
		auto HomeHandle = [&](ADraftDeskRoomHandle* H)
		{
			H->ReconciledLocation = GenXform.InverseTransformPosition(H->GetActorLocation());
			H->ReconcileSerial = Gen->ReconcileSerial;
		};

		TArray<ADraftDeskRoomHandle*> Handles;
		for (TActorIterator<ADraftDeskRoomHandle> It(World); It; ++It)
		{
			if (It->GetLevel() == GenLevel) { Handles.Add(*It); }
		}

		TSet<int32> ClaimedRooms;
		for (ADraftDeskRoomHandle* H : Handles)
		{
			const int32 RI = H->RoomIndex;
			if (RI < 0 || RI >= Anchors.Num() || ClaimedRooms.Contains(RI))
			{
				H->Modify(); World->EditorDestroyActor(H, true); ++Report.RoomDeleted; continue; // orphan / dup
			}
			ClaimedRooms.Add(RI);
			H->Modify();
			const FVector WantWorld = GenXform.TransformPosition(Anchors[RI]);
			if (FVector::Dist(H->GetActorLocation(), WantWorld) > Eps)
			{
				H->SetActorLocation(WantWorld); ++Report.RoomMoved;
			}
			else { ++Report.RoomKept; }
			HomeHandle(H);
		}

		for (int32 RI = 0; RI < Anchors.Num(); ++RI)
		{
			if (ClaimedRooms.Contains(RI)) { continue; }
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Params.ObjectFlags |= RF_Transactional;
			Params.OverrideLevel = GenLevel;
			ADraftDeskRoomHandle* H = World->SpawnActor<ADraftDeskRoomHandle>(
				GenXform.TransformPosition(Anchors[RI]), FRotator::ZeroRotator, Params);
			if (!H) { continue; }
			H->SetActorLabel(FString::Printf(TEXT("DD_ROOM_%d"), RI));
			H->RoomIndex = RI;
			HomeHandle(H);
			++Report.RoomSpawned;
		}
		Report.RoomTotal = Anchors.Num();

		if (GenLevel) { GenLevel->MarkPackageDirty(); }
	}

	// Reshape from a perpendicular drag: move the SHARED WALL between the threshold's two rooms by the
	// (grid-snapped) perp delta, co-moving BOTH abutting facing edges so the one-cell wall follows. Gated
	// by the engine (connection still resolves + rooms non-degenerate); reverts cleanly on fail. Port of
	// dd_sync.reshape. Returns true if the reshape was applied. idx: 0 = const-X wall, 1 = const-Y wall.
	bool TryReshape(ADraftDeskGenerator* Gen, int32 ti, int32 idx, double SignedPerp, double Grid)
	{
		if (ti < 0 || ti >= Gen->AuthoredThresholds.Num()) { return false; }
		const FDdThreshold& Th = Gen->AuthoredThresholds[ti];
		const int32 a = Th.RoomA, b = Th.RoomB;
		if (b == INDEX_NONE || Th.Kind == EDdThresholdKind::Rail) { return false; } // interior only; rails don't reshape
		if (a < 0 || a >= Gen->AuthoredRooms.Num() || b < 0 || b >= Gen->AuthoredRooms.Num()) { return false; }

		const float Perp = (Grid > 1.0) ? FMath::GridSnap((float)SignedPerp, (float)Grid) : (float)SignedPerp;
		if (FMath::IsNearlyZero(Perp)) { return false; }
		const float T = Gen->GetBuiltWallT();
		FDdRoom& RA = Gen->AuthoredRooms[a];
		FDdRoom& RB = Gen->AuthoredRooms[b];
		auto Comp = [idx](const FVector2D& V) { return idx == 0 ? V.X : V.Y; };
		auto SetComp = [idx](FVector2D& V, float v) { if (idx == 0) { V.X = v; } else { V.Y = v; } };

		const bool bALow = (Comp(RA.Min) + Comp(RA.Max)) < (Comp(RB.Min) + Comp(RB.Max));
		const float PlaneA = bALow ? Comp(RA.Max) + T * 0.5f : Comp(RA.Min) - T * 0.5f;
		const float PlaneB = bALow ? Comp(RB.Min) - T * 0.5f : Comp(RB.Max) + T * 0.5f;
		const float NewPlane = (PlaneA + PlaneB) * 0.5f + Perp;

		const FVector2D SA0 = RA.Min, SA1 = RA.Max, SB0 = RB.Min, SB1 = RB.Max; // save for revert
		if (bALow) { SetComp(RA.Max, NewPlane - T * 0.5f); SetComp(RB.Min, NewPlane + T * 0.5f); }
		else       { SetComp(RA.Min, NewPlane + T * 0.5f); SetComp(RB.Max, NewPlane - T * 0.5f); }

		if (!Gen->ReshapeGatePasses(a, b))
		{
			RA.Min = SA0; RA.Max = SA1; RB.Min = SB0; RB.Max = SB1; // revert: connection broke / room degenerate
			return false;
		}
		return true;
	}
}

FDdReconcileReport UDdNavToolset::ReconcileMarkers(const FString& GeneratorPath)
{
	FDdReconcileReport Report;
	UWorld* World = EditorWorldOrNull();
	if (!World) { UE_LOG(LogTemp, Warning, TEXT("draftDesk reconcile: refused (PIE / no editor world).")); return Report; }
	ADraftDeskGenerator* Gen = ResolveGen(GeneratorPath, World);
	if (!Gen) { UE_LOG(LogTemp, Error, TEXT("draftDesk reconcile: no generator (path '%s')."), *GeneratorPath); return Report; }

	const FScopedTransaction Transaction(LOCTEXT("ReconcileMarkers", "draftDesk: reconcile threshold markers"));
	ReconcileInternal(Gen, World, Report);
	return Report;
}

FDdReconcileReport UDdNavToolset::SyncDrags(const FString& GeneratorPath)
{
	FDdReconcileReport Report;
	UWorld* World = EditorWorldOrNull();
	if (!World) { UE_LOG(LogTemp, Warning, TEXT("draftDesk SyncDrags: refused (PIE / no editor world).")); return Report; }
	ADraftDeskGenerator* Gen = ResolveGen(GeneratorPath, World);
	if (!Gen) { UE_LOG(LogTemp, Error, TEXT("draftDesk SyncDrags: no generator (path '%s')."), *GeneratorPath); return Report; }

	const FScopedTransaction Transaction(LOCTEXT("SyncDrags", "draftDesk: sync threshold drags"));

	// Drags only fold into a Custom (authored) layout — built-in presets re-author each rebuild, so a
	// write to AuthoredThresholds is ignored. Still reconcile so the markers re-home.
	if (Gen->Preset != EDraftDeskPreset::Custom)
	{
		UE_LOG(LogTemp, Warning, TEXT("draftDesk SyncDrags: preset layout is read-only; reconciling markers only."));
		ReconcileInternal(Gen, World, Report);
		return Report;
	}

	const FTransform GenXform = Gen->GetActorTransform();
	double Grid = 50.0;
	if (Gen->Spec)
	{
		const FVector& G = Gen->Spec->Metrics.GridSnap;
		const double M = FMath::Min(G.X, G.Y);
		if (M >= 1.0) { Grid = M; }
	}
	const double PerpTol = FMath::Max(1.0, Grid * 0.5);   // off-wall beyond this = a Stage-B reshape (3b)
	const double DragEps = 1.0;                           // detect a fresh drag (NOT the coarse drift band)
	ULevel* GenLevel = Gen->GetLevel();

	// Opening axis + the set of currently-RESOLVED thresholds (from the pre-fold Openings). ResolvedThr
	// lets the delete sweep tell an author-deleted marker (threshold still resolved -> dissolve it) from a
	// threshold that went DORMANT because a room moved off it (no opening -> keep it, it re-resolves on re-abut).
	TMap<int32, int32> AxisByThr;
	TSet<int32> ResolvedThr;
	for (const FDdOpening& O : Gen->Openings)
	{
		if (O.SourceThreshold >= 0) { AxisByThr.Add(O.SourceThreshold, O.Axis); ResolvedThr.Add(O.SourceThreshold); }
	}

	Gen->Modify();

	// FOLD pass + build the `seen` set of source thresholds still owned by a live marker.
	TSet<int32> Seen;
	for (TActorIterator<ADraftDeskThreshold> It(World); It; ++It)
	{
		ADraftDeskThreshold* M = *It;
		if (M->GetLevel() != GenLevel) { continue; }
		const int32 i = M->SourceThreshold;
		if (i >= 0) { Seen.Add(i); }
		if (i < 0 || i >= Gen->AuthoredThresholds.Num()) { continue; }   // flights / unstamped: never folded
		FDdThreshold& Th = Gen->AuthoredThresholds[i];
		if (Th.bIsEntry) { continue; }                                   // R1: the entry is the normalize origin, never folded
		const int32* AxP = AxisByThr.Find(i);
		if (!AxP || (*AxP != 0 && *AxP != 1)) { continue; }              // 3a folds vertical walls only

		const FVector Delta = GenXform.InverseTransformPosition(M->GetActorLocation()) - M->ReconciledLocation;
		if (Delta.Size() <= DragEps) { continue; }                       // no fresh drag

		// Project the drag delta onto the wall (relative to the last reconciled home — immune to the
		// snapped plane drifting under the marker). axis 0 = const-X wall slides in Y; 1 = const-Y in X.
		double AlongDelta, PerpH;
		if (*AxP == 0) { AlongDelta = Delta.Y; PerpH = Delta.X; }   // const-X wall: along=Y, off-wall=X
		else           { AlongDelta = Delta.X; PerpH = Delta.Y; }   // const-Y wall: along=X, off-wall=Y

		if (FMath::Abs(PerpH) > PerpTol)
		{
			// perpendicular off the wall -> RESHAPE: move the shared wall so both rooms reform around it
			if (TryReshape(Gen, M->SourceThreshold, *AxP, PerpH, Grid)) { ++Report.Reshaped; }
			else { ++Report.Rejected; } // connection would break / room degenerate -> reverted, marker snaps back
			continue; // a tick is a reshape OR an along-slide, never both
		}
		Th.Position += AlongDelta; // along-wall slide (relative; carve re-clamps to the wall)
		++Report.Folded;
	}

	// ROOM-HANDLE translate + auto-gain. A dragged room handle moves the WHOLE space; the drag folds as a
	// relative delta from the handle's reconciled home (immune to the frozen origin), grid-snapped on the
	// RESULT in the authored frame (a room on-grid stays on-grid, W/D preserved). XY only here (Z->Level is C).
	//
	// Phase 1: collect each dragged handle's delta and snapshot its pre-move same-level abutting set BEFORE
	// any write, so a multi-handle drag compares against the original (uncontaminated) frame.
	struct FMovedRoom { int32 RoomIndex; float Dx; float Dy; TSet<int32> Before; };
	TArray<FMovedRoom> Moved;
	for (TActorIterator<ADraftDeskRoomHandle> It(World); It; ++It)
	{
		ADraftDeskRoomHandle* H = *It;
		if (H->GetLevel() != GenLevel) { continue; }
		const int32 RI = H->RoomIndex;
		if (RI < 0 || RI >= Gen->AuthoredRooms.Num()) { continue; }
		const FVector Delta = GenXform.InverseTransformPosition(H->GetActorLocation()) - H->ReconciledLocation;
		if (FVector2D(Delta.X, Delta.Y).Size() <= DragEps) { continue; }   // no fresh XY drag
		FMovedRoom MR; MR.RoomIndex = RI; MR.Dx = (float)Delta.X; MR.Dy = (float)Delta.Y;
		TArray<int32> Ab; Gen->AbuttingRooms(RI, Ab); MR.Before.Append(Ab);
		Moved.Add(MR);
	}
	// Phase 2: apply every translate (grid-snap the RESULT in the authored frame).
	{
		auto SnapAuthored = [Grid](float v) { return (Grid > 1.0) ? FMath::GridSnap(v, (float)Grid) : v; };
		for (const FMovedRoom& MR : Moved)
		{
			FDdRoom& R = Gen->AuthoredRooms[MR.RoomIndex];
			R.Min.X = SnapAuthored(R.Min.X + MR.Dx); R.Max.X = SnapAuthored(R.Max.X + MR.Dx);
			R.Min.Y = SnapAuthored(R.Min.Y + MR.Dy); R.Max.Y = SnapAuthored(R.Max.Y + MR.Dy);
			++Report.RoomTranslated;
		}
	}
	// Phase 3: AUTO-GAIN a Doorway on any NEWLY-formed same-level abutment (after \ before), unless the pair
	// is already wall-connected. Canonical (min,max) pair so the ordered opening label is stable and the
	// existence guard dedups a re-abut. Seed the fresh index into Seen so the DELETE sweep can't dissolve a
	// marker-less brand-new threshold this pass; it resolves + gets a marker on the rebuild. (A door that
	// DETACHED stays dormant; this only ADDS, per the detach+reconnect model. New-abutment = a Doorway.)
	for (const FMovedRoom& MR : Moved)
	{
		TArray<int32> After; Gen->AbuttingRooms(MR.RoomIndex, After);
		for (int32 Other : After)
		{
			if (MR.Before.Contains(Other)) { continue; }                 // not a NEW abutment
			const int32 A = FMath::Min(MR.RoomIndex, Other), B = FMath::Max(MR.RoomIndex, Other);
			if (Gen->VerticalLinkExists(A, B)) { continue; }             // already connected -> no dup
			FDdThreshold Door;
			Door.RoomA = A; Door.RoomB = B;
			Door.Kind = EDdThresholdKind::Doorway; Door.Plane = EDdPlaneClass::Vertical;
			const int32 NewIdx = Gen->AuthoredThresholds.Add(Door);
			Seen.Add(NewIdx);
			++Report.Gained;
		}
	}

	// DELETE sweep: an interior threshold with no live marker dissolves to a full-clear Passage. Merging
	// (not removing) keeps AuthoredThresholds indices stable so other markers' SourceThreshold stays valid.
	for (int32 i = 0; i < Gen->AuthoredThresholds.Num(); ++i)
	{
		if (Seen.Contains(i)) { continue; }
		FDdThreshold& Th = Gen->AuthoredThresholds[i];
		if (Th.bIsEntry || Th.RoomB == INDEX_NONE || Th.Kind == EDdThresholdKind::Rail)
		{
			++Report.Kept; continue;   // R1: entry / exterior / rail are never auto-removed
		}
		if (!ResolvedThr.Contains(i))
		{
			++Report.Kept; continue;   // DORMANT (no opening): a room moved off it; keep as-is, it re-resolves on re-abut
		}
		Th.Kind = EDdThresholdKind::Passage; Th.Width = 0.f; Th.Height = 0.f;
		++Report.Merged;
	}

	Gen->RerunConstructionScripts();   // rebuild -> refills Openings (Rebuild is private; this is the public route)
	ReconcileInternal(Gen, World, Report);
	return Report;
}

#undef LOCTEXT_NAMESPACE
