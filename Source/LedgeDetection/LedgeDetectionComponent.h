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

protected:
	virtual void BeginPlay() override;

	/** Reference to owning character */
	UPROPERTY()
	TObjectPtr<ACharacter> CharacterOwner;

private:
	bool ForwardTrace(FHitResult& OutHit, FVector& OutForwardDir);
	bool DownwardTrace(const FVector& WallImpactPoint, const FVector& WallNormal, FHitResult& OutHit);
	bool CheckCapsuleClearance(const FVector& TargetLocation);
	bool CheckVaultClearance(const FVector& WallImpactPoint, const FVector& ForwardDir, float ObstacleTopZ, FVector& OutVaultLandingLocation);
	void DrawDebugVisuals(const FLedgeDetectionResult& Result);
};
