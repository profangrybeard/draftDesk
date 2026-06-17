#include "DraftDeskGameMode.h"

#include "DraftDeskStatics.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

void ADraftDeskGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);

	if (NewPlayer && Spec)
	{
		UDraftDeskStatics::ApplyLocomotion(NewPlayer->GetPawn(), Spec);
	}
}
