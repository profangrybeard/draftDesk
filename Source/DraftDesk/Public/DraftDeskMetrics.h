#pragma once

#include "CoreMinimal.h"
#include "DraftDeskMetrics.generated.h"

/**
 * Player-movement / architectural metrics that drive a blockout. Units: cm.
 * Mirrors LD_Metrics.json. Edit these in the Details panel of a DraftDeskGenerator.
 */
USTRUCT(BlueprintType)
struct FDraftDeskMetrics
{
	GENERATED_BODY()

	// --- Doors ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Doors", meta = (ClampMin = "40", UIMin = "40", Units = "cm"))
	float DoorWidth = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Doors", meta = (ClampMin = "60", UIMin = "60", Units = "cm"))
	float DoorHeight = 220.f;

	// --- Circulation ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Circulation", meta = (ClampMin = "60", Units = "cm"))
	float CorridorWidth = 200.f;

	// --- Vertical ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vertical", meta = (ClampMin = "100", Units = "cm"))
	float CeilingMin = 300.f;

	// --- Steps & ramps ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steps", meta = (ClampMin = "1", Units = "cm"))
	float StepRise = 18.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steps", meta = (ClampMin = "1", Units = "cm"))
	float StepRun = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Steps", meta = (ClampMin = "0", ClampMax = "89", Units = "deg"))
	float MaxStepTraversalAngle = 40.f;

	// --- Cover ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cover", meta = (ClampMin = "10", Units = "cm"))
	float HalfWallHeight = 100.f;

	// --- Vault / window ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vault", meta = (ClampMin = "10", Units = "cm"))
	float VaultHeight = 110.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vault", meta = (ClampMin = "10", Units = "cm"))
	float WindowClearHeight = 130.f;

	// --- Jump ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump", meta = (ClampMin = "0", Units = "cm"))
	float JumpUpHeight = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump", meta = (ClampMin = "0", Units = "cm"))
	float JumpGapDistance = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump", meta = (ClampMin = "10", Units = "cm"))
	float MantleMaxHeight = 150.f;

	// --- Grapple ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple", meta = (ClampMin = "0", Units = "cm"))
	float GrappleMaxHeight = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple", meta = (ClampMin = "0", Units = "cm"))
	float GrappleMaxDistance = 3000.f;

	// --- Player ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player", meta = (ClampMin = "10", Units = "cm"))
	float PlayerCapsuleHalfHeight = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player", meta = (ClampMin = "5", Units = "cm"))
	float PlayerCapsuleRadius = 34.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player", meta = (ClampMin = "10", Units = "cm"))
	float PlayerEyeHeight = 160.f;
};
