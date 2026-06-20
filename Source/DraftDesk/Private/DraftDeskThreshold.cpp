#include "DraftDeskThreshold.h"

#include "Components/BillboardComponent.h"
#include "UObject/ConstructorHelpers.h"

ADraftDeskThreshold::ADraftDeskThreshold()
{
	PrimaryActorTick.bCanEverTick = false;
	bIsEditorOnlyActor = true; // an authoring anchor: never present in game / PIE

	Icon = CreateDefaultSubobject<UBillboardComponent>(TEXT("Icon"));
	SetRootComponent(Icon);
	Icon->SetMobility(EComponentMobility::Movable);          // grabbable with the move/rotate gizmo
	Icon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Icon->SetCastShadow(false);
	Icon->bHiddenInGame = true;

#if WITH_EDITORONLY_DATA
	static ConstructorHelpers::FObjectFinder<UTexture2D> SpriteTex(TEXT("/Engine/EditorResources/S_TargetPoint.S_TargetPoint"));
	if (SpriteTex.Succeeded())
	{
		Icon->SetSprite(SpriteTex.Object);
	}
	Icon->bIsScreenSizeScaled = true; // stays a readable size at any zoom, like other editor icons
#endif
}
