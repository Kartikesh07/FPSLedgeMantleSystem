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
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	/** Owner character reference */
	UPROPERTY(BlueprintReadOnly, Category = "Character")
	TObjectPtr<ALedgeCharacter> Character;

	/** Character movement component reference */
	UPROPERTY(BlueprintReadOnly, Category = "Character")
	TObjectPtr<UCharacterMovementComponent> MovementComponent;

	/** Current 2D ground speed of the character */
	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	float GroundSpeed = 0.0f;

	/** Whether the character has movement input / intent to move */
	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	bool bShouldMove = false;

	/** Whether the character is currently in the air / falling */
	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	bool bIsFalling = false;

	/** Whether the character is currently mantling an obstacle */
	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	bool bIsMantling = false;

	/** Current character velocity vector */
	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	FVector Velocity = FVector::ZeroVector;

	// --- Hand & Elbow IK ---
 
	/** Target location for Left Hand IK in Component Space */
	UPROPERTY(BlueprintReadOnly, Category = "Mantle|IK")
	FVector LeftHandIK_Location = FVector::ZeroVector;

	/** Target location for Right Hand IK in Component Space */
	UPROPERTY(BlueprintReadOnly, Category = "Mantle|IK")
	FVector RightHandIK_Location = FVector::ZeroVector;

	/** Target location for Left Elbow Joint Target in Component Space */
	UPROPERTY(BlueprintReadOnly, Category = "Mantle|IK")
	FVector LeftElbowIK_Location = FVector::ZeroVector;

	/** Target location for Right Elbow Joint Target in Component Space */
	UPROPERTY(BlueprintReadOnly, Category = "Mantle|IK")
	FVector RightElbowIK_Location = FVector::ZeroVector;

	/** Blending weight for Hand IK (0.0 = disabled, 1.0 = fully locked to ledge) */
	UPROPERTY(BlueprintReadOnly, Category = "Mantle|IK")
	float HandIK_Weight = 0.0f;
};
