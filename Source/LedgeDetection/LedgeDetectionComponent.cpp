// Copyright Epic Games, Inc. All Rights Reserved.

#include "LedgeDetectionComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "CollisionQueryParams.h"

ULedgeDetectionComponent::ULedgeDetectionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void ULedgeDetectionComponent::BeginPlay()
{
	Super::BeginPlay();
	CharacterOwner = Cast<ACharacter>(GetOwner());
}

bool ULedgeDetectionComponent::DetectLedge(FMantleLedgeInfo& OutLedgeInfo)
{
	OutLedgeInfo = FMantleLedgeInfo();

	if (!CharacterOwner)
	{
		CharacterOwner = Cast<ACharacter>(GetOwner());
		if (!CharacterOwner)
		{
			return false;
		}
	}

	UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement();
	if (!Capsule || !MoveComp)
	{
		return false;
	}

	// 1. Determine trace direction (favoring player input / acceleration if active)
	FVector TraceDir = MoveComp->GetCurrentAcceleration().GetSafeNormal2D();
	if (TraceDir.IsNearlyZero())
	{
		TraceDir = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();
	}

	// 2. Forward Wall Trace
	FHitResult WallHit;
	if (!TraceForwardForWall(TraceDir, WallHit))
	{
		return false;
	}

	// 3. Downward Ledge Trace
	FHitResult LedgeHit;
	if (!TraceDownForLedge(WallHit, LedgeHit))
	{
		return false;
	}

	// 4. Calculate Mantle Height from character's feet
	const float CapsuleRadius = Capsule->GetScaledCapsuleRadius();
	const float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float FeetZ = CharacterOwner->GetActorLocation().Z - CapsuleHalfHeight;
	const float MantleHeight = LedgeHit.ImpactPoint.Z - FeetZ;

	if (MantleHeight < MinMantleHeight || MantleHeight > MaxMantleHeight)
	{
		return false;
	}

	// 5. Calculate Candidate Target Transform on the ledge
	// Face perpendicular into the wall
	const FRotator TargetRotation = FRotator(0.0f, (-WallHit.ImpactNormal).Rotation().Yaw, 0.0f);
	const FVector InwardDirection = -WallHit.ImpactNormal.GetSafeNormal2D();

	// Calculate the precise front lip where the vertical wall meets the top ledge surface
	const FVector LedgeFrontLip = FVector(WallHit.ImpactPoint.X, WallHit.ImpactPoint.Y, LedgeHit.ImpactPoint.Z);

	// Push target inward past the wall edge so the capsule lands firmly on top
	const FVector TargetXY = LedgeFrontLip + (InwardDirection * (CapsuleRadius + LedgeInwardOffset));
	const float TargetZ = LedgeFrontLip.Z + CapsuleHalfHeight + 2.0f; // 2cm clearance above surface
	const FVector TargetLocation = FVector(TargetXY.X, TargetXY.Y, TargetZ);

	// 6. Clearance Check (ensure capsule fits at target location)
	if (!CheckCapsuleClearance(TargetLocation, CapsuleRadius, CapsuleHalfHeight))
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (bDrawDebug && GetWorld())
		{
			DrawDebugCapsule(GetWorld(), TargetLocation, CapsuleHalfHeight, CapsuleRadius, TargetRotation.Quaternion(), FColor::Red, false, DebugDrawDuration, 0, 2.0f);
		}
#endif
		return false;
	}

	// 7. Success - Populate Ledge Info
	OutLedgeInfo.bSuccess = true;
	OutLedgeInfo.TargetTransform = FTransform(TargetRotation, TargetLocation);
	OutLedgeInfo.WallLocation = WallHit.ImpactPoint;
	OutLedgeInfo.WallNormal = WallHit.ImpactNormal;
	OutLedgeInfo.LedgeLocation = LedgeHit.ImpactPoint;
	OutLedgeInfo.LedgeFrontLip = LedgeFrontLip;
	OutLedgeInfo.MantleHeight = MantleHeight;
	OutLedgeInfo.MantleType = (MantleHeight > 125.0f) ? EMantleType::HighMantle : EMantleType::LowMantle;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (bDrawDebug && GetWorld())
	{
		// Draw Wall Hit
		DrawDebugSphere(GetWorld(), WallHit.ImpactPoint, 10.0f, 12, FColor::Green, false, DebugDrawDuration, 0, 1.5f);
		DrawDebugLine(GetWorld(), WallHit.ImpactPoint, WallHit.ImpactPoint + WallHit.ImpactNormal * 30.0f, FColor::Green, false, DebugDrawDuration, 0, 2.0f);

		// Draw Ledge Top Hit
		DrawDebugSphere(GetWorld(), LedgeHit.ImpactPoint, 10.0f, 12, FColor::Yellow, false, DebugDrawDuration, 0, 1.5f);

		// Draw Target Capsule
		DrawDebugCapsule(GetWorld(), TargetLocation, CapsuleHalfHeight, CapsuleRadius, TargetRotation.Quaternion(), FColor::Cyan, false, DebugDrawDuration, 0, 2.0f);
	}
