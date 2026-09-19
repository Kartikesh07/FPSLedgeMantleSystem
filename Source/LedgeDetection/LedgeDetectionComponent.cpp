// Copyright Epic Games, Inc. All Rights Reserved.

#include "LedgeDetectionComponent.h"
#include "LedgeCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "MotionWarpingComponent.h"
#include "GameFramework/SpringArmComponent.h"
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
	if (CharacterOwner)
	{
		MotionWarpingComp = CharacterOwner->FindComponentByClass<UMotionWarpingComponent>();
		if (USkeletalMeshComponent* Mesh = CharacterOwner->GetMesh())
		{
			DefaultMeshRelativeLocation = Mesh->GetRelativeLocation();
			DefaultMeshRelativeRotation = Mesh->GetRelativeRotation();
		}
	}
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

	// 5. Calculate precise ledge edge and capsule landing position
	const FVector WallPlaneNormal = WallHit.ImpactNormal.GetSafeNormal2D();
	// Project TopHit.ImpactPoint onto the wall plane to find the exact corner edge of the obstacle
	const float DistFromWallPlane = FVector::DotProduct(TopHit.ImpactPoint - WallHit.ImpactPoint, WallPlaneNormal);
	const FVector TrueLedgeEdge = TopHit.ImpactPoint - (WallPlaneNormal * DistFromWallPlane);

	// Capsule landing location: safe distance inward on the ledge, bottom exactly flush with roof
	FVector TargetLandingLocation = TrueLedgeEdge + (-WallPlaneNormal * (CapsuleRadius + 15.0f));
	TargetLandingLocation.Z = TopHit.ImpactPoint.Z + CapsuleHalfHeight;

	if (!CheckCapsuleClearance(TargetLandingLocation))
	{
		return false;
	}

	// Fill out detection result
	OutResult.bLedgeFound = true;
	OutResult.WallLocation = WallHit.ImpactPoint;
	OutResult.WallNormal = WallHit.ImpactNormal;
	OutResult.LedgeLocation = TrueLedgeEdge;
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
	TransitionStartRotation = CharacterOwner->GetActorRotation();

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

		const float CapsuleRadius = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();

		// Optimal distance from wall face: Capsule radius + 4cm buffer (natural hanging posture, never penetrates wall)
		const float OptimalWallDistance = CapsuleRadius + 4.0f;
		const FVector WallPlaneNormal = Result.WallNormal.GetSafeNormal2D();
		const FVector WallSweetSpot = Result.LedgeLocation + (WallPlaneNormal * OptimalWallDistance);

		TransitionControlLocation = FVector(
			WallSweetSpot.X,
			WallSweetSpot.Y,
			Result.LedgeLocation.Z + CapsuleHalfHeight
		);

		// Align yaw perpendicular into the wall
		TransitionTargetRotation = FRotationMatrix::MakeFromX(-WallPlaneNormal).Rotator();
	}

	TransitionTargetRotation.Pitch = 0.0f;
	TransitionTargetRotation.Roll = 0.0f;

	// Temporarily switch to flying and ignore WorldStatic collision to avoid snagging on corners
	CharacterOwner->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	CharacterOwner->GetCharacterMovement()->Velocity = FVector::ZeroVector;
	CharacterOwner->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);

	// Ensure mesh starts and stays at default relative transform
	if (USkeletalMeshComponent* Mesh = CharacterOwner->GetMesh())
	{
		Mesh->SetRelativeLocationAndRotation(DefaultMeshRelativeLocation, DefaultMeshRelativeRotation, false, nullptr, ETeleportType::TeleportPhysics);
	}

	// Reset any spring arm offset so camera tracks smoothly via camera lag
	if (ALedgeCharacter* LedgeChar = Cast<ALedgeCharacter>(CharacterOwner))
	{
		if (USpringArmComponent* Boom = LedgeChar->GetCameraBoom())
		{
			Boom->TargetOffset = FVector::ZeroVector;
		}
	}

	// Lazy lookup for Motion Warping Component if not already cached
	if (!MotionWarpingComp && CharacterOwner)
	{
		MotionWarpingComp = CharacterOwner->FindComponentByClass<UMotionWarpingComponent>();
		if (!MotionWarpingComp)
		{
			MotionWarpingComp = NewObject<UMotionWarpingComponent>(CharacterOwner, TEXT("DynamicMotionWarpingComp"));
			if (MotionWarpingComp)
			{
				MotionWarpingComp->RegisterComponent();
			}
		}
	}

	bIsMotionWarpingActive = (bUseMotionWarping && MotionWarpingComp != nullptr);

	if (bIsMotionWarpingActive)
	{
		// 1. LedgeGrab: The ledge edge where hands plant, facing perpendicular into the wall
		MotionWarpingComp->AddOrUpdateWarpTargetFromLocationAndRotation(
			WarpLedgeGrabName,
			Result.LedgeLocation,
			TransitionTargetRotation
		);

		// 2. LedgeLanding: Final landing spot on top of the ledge (or other side of vault)
		MotionWarpingComp->AddOrUpdateWarpTargetFromLocationAndRotation(
			WarpLedgeLandingName,
			TransitionTargetLocation,
			TransitionTargetRotation
		);

		if (GEngine && bDrawDebug)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
				FString::Printf(TEXT("[Motion Warping] Targets Added: '%s' & '%s'"),
					*WarpLedgeGrabName.ToString(), *WarpLedgeLandingName.ToString()));
		}
	}
	else
	{
		if (GEngine && bDrawDebug)
		{
			GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Orange,
				FString::Printf(TEXT("[Motion Warping] Inactive! bUseMW=%d, Comp=%s"),
					bUseMotionWarping, MotionWarpingComp ? TEXT("Valid") : TEXT("NULL")));
		}
	}

	// Determine corresponding montage
	ActiveMontage = nullptr;
	switch (Result.ActionType)
	{
	case ELedgeActionType::Vault:
		ActiveMontage = VaultMontage;
		CurrentTransitionDuration = VaultDuration;
		break;
	case ELedgeActionType::LowMantle:
		ActiveMontage = LowMantleMontage;
		CurrentTransitionDuration = LowMantleDuration;
		break;
	case ELedgeActionType::HighMantle:
		ActiveMontage = HighMantleMontage;
		CurrentTransitionDuration = HighMantleDuration;
		break;
	default:
		CurrentTransitionDuration = 1.0f;
		break;
	}

	// Play montage and register completion delegates
	if (ActiveMontage && CharacterOwner)
	{
		const float EffectivePlayRate = FMath::Max(MontagePlayRate, 0.1f);
		const float MontageLength = CharacterOwner->PlayAnimMontage(ActiveMontage, EffectivePlayRate);
		if (MontageLength > 0.0f)
		{
			if (bSyncDurationToMontage)
			{
				CurrentTransitionDuration = MontageLength / EffectivePlayRate;
			}

			// Bind montage ended delegate to finish transition only when the animation completes
			if (USkeletalMeshComponent* MeshComp = CharacterOwner->GetMesh())
			{
				if (UAnimInstance* AnimInst = MeshComp->GetAnimInstance())
				{
					FOnMontageEnded EndDelegate;
					EndDelegate.BindUObject(this, &ULedgeDetectionComponent::OnMontageEnded);
					AnimInst->Montage_SetEndDelegate(EndDelegate, ActiveMontage);
				}
			}

			if (GEngine && bDrawDebug)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
					FString::Printf(TEXT("[Ledge Montage] %s | Duration: %.2fs | MW: %s"),
						*ActiveMontage->GetName(), CurrentTransitionDuration,
						bIsMotionWarpingActive ? TEXT("ACTIVE") : TEXT("OFF")));
			}
		}
		else
		{
			if (GEngine && bDrawDebug)
			{
				GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Yellow,
					FString::Printf(TEXT("[Ledge Montage] PlayAnimMontage returned 0 for %s! Check Slot name / AnimInstance!"), *ActiveMontage->GetName()));
			}
		}
	}
	else if (!ActiveMontage)
	{
		if (GEngine && bDrawDebug)
		{
			GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Red,
				TEXT("[Ledge Montage] No Montage assigned! Check BP_LedgeCharacter -> LedgeDetector properties."));
		}
	}

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

	if (bIsMotionWarpingActive)
	{
		// True Root Motion + Motion Warping: The animation's root motion physically drives the capsule!
		// We do NOT override actor location with SetActorLocation so root motion has 100% control.
		ApplyCameraOffset(NormalizedTime);

		// Safety watchdog: In case animation fails to notify completion
		if (TransitionTimeElapsed >= (CurrentTransitionDuration + 0.35f))
		{
			FinishTransition();
		}
		return;
	}

	FVector NewLocation = TransitionStartLocation;

	if (CurrentAction == ELedgeActionType::Vault)
	{
		// Smooth parabolic arc over the crest for vaults
		const float Alpha = FMath::SmoothStep(0.0f, 1.0f, NormalizedTime);
		const float OneMinusAlpha = 1.0f - Alpha;
		NewLocation = (OneMinusAlpha * OneMinusAlpha * TransitionStartLocation)
			+ (2.0f * OneMinusAlpha * Alpha * TransitionControlLocation)
			+ (Alpha * Alpha * TransitionTargetLocation);
	}
	else // Low or High Mantle (Biomechanical human climb matching ALS keyframes)
	{
		// 1. Horizontal XY Position:
		// t = 0.00 -> 0.15: Slide into wall sweet spot (hands reach & grab ledge edge)
		// t = 0.15 -> 0.55: Hold steady close to wall as arms pull body straight up
		// t = 0.55 -> 0.92: Push forward onto top landing surface as chest/pelvis clears crest
		// t = 0.92 -> 1.00: Settle on top
		FVector CurrentXY;
		if (NormalizedTime < 0.15f)
		{
			const float tXY = FMath::SmoothStep(0.0f, 1.0f, NormalizedTime / 0.15f);
			CurrentXY = FMath::Lerp(
				FVector(TransitionStartLocation.X, TransitionStartLocation.Y, 0.0f),
				FVector(TransitionControlLocation.X, TransitionControlLocation.Y, 0.0f),
				tXY
			);
		}
		else if (NormalizedTime < 0.55f)
		{
			CurrentXY = FVector(TransitionControlLocation.X, TransitionControlLocation.Y, 0.0f);
		}
		else if (NormalizedTime < 0.92f)
		{
			const float tXY = FMath::SmoothStep(0.0f, 1.0f, (NormalizedTime - 0.55f) / 0.37f);
			CurrentXY = FMath::Lerp(
				FVector(TransitionControlLocation.X, TransitionControlLocation.Y, 0.0f),
				FVector(TransitionTargetLocation.X, TransitionTargetLocation.Y, 0.0f),
				tXY
			);
		}
		else
		{
			CurrentXY = FVector(TransitionTargetLocation.X, TransitionTargetLocation.Y, 0.0f);
		}

		// 2. Vertical Z Position:
		// t = 0.00 -> 0.15: Hold at start height (reach / plant hands on ledge)
		// t = 0.15 -> 0.70: Powerful muscular pull-up in lockstep with arm pull
		// t = 0.70 -> 1.00: Hold at top landing height (feet touching floor surface)
		float CurrentZ = TransitionStartLocation.Z;
		if (NormalizedTime < 0.15f)
		{
			CurrentZ = TransitionStartLocation.Z;
		}
		else if (NormalizedTime < 0.70f)
		{
			const float tZ = (NormalizedTime - 0.15f) / 0.55f;
			const float ZAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, tZ, 2.0f);
			CurrentZ = FMath::Lerp(TransitionStartLocation.Z, TransitionTargetLocation.Z, ZAlpha);
		}
		else
		{
			CurrentZ = TransitionTargetLocation.Z;
		}

		NewLocation = FVector(CurrentXY.X, CurrentXY.Y, CurrentZ);
	}

	CharacterOwner->SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);

	// Yaw alignment: smooth rotation facing perpendicular into the wall during the grab phase (0.0 -> 0.15)
	const float RotAlpha = FMath::Clamp(NormalizedTime / 0.15f, 0.0f, 1.0f);
	const float SmoothRotAlpha = FMath::SmoothStep(0.0f, 1.0f, RotAlpha);
	const FRotator TargetRot = FMath::Lerp(TransitionStartRotation, TransitionTargetRotation, SmoothRotAlpha);
	CharacterOwner->SetActorRotation(FRotator(0.0f, TargetRot.Yaw, 0.0f));

	ApplyCameraOffset(NormalizedTime);

	if (NormalizedTime >= 1.0f)
	{
		FinishTransition();
	}
}

