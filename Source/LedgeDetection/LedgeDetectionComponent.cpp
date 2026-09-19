// Copyright Epic Games, Inc. All Rights Reserved.

#include "LedgeDetectionComponent.h"
#include "LedgeCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

ULedgeDetectionComponent::ULedgeDetectionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void ULedgeDetectionComponent::BeginPlay()
{
	Super::BeginPlay();
	CharacterOwner = Cast<ACharacter>(GetOwner());
	SetComponentTickEnabled(false);
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
	const float WalkableZ = CharacterOwner->GetCharacterMovement() ? CharacterOwner->GetCharacterMovement()->GetWalkableFloorZ() : 0.7f;
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
	const FVector FeetLocation = CharacterOwner->GetActorLocation() - FVector(0.f, 0.f, CapsuleHalfHeight);
	
	// Use controller yaw rotation so player looks towards the intended ledge
	FRotator ControlRot = CharacterOwner->GetControlRotation();
	ControlRot.Pitch = 0.0f;
	ControlRot.Roll = 0.0f;
	OutForwardDir = ControlRot.Vector();

	FCollisionQueryParams QueryParams(TEXT("LedgeForwardTrace"), false, CharacterOwner);

	// 1. High Trace (Chest height ~ 125 cm above feet): detects tall and medium walls
	const FVector HighStart = FeetLocation + FVector(0.f, 0.f, 125.0f);
	const FVector HighEnd = HighStart + (OutForwardDir * ForwardTraceDistance);
	FHitResult HighHit;
	const bool bHighHit = GetWorld()->LineTraceSingleByChannel(HighHit, HighStart, HighEnd, TraceChannel, QueryParams);

	// 2. Low Trace (Hip/Waist height ~ 55 cm above feet): detects low vaultable obstacles (40 - 100 cm)
	const FVector LowStart = FeetLocation + FVector(0.f, 0.f, 55.0f);
	const FVector LowEnd = LowStart + (OutForwardDir * ForwardTraceDistance);
	FHitResult LowHit;
	const bool bLowHit = GetWorld()->LineTraceSingleByChannel(LowHit, LowStart, LowEnd, TraceChannel, QueryParams);

	if (bDrawDebug)
	{
		DrawDebugLine(GetWorld(), HighStart, bHighHit ? HighHit.ImpactPoint : HighEnd, bHighHit ? FColor::Green : FColor::Red, false, DebugDrawDuration, 0, 1.5f);
		DrawDebugLine(GetWorld(), LowStart, bLowHit ? LowHit.ImpactPoint : LowEnd, bLowHit ? FColor::Green : FColor::Red, false, DebugDrawDuration, 0, 1.5f);
	}

	// Prefer High hit for alignment if both hit; otherwise use Low hit for low obstacles
	if (bHighHit && HighHit.bBlockingHit)
	{
		OutHit = HighHit;
		return true;
	}
	else if (bLowHit && LowHit.bBlockingHit)
	{
		OutHit = LowHit;
		return true;
	}

	return false;
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

void ULedgeDetectionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsTransitioning)
	{
		UpdateTransition(DeltaTime);
	}
}

