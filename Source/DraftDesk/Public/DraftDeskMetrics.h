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

	// --- Authoring grid ---
	/** The blocking grid (cm). Room footprints, openings, and floor heights snap to this in X/Y/Z, and
	 *  WallThickness rounds UP to a whole cell so abutting room faces stay grid-aligned and shared walls
	 *  still dedup. Generally keep X=Y=Z (one locked grid); set per-axis only if you must. A 0 on an axis
	 *  disables snap there (escape hatch). Precision over artistic control: author to the grid, or the
	 *  tool rounds you onto it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid", meta = (Units = "cm"))
	FVector GridSnap = FVector(50.f, 50.f, 50.f);

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

	// --- Locomotion (drives the CharacterMovementComponent via the GameMode) ---
	/** Slow / "walk" ground speed, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion", meta = (ClampMin = "0"))
	float WalkSpeed = 300.f;

	/** Default / "run" ground speed, cm/s. Becomes MaxWalkSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion", meta = (ClampMin = "0"))
	float RunSpeed = 600.f;

	/** Jump launch velocity, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion", meta = (ClampMin = "0"))
	float JumpZVelocity = 420.f;

	/** First-person camera field of view, degrees. Applied to the pawn camera on spawn — tune it
	 *  alongside RunSpeed and door size (faster movement / wider apertures usually want more FOV). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion", meta = (ClampMin = "30", ClampMax = "170", Units = "deg"))
	float FOV = 90.f;
};