void ULedgeDetectionComponent::FinishTransition()
{
	if (!bIsTransitioning)
	{
		return;
	}

	bIsTransitioning = false;
	SetComponentTickEnabled(false);

	if (CharacterOwner)
	{
		// Ensure mesh relative transform remains at default
		if (USkeletalMeshComponent* Mesh = CharacterOwner->GetMesh())
		{
			Mesh->SetRelativeLocationAndRotation(DefaultMeshRelativeLocation, DefaultMeshRelativeRotation, false, nullptr, ETeleportType::TeleportPhysics);

			if (UAnimInstance* AnimInst = Mesh->GetAnimInstance())
			{
				if (ActiveMontage)
				{
					FOnMontageEnded DummyEnded;
					AnimInst->Montage_SetEndDelegate(DummyEnded, ActiveMontage);
				}
			}
		}

		if (ActiveMontage)
		{
			CharacterOwner->StopAnimMontage(ActiveMontage);
			ActiveMontage = nullptr;
		}

		// Snap capsule to exact destination
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

	// Clean up Motion Warping targets
	if (MotionWarpingComp)
	{
		MotionWarpingComp->RemoveWarpTarget(WarpLedgeGrabName);
		MotionWarpingComp->RemoveWarpTarget(WarpLedgeLandingName);
	}
	bIsMotionWarpingActive = false;

	ResetCameraOffset();
	CurrentAction = ELedgeActionType::None;
}

void ULedgeDetectionComponent::CancelTransition()
{
	if (bIsTransitioning)
	{
		if (CharacterOwner)
		{
			if (USkeletalMeshComponent* Mesh = CharacterOwner->GetMesh())
			{
				Mesh->SetRelativeLocationAndRotation(DefaultMeshRelativeLocation, DefaultMeshRelativeRotation, false, nullptr, ETeleportType::TeleportPhysics);
			}

			if (ActiveMontage)
			{
				CharacterOwner->StopAnimMontage(ActiveMontage);
				ActiveMontage = nullptr;
			}
		}
		FinishTransition();
	}
}

void ULedgeDetectionComponent::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	// Do not cut transition short during blend-out; let the cross-fade complete smoothly
}

void ULedgeDetectionComponent::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bIsTransitioning && Montage == ActiveMontage)
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
