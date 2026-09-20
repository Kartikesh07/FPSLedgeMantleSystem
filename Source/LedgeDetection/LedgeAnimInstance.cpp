// Copyright Epic Games, Inc. All Rights Reserved.

#include "LedgeAnimInstance.h"
#include "LedgeCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

void ULedgeAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	Character = Cast<ALedgeCharacter>(TryGetPawnOwner());
	if (Character)
	{
		MovementComponent = Character->GetCharacterMovement();
	}
}

void ULedgeAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!Character)
	{
		Character = Cast<ALedgeCharacter>(TryGetPawnOwner());
		if (Character)
		{
			MovementComponent = Character->GetCharacterMovement();
		}
	}

	if (!Character || !MovementComponent)
	{
		return;
	}

	// Update movement metrics
	Velocity = MovementComponent->Velocity;
	GroundSpeed = Velocity.Size2D();

	// Update flags
	const bool bHasAcceleration = MovementComponent->GetCurrentAcceleration().SizeSquared() > 0.0f;
	bShouldMove = (GroundSpeed > 3.0f) && bHasAcceleration;
	bIsFalling = MovementComponent->IsFalling();
	bIsMantling = Character->IsMantling();

	// Update Hand & Elbow IK data
	LeftHandIK_Location = Character->LeftHandIK_Location;
	RightHandIK_Location = Character->RightHandIK_Location;
	LeftElbowIK_Location = Character->LeftElbowIK_Location;
	RightElbowIK_Location = Character->RightElbowIK_Location;
	HandIK_Weight = Character->HandIK_Weight;
}
