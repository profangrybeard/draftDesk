// Copyright draftDesk.

#include "DdNavToolset.h"

#include "Editor.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"

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
