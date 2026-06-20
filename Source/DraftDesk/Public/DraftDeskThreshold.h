#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DraftDeskLayout.h"
#include "DraftDeskThreshold.generated.h"

class UBillboardComponent;

/**
 * An author-movable threshold anchor — the authoring surface for draftDesk.
 *
 * Drag it at edit time to place a connection between spaces; on "sync" the generator reads the
 * placed thresholds and builds the layout around them (threshold = input, geometry = output).
 * Editor-only: a camera-facing icon with the standard move/rotate gizmo, no collision, not present
 * in game, no light or shadow. It draws over geometry, so it stays visible once the kit has roofs.
 */
UCLASS()
class DRAFTDESK_API ADraftDeskThreshold : public AActor
{
	GENERATED_BODY()

public:
	ADraftDeskThreshold();

	/** What kind of connection this threshold is. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Threshold")
	EDraftDeskLinkKind Kind = EDraftDeskLinkKind::Doorway;

	/** Clear width of the opening (cm); 0 => the spec's metric default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Threshold", meta = (ClampMin = "0", Units = "cm"))
	float Width = 0.f;

	/** Clear height of the opening (cm); 0 => the spec's metric default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Threshold", meta = (ClampMin = "0", Units = "cm"))
	float Height = 0.f;

	/** Author-facing label, e.g. "great hall -> guard". Helps read the flow graph at a glance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Threshold")
	FName Label;

protected:
	/** Camera-facing editor icon; the actor's movable root (select it for the gizmo). */
	UPROPERTY(VisibleAnywhere, Category = "Threshold")
	TObjectPtr<UBillboardComponent> Icon;
};
