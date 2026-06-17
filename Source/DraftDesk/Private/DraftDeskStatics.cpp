#include "DraftDeskStatics.h"

#include "DraftDeskSpec.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

bool UDraftDeskStatics::ApplyLocomotion(APawn* Pawn, const UDraftDeskSpec* Spec)
{
	if (!Pawn || !Spec)
	{
		return false;
	}

	ACharacter* Character = Cast<ACharacter>(Pawn);
	if (!Character)
	{
		return false;
	}

	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (!Movement)
	{
		return false;
	}

	const FDraftDeskMetrics& M = Spec->Metrics;
	// "Run" is the default top ground speed; walk/sprint states interpolate between Walk and Run downstream.
	Movement->MaxWalkSpeed = M.RunSpeed;
	Movement->MaxWalkSpeedCrouched = M.WalkSpeed;
	Movement->JumpZVelocity = M.JumpZVelocity;
	return true;
}
