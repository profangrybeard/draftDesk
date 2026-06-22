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
				M->Modify(); World->EditorDestroyActor(M, true); ++Report.Deleted; continue;
			}
			if (Claimed.Contains(L)) { M->Modify(); World->EditorDestroyActor(M, true); ++Report.Deleted; continue; }
			Claimed.Add(L);
			const FDdOpening& O = **Found;
			M->Modify();
			M->SourceThreshold = O.SourceThreshold; M->SourceFlight = O.SourceFlight;
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
			Home(M);
			++Report.Spawned;
		}

		if (GenLevel) { GenLevel->MarkPackageDirty(); }
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

	// Opening axis per source threshold (from the current, pre-fold Openings).
	TMap<int32, int32> AxisByThr;
	for (const FDdOpening& O : Gen->Openings)
	{
		if (O.SourceThreshold >= 0) { AxisByThr.Add(O.SourceThreshold, O.Axis); }
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
		double AlongDelta, Perp;
		if (*AxP == 0) { AlongDelta = Delta.Y; Perp = FMath::Sqrt(Delta.X * Delta.X + Delta.Z * Delta.Z); }
		else           { AlongDelta = Delta.X; Perp = FMath::Sqrt(Delta.Y * Delta.Y + Delta.Z * Delta.Z); }

		if (Perp > PerpTol) { ++Report.Reshaped; continue; }             // perpendicular -> Stage-B (3b); reconcile snaps back
		Th.Position += AlongDelta;                                       // RELATIVE fold; raw (carve re-clamps to the wall)
		++Report.Folded;
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
		Th.Kind = EDdThresholdKind::Passage; Th.Width = 0.f; Th.Height = 0.f;
		++Report.Merged;
	}

	Gen->RerunConstructionScripts();   // rebuild -> refills Openings (Rebuild is private; this is the public route)
	ReconcileInternal(Gen, World, Report);
	return Report;
}

#undef LOCTEXT_NAMESPACE
