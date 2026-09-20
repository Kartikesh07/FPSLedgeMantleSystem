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

	/** Maximum mantle height (cm) from feet */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge Detection|Config")
	float MaxMantleHeight = 225.0f;

	/** Minimum mantle height (cm) from feet */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge Detection|Config")
	float MinMantleHeight = 50.0f;

	/** Distance to trace forward looking for a wall */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge Detection|Config")
	float ForwardTraceDistance = 85.0f;

	/** Radius of the downward sphere trace */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge Detection|Config")
	float DownwardTraceRadius = 15.0f;

	/** Inward push offset past the ledge edge to land the capsule stably with realistic arm reach */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge Detection|Config")
	float LedgeInwardOffset = 6.0f;

	/** Collision channel used for wall and ledge traces */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge Detection|Config")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Whether to draw debug lines, spheres, and target capsule */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge Detection|Debug")
	bool bDrawDebug = true;

	/** Draw debug lifetime in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge Detection|Debug")
	float DebugDrawDuration = 5.0f;

	/** Primary function to check if a valid ledge exists ahead of the character */
	UFUNCTION(BlueprintCallable, Category = "Ledge Detection")
	bool DetectLedge(FMantleLedgeInfo& OutLedgeInfo);

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<ACharacter> CharacterOwner;

	/** Helper to perform the forward wall trace */
	bool TraceForwardForWall(const FVector& TraceDirection, FHitResult& OutHit);

	/** Helper to perform the downward ledge trace */
	bool TraceDownForLedge(const FHitResult& WallHit, FHitResult& OutHit);

	/** Helper to verify the capsule has enough clearance at the target destination */
	bool CheckCapsuleClearance(const FVector& TargetLocation, float CapsuleRadius, float CapsuleHalfHeight) const;
};