#endif

	return true;
}

bool ULedgeDetectionComponent::TraceForwardForWall(const FVector& TraceDirection, FHitResult& OutHit)
{
	if (!CharacterOwner || !GetWorld())
	{
		return false;
	}

	UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	const float CapsuleRadius = Capsule ? Capsule->GetScaledCapsuleRadius() : 42.0f;
	const float CapsuleHalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 96.0f;

	// Trace from waist/chest level forward
	const FVector ActorLoc = CharacterOwner->GetActorLocation();
	const FVector Start = FVector(ActorLoc.X, ActorLoc.Y, ActorLoc.Z - CapsuleHalfHeight * 0.2f);
	const FVector End = Start + (TraceDirection * (CapsuleRadius + ForwardTraceDistance));

	FCollisionQueryParams QueryParams(TEXT("MantleForwardTrace"), false, CharacterOwner);

	bool bHit = GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, TraceChannel, QueryParams);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (bDrawDebug && GetWorld())
	{
		DrawDebugLine(GetWorld(), Start, bHit ? OutHit.ImpactPoint : End, bHit ? FColor::Green : FColor::Orange, false, DebugDrawDuration, 0, 1.5f);
	}
#endif

	if (!bHit || !OutHit.bBlockingHit)
	{
		return false;
	}

	// The surface must be steep/vertical enough to be a wall (not a walkable floor)
	if (OutHit.ImpactNormal.Z > 0.25f)
	{
		return false;
	}

	return true;
}

bool ULedgeDetectionComponent::TraceDownForLedge(const FHitResult& WallHit, FHitResult& OutHit)
{
	if (!CharacterOwner || !GetWorld())
	{
		return false;
	}

	UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement();
	const float CapsuleHalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 96.0f;
	const float WalkableFloorZ = MoveComp ? MoveComp->GetWalkableFloorZ() : 0.71f;

	// Start above the wall, projecting slightly inwards towards the ledge surface
	const FVector InwardDirection = -WallHit.ImpactNormal.GetSafeNormal2D();
	const FVector LedgeTraceXY = WallHit.ImpactPoint + (InwardDirection * (DownwardTraceRadius + 10.0f));

	const float ActorZ = CharacterOwner->GetActorLocation().Z;
	const FVector TraceStart = FVector(LedgeTraceXY.X, LedgeTraceXY.Y, ActorZ + MaxMantleHeight + DownwardTraceRadius);
	const FVector TraceEnd = FVector(LedgeTraceXY.X, LedgeTraceXY.Y, (ActorZ - CapsuleHalfHeight) + MinMantleHeight);

	FCollisionQueryParams QueryParams(TEXT("MantleDownTrace"), false, CharacterOwner);

	bool bHit = GetWorld()->SweepSingleByChannel(
		OutHit,
		TraceStart,
		TraceEnd,
		FQuat::Identity,
		TraceChannel,
		FCollisionShape::MakeSphere(DownwardTraceRadius),
		QueryParams
	);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (bDrawDebug && GetWorld())
	{
		DrawDebugLine(GetWorld(), TraceStart, bHit ? OutHit.ImpactPoint : TraceEnd, bHit ? FColor::Yellow : FColor::Red, false, DebugDrawDuration, 0, 1.5f);
	}
#endif

	if (!bHit || !OutHit.bBlockingHit)
	{
		return false;
	}

	// Surface must be flat enough to stand on (walkable slope)
	if (OutHit.ImpactNormal.Z < WalkableFloorZ)
	{
		return false;
	}

	return true;
}

bool ULedgeDetectionComponent::CheckCapsuleClearance(const FVector& TargetLocation, float CapsuleRadius, float CapsuleHalfHeight) const
{
	if (!CharacterOwner || !GetWorld())
	{
		return false;
	}

	FCollisionQueryParams QueryParams(TEXT("MantleClearance"), false, CharacterOwner);

	// Overlap check for capsule at target
	const bool bOverlap = GetWorld()->OverlapBlockingTestByChannel(
		TargetLocation,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeCapsule(CapsuleRadius * 0.95f, CapsuleHalfHeight * 0.95f),
		QueryParams
	);

	return !bOverlap;
}
