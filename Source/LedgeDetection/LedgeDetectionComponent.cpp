// Copyright Epic Games, Inc. All Rights Reserved.

#include "LedgeDetectionComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

ULedgeDetectionComponent::ULedgeDetectionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void ULedgeDetectionComponent::BeginPlay()
{
	Super::BeginPlay();
	CharacterOwner = Cast<ACharacter>(GetOwner());
}

bool ULedgeDetectionComponent::DetectLedge(FLedgeDetectionResult& OutResult)
{
	OutResult = FLedgeDetectionResult();

	if (!CharacterOwner || !GetWorld())
	{
		return false;
	}

	// 1. Forward Trace to detect the wall face
	FHitResult WallHit;
	FVector ForwardDir;
	if (!ForwardTrace(WallHit, ForwardDir))
	{
		return false;
	}

	// 2. Downward Trace to find the ledge top surface
	FHitResult TopHit;
	if (!DownwardTrace(WallHit.ImpactPoint, WallHit.ImpactNormal, TopHit))
	{
		return false;
	}

	// 3. Slope check - is top surface walkable?
	const float WalkableZ = CharacterOwner->GetCharacterMovement() ? CharacterOwner->GetCharacterMovement()->WalkableFloorZ : 0.7f;
	if (TopHit.ImpactNormal.Z < WalkableZ)
	{
		return false;
	}

	// 4. Calculate ledge height relative to feet
	const float CapsuleHalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float CapsuleRadius = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float FeetZ = CharacterOwner->GetActorLocation().Z - CapsuleHalfHeight;
	const float LedgeHeight = TopHit.ImpactPoint.Z - FeetZ;

	if (LedgeHeight < MinLedgeHeight || LedgeHeight > MaxLedgeHeight)
	{
		return false;
	}

	// 5. Capsule clearance check on top of the ledge (Mantle destination)
	FVector TargetLandingLocation = TopHit.ImpactPoint + (-WallHit.ImpactNormal * (CapsuleRadius + 5.0f));
	TargetLandingLocation.Z = TopHit.ImpactPoint.Z + CapsuleHalfHeight + 2.0f;

	if (!CheckCapsuleClearance(TargetLandingLocation))
	{
		return false;
	}

	// Fill out detection result
	OutResult.bLedgeFound = true;
	OutResult.WallLocation = WallHit.ImpactPoint;
	OutResult.WallNormal = WallHit.ImpactNormal;
	OutResult.LedgeLocation = TopHit.ImpactPoint;
	OutResult.LedgeNormal = TopHit.ImpactNormal;
	OutResult.LedgeHeight = LedgeHeight;
	OutResult.TargetLandingLocation = TargetLandingLocation;

	// 6. Check if eligible for a Vault (low obstacle with open clearance behind)
	if (LedgeHeight <= MaxVaultHeight)
	{
		FVector VaultLanding;
		if (CheckVaultClearance(WallHit.ImpactPoint, ForwardDir, TopHit.ImpactPoint.Z, VaultLanding))
		{
			OutResult.bIsVaultable = true;
			OutResult.VaultLandingLocation = VaultLanding;
			OutResult.ActionType = ELedgeActionType::Vault;
		}
		else
		{
			OutResult.ActionType = ELedgeActionType::LowMantle;
		}
	}
	else
	{
		OutResult.ActionType = (LedgeHeight > 130.0f) ? ELedgeActionType::HighMantle : ELedgeActionType::LowMantle;
	}

	// 7. Debug Drawing
	if (bDrawDebug)
	{
		DrawDebugVisuals(OutResult);
	}

	return true;
}

bool ULedgeDetectionComponent::ForwardTrace(FHitResult& OutHit, FVector& OutForwardDir)
{
	const float CapsuleHalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	
	// Use controller yaw rotation so player looks towards the intended ledge
	FRotator ControlRot = CharacterOwner->GetControlRotation();
	ControlRot.Pitch = 0.0f;
	ControlRot.Roll = 0.0f;
	OutForwardDir = ControlRot.Vector();

	// Trace forward from chest/eye level
	const FVector TraceStart = CharacterOwner->GetActorLocation() + FVector(0.f, 0.f, CapsuleHalfHeight * 0.3f);
	const FVector TraceEnd = TraceStart + (OutForwardDir * ForwardTraceDistance);

	FCollisionQueryParams QueryParams(TEXT("LedgeForwardTrace"), false, CharacterOwner);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(OutHit, TraceStart, TraceEnd, TraceChannel, QueryParams);

	if (bDrawDebug)
	{
		DrawDebugLine(GetWorld(), TraceStart, bHit ? OutHit.ImpactPoint : TraceEnd, bHit ? FColor::Green : FColor::Red, false, DebugDrawDuration, 0, 1.5f);
	}

	return bHit && OutHit.bBlockingHit;
}

