// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "LedgeAnimInstance.generated.h"

class ALedgeCharacter;
class UCharacterMovementComponent;

UCLASS()
class LEDGEDETECTION_API ULedgeAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	ULedgeAnimInstance();

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Character")
	TObjectPtr<ALedgeCharacter> LedgeCharacter;

	UPROPERTY(BlueprintReadOnly, Category = "Character")
	TObjectPtr<UCharacterMovementComponent> MovementComponent;

	/** Character ground speed (horizontal velocity magnitude) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float GroundSpeed;

	/** Whether the character has movement input or speed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	bool bShouldMove;

	/** Whether character is falling / in air */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	bool bIsFalling;

	/** Whether character is currently vaulting or mantling */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	bool bIsLedgeTransitioning;
};
