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
			// FindPathToLocationSynchronously projects the endpoints onto the navmesh and pathfinds
			// on the live nav data; a partial path means the target could not actually be reached.
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

FDdReconcileReport UDdNavToolset::ReconcileMarkers(const FString& GeneratorPath)
{
	FDdReconcileReport Report;

	// Editor-world ONLY: never spawn/delete into a PIE-duplicated world (markers would be discarded on
	// PIE exit, leaving openings marker-less). Adversarial-pass fix #3.
	if (!GEditor || GEditor->PlayWorld != nullptr || GIsPlayInEditorWorld)
	{
		UE_LOG(LogTemp, Warning, TEXT("draftDesk reconcile: refused (PIE active or no editor)."));
		return Report;
	}
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World || World->WorldType != EWorldType::Editor)
	{
		return Report;
	}

	ADraftDeskGenerator* Gen = FindObject<ADraftDeskGenerator>(nullptr, *GeneratorPath);
	if (!Gen)
	{
		for (TActorIterator<ADraftDeskGenerator> It(World); It; ++It) { Gen = *It; break; }
	}
	if (!Gen)
	{
		UE_LOG(LogTemp, Error, TEXT("draftDesk reconcile: no generator (path '%s')."), *GeneratorPath);
		return Report;
	}

	ULevel* GenLevel = Gen->GetLevel();
	const TArray<FDdOpening>& Openings = Gen->Openings;
	const FTransform GenXform = Gen->GetActorTransform();
	Report.Total = Openings.Num();

	// Desired markers, keyed by Label. A colliding label (two thresholds sharing a wall mint the same
	// "A-B") is a build smell: keep first, count loud. The gate is multiset-aware so it can't false-pass.
	TMap<FName, const FDdOpening*> Desired;
	for (const FDdOpening& O : Openings)
	{
		const FName Key(*O.Label);
		if (Desired.Contains(Key))
		{
			++Report.Duplicates;
			UE_LOG(LogTemp, Error, TEXT("draftDesk reconcile: duplicate opening label '%s' kept-first (slice-3 SourceId needed)."), *O.Label);
			continue;
		}
		Desired.Add(Key, &O);
	}

	// Collect this generator's marker actors (scoped to its level), as a snapshot so we can delete safely.
	TArray<ADraftDeskThreshold*> Markers;
	for (TActorIterator<ADraftDeskThreshold> It(World); It; ++It)
	{
		if (It->GetLevel() == GenLevel) { Markers.Add(*It); }
	}

	const FScopedTransaction Transaction(LOCTEXT("ReconcileMarkers", "draftDesk: reconcile threshold markers"));
	const double Eps = 1.0; // cm — fine enough to correct the ~25cm pre-snap seed drift. slice 3 adds drag arbitration.

	TSet<FName> Claimed;
	for (ADraftDeskThreshold* M : Markers)
	{
		const FName L = M->Label;
		const FDdOpening* const* Found = Desired.Find(L);
		if (!Found)
		{
			// Orphan: label not in current Openings. Guard the entry (R1) and rails (caps oscillate
			// resolved/unresolved with no-drop edits) — never delete those on a transient miss.
			if (L == FName(TEXT("entry")) || M->Kind == EDdThresholdKind::Rail)
			{
				++Report.Kept;
				UE_LOG(LogTemp, Warning, TEXT("draftDesk reconcile: kept '%s' (entry/rail guard) — absent from openings."), *L.ToString());
				continue;
			}
			M->Modify();
			World->EditorDestroyActor(M,/*bShouldModifyLevel*/ true);
			++Report.Deleted;
			continue;
		}
		if (Claimed.Contains(L))
		{
			// Duplicate marker for one opening (double-seed / copy-paste / undo dup): keep first, delete rest.
			M->Modify();
			World->EditorDestroyActor(M,true);
			++Report.Deleted;
			continue;
		}
		Claimed.Add(L);
		const FDdOpening& O = **Found;
		const FVector CurLocal = GenXform.InverseTransformPosition(M->GetActorLocation());
		if (FVector::Dist(CurLocal, O.Position) > Eps)
		{
			M->Modify();
			M->SetActorLocation(GenXform.TransformPosition(O.Position));
			M->Kind = O.Kind; // realign identity-ish field; leave author-editable Width/Height to the author
			++Report.Moved;
		}
		else
		{
			++Report.Kept;
		}
	}

	// Spawn one marker for every opening no marker claimed (closes Openings - markers => map is TOTAL).
	for (const TPair<FName, const FDdOpening*>& Pair : Desired)
	{
		if (Claimed.Contains(Pair.Key)) { continue; }
		const FDdOpening& O = *Pair.Value;

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transactional; // persistent, undoable — NOT RF_Transient
		Params.OverrideLevel = GenLevel;
		ADraftDeskThreshold* M = World->SpawnActor<ADraftDeskThreshold>(
			GenXform.TransformPosition(O.Position), FRotator::ZeroRotator, Params);
		if (!M) { continue; }

		M->SetActorLabel(FString(TEXT("DD_TH_")) + O.Label); // Outliner name
		M->Label = Pair.Key;                                  // identity key
		M->Kind = O.Kind;
		M->Plane = O.Plane;
		M->RoomA = O.RoomA;
		M->RoomB = O.RoomB;
		M->Width = O.Width;
		M->Height = O.Height;
		M->Sill = O.Sill;
		M->bIsEntry = O.bIsEntry;
		++Report.Spawned;
	}

	if (GenLevel) { GenLevel->MarkPackageDirty(); }
	return Report;
}

#undef LOCTEXT_NAMESPACE