bool ULedgeDetectionComponent::DownwardTrace(const FVector& WallImpactPoint, const FVector& WallNormal, FHitResult& OutHit)
{
	const float CapsuleHalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float FeetZ = CharacterOwner->GetActorLocation().Z - CapsuleHalfHeight;

	// Trace downward offset inward past the wall face
	const FVector InwardDir = -WallNormal;
	FVector TopTraceStart = WallImpactPoint + (InwardDir * LedgeDepthOffset);
	TopTraceStart.Z = FeetZ + MaxLedgeHeight;

	FVector TopTraceEnd = TopTraceStart;
	TopTraceEnd.Z = FeetZ + MinLedgeHeight;

	FCollisionQueryParams QueryParams(TEXT("LedgeDownwardTrace"), false, CharacterOwner);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(OutHit, TopTraceStart, TopTraceEnd, TraceChannel, QueryParams);

	if (bDrawDebug)
	{
		DrawDebugLine(GetWorld(), TopTraceStart, bHit ? OutHit.ImpactPoint : TopTraceEnd, bHit ? FColor::Orange : FColor::Purple, false, DebugDrawDuration, 0, 1.5f);
	}

	return bHit && OutHit.bBlockingHit;
}

bool ULedgeDetectionComponent::CheckCapsuleClearance(const FVector& TargetLocation)
{
	const float CapsuleHalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float CapsuleRadius = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();

	// Shrink slightly to prevent false positives with floor
	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(CapsuleRadius * 0.9f, CapsuleHalfHeight * 0.9f);
	FCollisionQueryParams QueryParams(TEXT("LedgeClearanceCheck"), false, CharacterOwner);

	const bool bBlocked = GetWorld()->OverlapBlockingTestByChannel(
		TargetLocation,
		FQuat::Identity,
		TraceChannel,
		CapsuleShape,
		QueryParams
	);

	return !bBlocked;
}

bool ULedgeDetectionComponent::CheckVaultClearance(const FVector& WallImpactPoint, const FVector& ForwardDir, float ObstacleTopZ, FVector& OutVaultLandingLocation)
{
	const float CapsuleHalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float CapsuleRadius = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float FeetZ = CharacterOwner->GetActorLocation().Z - CapsuleHalfHeight;

	// Trace past the obstacle to find landing ground on the other side
	const FVector PastObstaclePoint = WallImpactPoint + (ForwardDir * (MaxVaultThickness + CapsuleRadius * 1.5f));
	const FVector GroundTraceStart = FVector(PastObstaclePoint.X, PastObstaclePoint.Y, ObstacleTopZ + 20.0f);
	const FVector GroundTraceEnd = FVector(PastObstaclePoint.X, PastObstaclePoint.Y, FeetZ - 50.0f);

	FHitResult GroundHit;
	FCollisionQueryParams QueryParams(TEXT("VaultGroundTrace"), false, CharacterOwner);

	if (GetWorld()->LineTraceSingleByChannel(GroundHit, GroundTraceStart, GroundTraceEnd, TraceChannel, QueryParams))
	{
		OutVaultLandingLocation = GroundHit.ImpactPoint + FVector(0.f, 0.f, CapsuleHalfHeight + 2.0f);
		return CheckCapsuleClearance(OutVaultLandingLocation);
	}

	return false;
}

void ULedgeDetectionComponent::DrawDebugVisuals(const FLedgeDetectionResult& Result)
{
	UWorld* World = GetWorld();
	if (!World || !CharacterOwner)
	{
		return;
	}

	const float CapsuleHalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float CapsuleRadius = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();

	// 1. Ledge top point (Emerald Sphere)
	DrawDebugSphere(World, Result.LedgeLocation, 8.0f, 12, FColor::Emerald, false, DebugDrawDuration, 0, 1.5f);

	// 2. Wall normal arrow (Yellow)
	DrawDebugDirectionalArrow(World, Result.WallLocation, Result.WallLocation + (Result.WallNormal * 30.0f), 15.0f, FColor::Yellow, false, DebugDrawDuration, 0, 2.0f);

	// 3. Target Mantle Landing Capsule (Cyan)
	DrawDebugCapsule(World, Result.TargetLandingLocation, CapsuleHalfHeight, CapsuleRadius, FQuat::Identity, FColor::Cyan, false, DebugDrawDuration, 0, 1.5f);

	// 4. If vaultable, draw Vault Landing Capsule (Orange)
	if (Result.bIsVaultable)
	{
		DrawDebugCapsule(World, Result.VaultLandingLocation, CapsuleHalfHeight, CapsuleRadius, FQuat::Identity, FColor::Orange, false, DebugDrawDuration, 0, 1.5f);
		DrawDebugLine(World, Result.LedgeLocation, Result.VaultLandingLocation, FColor::Orange, false, DebugDrawDuration, 0, 2.0f);
	}
}
