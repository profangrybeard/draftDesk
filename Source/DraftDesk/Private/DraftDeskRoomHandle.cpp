#include "DraftDeskRoomHandle.h"

#include "Components/BillboardComponent.h"
#include "UObject/ConstructorHelpers.h"

ADraftDeskRoomHandle::ADraftDeskRoomHandle()
{
	PrimaryActorTick.bCanEverTick = false;
	bIsEditorOnlyActor = true; // an authoring handle: never present in game / PIE

	Icon = CreateDefaultSubobject<UBillboardComponent>(TEXT("Icon"));
	SetRootComponent(Icon);
	Icon->SetMobility(EComponentMobility::Movable);          // grabbable with the move gizmo
	Icon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Icon->SetCastShadow(false);
	Icon->bHiddenInGame = true;

#if WITH_EDITORONLY_DATA
	// A DISTINCT glyph from the threshold marker's S_TargetPoint, so "move the room" reads apart from
	// "move the door" at a glance. (If the asset is missing the handle is still a selectable actor.)
	static ConstructorHelpers::FObjectFinder<UTexture2D> SpriteTex(TEXT("/Engine/EditorResources/S_Actor.S_Actor"));
	if (SpriteTex.Succeeded())
	{
		Icon->SetSprite(SpriteTex.Object);
	}
	Icon->bIsScreenSizeScaled = true; // stays a readable size at any zoom, like other editor icons
#endif
}
