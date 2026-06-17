#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DraftDeskStatics.generated.h"

class APawn;
class UDraftDeskSpec;

/** Helpers for applying a draftDesk spec to gameplay. Lets any GameMode/Character consume the spec without inheriting a plugin base. */
UCLASS()
class DRAFTDESK_API UDraftDeskStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Push a spec's Locomotion metrics onto a pawn's CharacterMovementComponent
	 * (RunSpeed -> MaxWalkSpeed, JumpZVelocity). Returns true if applied.
	 */
	UFUNCTION(BlueprintCallable, Category = "draftDesk")
	static bool ApplyLocomotion(APawn* Pawn, const UDraftDeskSpec* Spec);
};