bool ULedgeDetectionComponent::StartTransition(const FLedgeDetectionResult& Result)
{
	if (!Result.bLedgeFound || !CharacterOwner || bIsTransitioning)
	{
		return false;
	}

	bIsTransitioning = true;
	CurrentAction = Result.ActionType;
	TransitionTimeElapsed = 0.0f;

	const float CapsuleHalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	TransitionStartLocation = CharacterOwner->GetActorLocation();

	// Temporarily switch to flying and ignore WorldStatic collision to avoid snagging on corners
	CharacterOwner->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	CharacterOwner->GetCharacterMovement()->Velocity = FVector::ZeroVector;
	CharacterOwner->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);

	if (Result.ActionType == ELedgeActionType::Vault)
	{
		CurrentTransitionDuration = VaultDuration;
		TransitionTargetLocation = Result.VaultLandingLocation;

		// Apex is directly over the obstacle crest
		TransitionControlLocation = FVector(
			Result.LedgeLocation.X,
			Result.LedgeLocation.Y,
			Result.LedgeLocation.Z + CapsuleHalfHeight + 15.0f
		);

		// Direction of travel for vault
		VaultForwardDirection = (Result.VaultLandingLocation - TransitionStartLocation).GetSafeNormal2D();
		if (VaultForwardDirection.IsNearlyZero())
		{
			VaultForwardDirection = CharacterOwner->GetActorForwardVector();
		}

		// Keep or boost forward speed on landing
		VaultExitSpeed = FMath::Max(CharacterOwner->GetCharacterMovement()->MaxWalkSpeed, 600.0f);
		TransitionTargetRotation = FRotationMatrix::MakeFromX(VaultForwardDirection).Rotator();
	}
	else // Low or High Mantle
	{
		CurrentTransitionDuration = (Result.ActionType == ELedgeActionType::HighMantle) ? HighMantleDuration : LowMantleDuration;
		TransitionTargetLocation = Result.TargetLandingLocation;

		// Lift vertically first to clear edge, then step onto surface
		TransitionControlLocation = FVector(
			TransitionStartLocation.X,
			TransitionStartLocation.Y,
			Result.LedgeLocation.Z + CapsuleHalfHeight + 6.0f
		);

		// Align yaw perpendicular to wall
		TransitionTargetRotation = FRotationMatrix::MakeFromX(-Result.WallNormal).Rotator();
	}

	TransitionTargetRotation.Pitch = 0.0f;
	TransitionTargetRotation.Roll = 0.0f;

	SetComponentTickEnabled(true);
	return true;
}

void ULedgeDetectionComponent::UpdateTransition(float DeltaTime)
{
	if (!CharacterOwner)
	{
		FinishTransition();
		return;
	}

	TransitionTimeElapsed += DeltaTime;
	const float NormalizedTime = FMath::Clamp(TransitionTimeElapsed / CurrentTransitionDuration, 0.0f, 1.0f);

	// Smooth Ease-in / Ease-out curve
	const float Alpha = FMath::SmoothStep(0.0f, 1.0f, NormalizedTime);

	// Quadratic Bezier Interpolation
	const float OneMinusAlpha = 1.0f - Alpha;
	const FVector NewLocation = (OneMinusAlpha * OneMinusAlpha * TransitionStartLocation)
		+ (2.0f * OneMinusAlpha * Alpha * TransitionControlLocation)
		+ (Alpha * Alpha * TransitionTargetLocation);

	CharacterOwner->SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);

	// Smoothly align character yaw
	const FRotator CurrentRot = CharacterOwner->GetActorRotation();
	const FRotator NewRot = FMath::RInterpTo(CurrentRot, TransitionTargetRotation, DeltaTime, 14.0f);
	CharacterOwner->SetActorRotation(FRotator(0.0f, NewRot.Yaw, 0.0f));

	// Procedural first-person camera weight dip
	ApplyCameraOffset(NormalizedTime);

	if (NormalizedTime >= 1.0f)
	{
		FinishTransition();
	}
}

void ULedgeDetectionComponent::FinishTransition()
{
	bIsTransitioning = false;
	SetComponentTickEnabled(false);

	if (CharacterOwner)
	{
		// Snap to exact destination
		CharacterOwner->SetActorLocation(TransitionTargetLocation, false, nullptr, ETeleportType::TeleportPhysics);
		CharacterOwner->SetActorRotation(FRotator(0.0f, TransitionTargetRotation.Yaw, 0.0f));

		// Restore collision response to static geometry
		CharacterOwner->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);

		// Restore standard walking physics
		CharacterOwner->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		// If vaulting, preserve and launch with forward running momentum!
		if (CurrentAction == ELedgeActionType::Vault)
		{
			CharacterOwner->GetCharacterMovement()->Velocity = VaultForwardDirection * VaultExitSpeed;
		}
		else
		{
			CharacterOwner->GetCharacterMovement()->Velocity = FVector::ZeroVector;
		}
	}

	ResetCameraOffset();
	CurrentAction = ELedgeActionType::None;
}

void ULedgeDetectionComponent::CancelTransition()
{
	if (bIsTransitioning)
	{
		FinishTransition();
	}
}

void ULedgeDetectionComponent::ApplyCameraOffset(float Alpha)
{
	// Third-person camera is handled via SpringArm/CharacterMovement and animations
}

void ULedgeDetectionComponent::ResetCameraOffset()
{
}
