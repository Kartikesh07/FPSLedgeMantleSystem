// Copyright Epic Games, Inc. All Rights Reserved.

#include "LedgeAnimInstance.h"
#include "LedgeCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

ULedgeAnimInstance::ULedgeAnimInstance()
	: GroundSpeed(0.0f)
	, bShouldMove(false)
	, bIsFalling(false)
	, bIsLedgeTransitioning(false)
{
}

void ULedgeAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	LedgeCharacter = Cast<ALedgeCharacter>(TryGetPawnOwner());
	if (LedgeCharacter)
	{
		MovementComponent = LedgeCharacter->GetCharacterMovement();
	}
}

void ULedgeAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!LedgeCharacter)
	{
		LedgeCharacter = Cast<ALedgeCharacter>(TryGetPawnOwner());
		if (LedgeCharacter)
		{
			MovementComponent = LedgeCharacter->GetCharacterMovement();
		}
	}

	if (!LedgeCharacter || !MovementComponent)
	{
		return;
	}

	const FVector Velocity = MovementComponent->Velocity;
	GroundSpeed = Velocity.Size2D();

	bIsFalling = MovementComponent->IsFalling();
	bIsLedgeTransitioning = LedgeCharacter->IsLedgeTransitioning();

	const FVector Acceleration = MovementComponent->GetCurrentAcceleration();
	bShouldMove = (GroundSpeed > 3.0f) && (!Acceleration.IsNearlyZero());
}
