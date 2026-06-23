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
	static ConstructorHelpers::FObjectFinder<UTexture2D> ValidTex(TEXT("/Engine/EditorResources/S_TargetPoint.S_TargetPoint"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> InvalidTex(TEXT("/Engine/EditorResources/S_Note.S_Note"));
	ValidSprite = ValidTex.Succeeded() ? ValidTex.Object : nullptr;
	InvalidSprite = InvalidTex.Succeeded() ? InvalidTex.Object : nullptr;   // distinct "invalid" glyph (red-X texture is a follow-up)
	if (ValidSprite) { Icon->SetSprite(ValidSprite); }
	Icon->bIsScreenSizeScaled = true; // stays a readable size at any zoom, like other editor icons
#endif
}

void ADraftDeskThreshold::RefreshInvalidVisual()
{
#if WITH_EDITORONLY_DATA
	if (!Icon) { return; }
	UTexture2D* const Want = bInvalid ? InvalidSprite : ValidSprite;   // a broken connection reads distinct, not gone
	if (Want) { Icon->SetSprite(Want); }
#endif
}
