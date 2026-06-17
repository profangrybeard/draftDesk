#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DraftDeskMetrics.h"
#include "DraftDeskSpec.generated.h"

/**
 * The authored "spec sheet" — one Data Asset that is the single source of truth for a
 * game's metrics. Consumed by ADraftDeskGenerator (geometry) and ADraftDeskGameMode
 * (game feel). Edit it once; the blockout and the movement both follow.
 */
UCLASS(BlueprintType)
class DRAFTDESK_API UDraftDeskSpec : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "draftDesk", meta = (ShowOnlyInnerProperties))
	FDraftDeskMetrics Metrics;
};
