// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LedgeDetectionTypes.h"
#include "LedgeDetectionComponent.generated.h"

class ACharacter;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LEDGEDETECTION_API ULedgeDetectionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULedgeDetectionComponent();

	/** Forward reach distance for detecting wall surfaces */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge Detection|Dimensions")
	float ForwardTraceDistance = 100.0f;

	/** Minimum height above player feet to be considered a ledge (below this is standard step-up) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge Detection|Dimensions")
	float MinLedgeHeight = 50.0f;

	/** Maximum reach height above player feet (high pull-up reach) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge Detection|Dimensions")
	float MaxLedgeHeight = 225.0f;

	/** Maximum ledge height that qualifies for a vault instead of a mantle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge Detection|Dimensions")
	float MaxVaultHeight = 100.0f;

	/** Maximum obstacle thickness (depth) that can be vaulted over */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge Detection|Dimensions")
	float MaxVaultThickness = 120.0f;

	/** Inward distance behind the wall face to trace downward for the top surface */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge Detection|Dimensions")
	float LedgeDepthOffset = 25.0f;

	/** Collision channel used for traces */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge Detection|Collision")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Enable debug visualization of traces, hits, and landing spots */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge Detection|Debug")
	bool bDrawDebug = true;

	/** Duration (seconds) for debug lines and markers */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge Detection|Debug")
	float DebugDrawDuration = 4.0f;

	/** Primary detection function. Returns true if a valid, cleared ledge is found */
	UFUNCTION(BlueprintCallable, Category = "Ledge Detection")
	bool DetectLedge(FLedgeDetectionResult& OutResult);

	/** Returns true if character is currently performing a mantle or vault */
	UFUNCTION(BlueprintCallable, Category = "Ledge Transition")
	bool IsTransitioning() const { return bIsTransitioning; }

	/** Starts smooth procedural transition for the detected ledge */
	UFUNCTION(BlueprintCallable, Category = "Ledge Transition")
	bool StartTransition(const FLedgeDetectionResult& Result);

	/** Cancels an active transition and restores walking mode */
	UFUNCTION(BlueprintCallable, Category = "Ledge Transition")
	void CancelTransition();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Duration (seconds) for vaulting over thin obstacles */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge Transition|Durations")
	float VaultDuration = 0.42f;

	/** Duration (seconds) for low step-up mantles */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge Transition|Durations")
	float LowMantleDuration = 0.5f;

	/** Duration (seconds) for high pull-up mantles */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge Transition|Durations")
	float HighMantleDuration = 0.75f;

	/** Maximum downward camera offset in cm during mantle to simulate weight transfer */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge Transition|Camera")
	float CameraDipMaxOffsetZ = 8.0f;

protected:
	virtual void BeginPlay() override;

	/** Reference to owning character */
	UPROPERTY()
	TObjectPtr<ACharacter> CharacterOwner;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ledge Transition")
	bool bIsTransitioning = false;

private:
	bool ForwardTrace(FHitResult& OutHit, FVector& OutForwardDir);
	bool DownwardTrace(const FVector& WallImpactPoint, const FVector& WallNormal, FHitResult& OutHit);
	bool CheckCapsuleClearance(const FVector& TargetLocation);
	bool CheckVaultClearance(const FVector& WallImpactPoint, const FVector& ForwardDir, float ObstacleTopZ, FVector& OutVaultLandingLocation);
	void DrawDebugVisuals(const FLedgeDetectionResult& Result);

	// Transition trajectory state
	float TransitionTimeElapsed = 0.0f;
	float CurrentTransitionDuration = 0.5f;
	FVector TransitionStartLocation = FVector::ZeroVector;
	FVector TransitionControlLocation = FVector::ZeroVector;
	FVector TransitionTargetLocation = FVector::ZeroVector;
	FRotator TransitionTargetRotation = FRotator::ZeroRotator;
	ELedgeActionType CurrentAction = ELedgeActionType::None;
	FVector VaultForwardDirection = FVector::ZeroVector;
	float VaultExitSpeed = 600.0f;

	void UpdateTransition(float DeltaTime);
	void FinishTransition();
	void ApplyCameraOffset(float Alpha);
	void ResetCameraOffset();
};
