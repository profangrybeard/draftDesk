// Copyright draftDesk.

#include "Modules/ModuleManager.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "DdNavToolset.h"
#include "DraftDeskGenerator.h"
#include "DraftDeskThreshold.h"
#include "DraftDeskRoomHandle.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

// One SyncDrags is queued per gizmo release. A multi-select drag fires OnActorMoved once PER actor,
// sequentially (each PostEditMove completes before the next), so a nested-reentrancy guard would NOT
// coalesce them — they're siblings. A next-tick latch does: the first move this frame queues one deferred
// SyncDrags, the rest see the latch set and return. One pass, one undo transaction, every marker's FINAL
// dragged position read against its pre-drag baseline. (Re-armed after the deferred sync completes.)
namespace { bool GDdSyncQueued = false; }

// Editor-only module: hosts draftDesk's MCP toolsets (registered explicitly — not auto-discovered) and
// wires LIVE authoring (moving a threshold marker folds+rebuilds+reconciles with no command).
class FDraftDeskEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UToolsetRegistry::RegisterToolsetClass(UDdNavToolset::StaticClass());
		if (GEditor)
		{
			ActorMovedHandle = GEditor->OnActorMoved().AddRaw(this, &FDraftDeskEditorModule::HandleActorMoved);
		}
	}

	virtual void ShutdownModule() override
	{
		if (GEditor && ActorMovedHandle.IsValid())
		{
			GEditor->OnActorMoved().Remove(ActorMovedHandle);
		}
		ActorMovedHandle.Reset();
		UToolsetRegistry::UnregisterToolsetClass(UDdNavToolset::StaticClass());
	}

private:
	FDelegateHandle ActorMovedHandle;

	void HandleActorMoved(AActor* Actor)
	{
		if (!Actor) { return; }
		// Discriminate by which cast succeeds: a threshold MARKER (moves a connection) or a ROOM HANDLE
		// (moves the space). Anything else is ignored. Both route to the same coalesced SyncDrags.
		ADraftDeskThreshold* M = Cast<ADraftDeskThreshold>(Actor);
		ADraftDeskRoomHandle* H = M ? nullptr : Cast<ADraftDeskRoomHandle>(Actor);
		if (!M && !H) { return; }
		if (M && M->SourceFlight >= 0) { return; }            // flight markers are locked / derived — never folded
		if (!GEditor || GEditor->PlayWorld != nullptr || GIsPlayInEditorWorld) { return; } // editor world only
		UWorld* W = GEditor->GetEditorWorldContext().World();
		if (!W || W->WorldType != EWorldType::Editor || Actor->GetWorld() != W) { return; }
		if (GDdSyncQueued) { return; }                        // ONE latch coalesces a mixed marker+handle drag

		ADraftDeskGenerator* Gen = nullptr;
		for (TActorIterator<ADraftDeskGenerator> It(W); It; ++It)
		{
			if (It->GetLevel() == Actor->GetLevel()) { Gen = *It; break; }
		}
		if (!Gen) { return; }

		const FString Path = Gen->GetPathName();
		GDdSyncQueued = true;
		// Defer to next tick: the gizmo teardown (which fired this) fully completes first, so SyncDrags'
		// RerunConstructionScripts + reconcile don't run inside the interaction's own stack.
		GEditor->GetTimerManager()->SetTimerForNextTick([Path]()
		{
			GDdSyncQueued = false;
			UDdNavToolset::SyncDrags(Path);
		});
	}
};

IMPLEMENT_MODULE(FDraftDeskEditorModule, DraftDeskEditor);
