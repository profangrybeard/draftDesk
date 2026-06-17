#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DraftDeskGameMode.generated.h"

class UDraftDeskSpec;

/**
 * Optional base GameMode that holds the active spec and applies its Locomotion metrics
 * to every player it spawns. The GameMode lives only on the server, so this is the
 * authoritative single reference point for game-feel metrics. (Your own GameMode can
 * instead just hold a Spec and call UDraftDeskStatics::ApplyLocomotion.)
 */
UCLASS()
class DRAFTDESK_API ADraftDeskGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	/** Single source of truth for this game's metrics. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk")
	TObjectPtr<UDraftDeskSpec> Spec;

	virtual void RestartPlayer(AController* NewPlayer) override;
};
