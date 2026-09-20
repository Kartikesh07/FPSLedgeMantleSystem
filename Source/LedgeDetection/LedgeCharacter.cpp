// Copyright Epic Games, Inc. All Rights Reserved.

#include "LedgeCharacter.h"
#include "LedgeDetectionComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Animation/AnimMontage.h"
#include "Curves/CurveVector.h"
#include "Kismet/KismetMathLibrary.h"

#include "UObject/ConstructorHelpers.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

ALedgeCharacter::ALedgeCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->SocketOffset = FVector(0.0f, 0.0f, 50.0f);

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Create Ledge Detection Component
	LedgeDetectionComponent = CreateDefaultSubobject<ULedgeDetectionComponent>(TEXT("LedgeDetectionComponent"));

	// Setup Mesh orientation (facing forward along X)
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -96.f));
	GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));

	// Load default Enhanced Input assets
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMCFinder(TEXT("/Game/Input/IMC_Default.IMC_Default"));
	if (IMCFinder.Succeeded())
	{
		DefaultMappingContext = IMCFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> MoveActionFinder(TEXT("/Game/Input/IA_Move.IA_Move"));
	if (MoveActionFinder.Succeeded())
	{
		MoveAction = MoveActionFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> LookActionFinder(TEXT("/Game/Input/IA_Look.IA_Look"));
	if (LookActionFinder.Succeeded())
	{
		LookAction = LookActionFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> JumpActionFinder(TEXT("/Game/Input/IA_Jump.IA_Jump"));
	if (JumpActionFinder.Succeeded())
	{
		JumpAction = JumpActionFinder.Object;
	}

	// Load default ALS Skeletal Mesh
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshFinder(TEXT("/Game/AdvancedLocomotionV4/CharacterAssets/MannequinSkeleton/Meshes/AnimMan.AnimMan"));
	if (MeshFinder.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(MeshFinder.Object);
	}
	else
	{
		static ConstructorHelpers::FObjectFinder<USkeletalMesh> AltMeshFinder(TEXT("/Game/AdvancedLocomotionV4/CharacterAssets/MannequinSkeleton/Meshes/Mannequin.Mannequin"));
		if (AltMeshFinder.Succeeded())
		{
			GetMesh()->SetSkeletalMesh(AltMeshFinder.Object);
		}
	}

	// Load default ALS Mantle montages
	static ConstructorHelpers::FObjectFinder<UAnimMontage> Mantle1mFinder(
		TEXT("/Game/AdvancedLocomotionV4/CharacterAssets/MannequinSkeleton/AnimationExamples/Actions/ALS_N_Mantle_1m_Montage_Default.ALS_N_Mantle_1m_Montage_Default"));
	if (Mantle1mFinder.Succeeded())
	{
		Mantle1mMontage = Mantle1mFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> Mantle2mFinder(
		TEXT("/Game/AdvancedLocomotionV4/CharacterAssets/MannequinSkeleton/AnimationExamples/Actions/ALS_N_Mantle_2m_Montage_Default.ALS_N_Mantle_2m_Montage_Default"));
	if (Mantle2mFinder.Succeeded())
	{
		Mantle2mMontage = Mantle2mFinder.Object;
	}
}

void ALedgeCharacter::Jump()
{
	if (bIsMantling)
	{
		return;
	}

	// Check for a mantleable ledge ahead
	if (LedgeDetectionComponent)
	{
		FMantleLedgeInfo LedgeInfo;
		if (LedgeDetectionComponent->DetectLedge(LedgeInfo))
		{
			StartMantle(LedgeInfo);
			return;
		}
	}

	// Default jump if no ledge was detected
	Super::Jump();
}

void ALedgeCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsMantling)
	{
		UpdateMantle(DeltaTime);
	}
}

void ALedgeCharacter::StartMantle(const FMantleLedgeInfo& LedgeInfo)
{
	if (bIsMantling || !LedgeInfo.bSuccess)
	{
		return;
	}

	bIsMantling = true;
	CurrentMantleLedgeInfo = LedgeInfo;
	MantleElapsedTime = 0.0f;

	// 1. Set the Mantle Target
	MantleTarget = LedgeInfo.TargetTransform;

	// 2. Calculate the Actual Start Offset (offset amount between the actor and target transform)
	MantleActualStartOffset = TransformSub(GetActorTransform(), MantleTarget);

	// 3. Configure ALS Inplace Mantle Parameters
	UAnimMontage* MontageToPlay = nullptr;
	FVector StartingOffset = FVector::ZeroVector;

	if (LedgeInfo.MantleType == EMantleType::HighMantle)
	{
		MontageToPlay = Mantle2mMontage;
		StartingOffset = HighMantleStartingOffset;
		MantlePlayRate = HighMantlePlayRate;
		MantleDuration = HighMantleDuration;
	}
	else
	{
		MontageToPlay = Mantle1mMontage;
		StartingOffset = LowMantleStartingOffset;
		MantlePlayRate = LowMantlePlayRate;
		MantleDuration = LowMantleDuration;
	}

	// Dynamically match mantle duration with montage length and play rate
	if (MontageToPlay && MantlePlayRate > 0.01f)
	{
		MantleDuration = MontageToPlay->GetPlayLength() / MantlePlayRate;
	}

	// 4. Calculate the Animated Start Offset from the Target Location
	// RotatedVector: StartingOffset.Y along target forward vector, StartingOffset.Z vertically
	FVector RotatedVector = MantleTarget.GetRotation().Vector() * StartingOffset.Y;
	RotatedVector.Z = StartingOffset.Z;
	const FTransform StartOffset(MantleTarget.Rotator(), MantleTarget.GetLocation() - RotatedVector, FVector::OneVector);
	MantleAnimatedStartOffset = TransformSub(StartOffset, MantleTarget);

	// 5. Disable standard movement & collisions to allow pure cinematic mantle motion
	GetCharacterMovement()->SetMovementMode(MOVE_None);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);

	// 6. Play Anim Montage with authentic ALS PlayRate
	if (MontageToPlay)
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			AnimInstance->Montage_Play(MontageToPlay, MantlePlayRate, EMontagePlayReturnType::MontageLength, 0.0f);
		}
		else
		{
			PlayAnimMontage(MontageToPlay, MantlePlayRate);
		}

		UE_LOG(LogTemp, Warning, TEXT("Playing ALS Mantle: %s (Duration: %.2f s, PlayRate: %.2f)"),
			*MontageToPlay->GetName(), MantleDuration, MantlePlayRate);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("StartMantle Error: MontageToPlay is NULL! Please check BP_LedgeCharacter Details."));
	}

	// 7. Hand IK remains disabled (0.0f) so authentic mocap plays without distortion
	HandIK_Weight = 0.0f;

	UE_LOG(LogTemp, Log, TEXT("Started ALS Mantle! Height: %.1f cm | ActualOffset: %s | AnimOffset: %s"),
		LedgeInfo.MantleHeight, *MantleActualStartOffset.GetLocation().ToString(), *MantleAnimatedStartOffset.GetLocation().ToString());
}

void ALedgeCharacter::UpdateMantle(float DeltaTime)
{
	MantleElapsedTime += DeltaTime;
	const float NormalizedTime = FMath::Clamp(MantleElapsedTime / FMath::Max(MantleDuration, 0.01f), 0.0f, 1.0f);

	// 1. Update Position and Correction Alphas using ALS Curve logic
	float PositionAlpha = 0.0f;
	float XYCorrectionAlpha = 0.0f;
	float ZCorrectionAlpha = 0.0f;

	if (MantlePositionCurve)
	{
		const FVector CurveVal = MantlePositionCurve->GetVectorValue(MantleElapsedTime);
		PositionAlpha = FMath::Clamp(CurveVal.X, 0.0f, 1.0f);
		XYCorrectionAlpha = FMath::Clamp(CurveVal.Y, 0.0f, 1.0f);
		ZCorrectionAlpha = FMath::Clamp(CurveVal.Z, 0.0f, 1.0f);
	}
	else
	{
		// Authentic ALS procedural curves:
		// Horizontal & Vertical Correction: smoothly blend actual jump offset into the animated start position
		const float CorrAlpha = FMath::Clamp(MantleElapsedTime / FMath::Max(MantleBlendInDuration, 0.01f), 0.0f, 1.0f);
		XYCorrectionAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, CorrAlpha, 2.0f);
		ZCorrectionAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, CorrAlpha, 2.0f);

		// PositionAlpha: Drives transition from hang position up onto the ledge
		// Phase 1 (0.0 to 0.25): Hang / gather phase (character holds ledge, gathers momentum)
		// Phase 2 (0.25 to 0.80): Pull-up, chest clearance, and leg drive onto platform
		// Phase 3 (0.80 to 1.0): Settle and rise to standing
		if (NormalizedTime < 0.25f)
		{
			PositionAlpha = 0.02f * FMath::Pow(NormalizedTime / 0.25f, 2.0f);
		}
		else if (NormalizedTime < 0.80f)
		{
			const float SubT = (NormalizedTime - 0.25f) / 0.55f;
			PositionAlpha = 0.02f + 0.83f * FMath::InterpEaseInOut(0.0f, 1.0f, SubT, 2.0f);
		}
		else
		{
			const float SubT = (NormalizedTime - 0.80f) / 0.20f;
			PositionAlpha = 0.85f + 0.15f * (1.0f - FMath::Pow(1.0f - SubT, 2.0f));
		}
	}

	// 2. Initial Blend-In (eliminates any visual pop from the player's launch position)
	const float BlendInAlpha = FMath::Clamp(MantleElapsedTime / FMath::Max(MantleBlendInDuration, 0.01f), 0.0f, 1.0f);
	const float BlendIn = FMath::InterpEaseInOut(0.0f, 1.0f, BlendInAlpha, 2.0f);

	// 3. ALS Step 3: Lerp multiple transforms together for independent control over horizontal, vertical, and rotation
	const FTransform TargetHzTransform(
		MantleAnimatedStartOffset.GetRotation(),
		FVector(MantleAnimatedStartOffset.GetLocation().X, MantleAnimatedStartOffset.GetLocation().Y, MantleActualStartOffset.GetLocation().Z),
		FVector::OneVector);

	const FTransform HzLerpResult = UKismetMathLibrary::TLerp(MantleActualStartOffset, TargetHzTransform, XYCorrectionAlpha);

	const FTransform TargetVtTransform(
		MantleActualStartOffset.GetRotation(),
		FVector(MantleActualStartOffset.GetLocation().X, MantleActualStartOffset.GetLocation().Y, MantleAnimatedStartOffset.GetLocation().Z),
		FVector::OneVector);

	const FTransform VtLerpResult = UKismetMathLibrary::TLerp(MantleActualStartOffset, TargetVtTransform, ZCorrectionAlpha);

	const FTransform ResultTransform(
		HzLerpResult.GetRotation(),
		FVector(HzLerpResult.GetLocation().X, HzLerpResult.GetLocation().Y, VtLerpResult.GetLocation().Z),
		FVector::OneVector);

	// Blend from blended offset into the final mantle target using PositionAlpha
	const FTransform ResultLerp = UKismetMathLibrary::TLerp(
		TransformAdd(MantleTarget, ResultTransform),
		MantleTarget,
		PositionAlpha);

	// Initial Blend In to eliminate any visual pop from the player's launch position
	const FTransform LerpedTarget = UKismetMathLibrary::TLerp(
		TransformAdd(MantleTarget, MantleActualStartOffset),
		ResultLerp,
		BlendIn);

	// 4. Set actor location and rotation
	SetActorLocationAndRotation(LerpedTarget.GetLocation(), LerpedTarget.GetRotation().Rotator(), false, nullptr, ETeleportType::TeleportPhysics);

	// Hand IK remains disabled (0.0f)
	HandIK_Weight = 0.0f;

	if (NormalizedTime >= 1.0f)
	{
		EndMantle();
	}
}

void ALedgeCharacter::EndMantle()
{
	bIsMantling = false;
	HandIK_Weight = 0.0f;
	LeftHandIK_Location = FVector::ZeroVector;
	RightHandIK_Location = FVector::ZeroVector;

	// Snap perfectly to MantleTarget
	SetActorTransform(MantleTarget, false, nullptr, ETeleportType::TeleportPhysics);

	// Restore capsule collision
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);

	// Restore walking movement mode
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	UE_LOG(LogTemp, Log, TEXT("ALS Mantle Completed successfully."));
}

void ALedgeCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}

void ALedgeCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		if (JumpAction)
		{
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		}

		// Moving
		if (MoveAction)
		{
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ALedgeCharacter::Move);
		}

		// Looking
		if (LookAction)
		{
			EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ALedgeCharacter::Look);
		}
	}
}

void ALedgeCharacter::Move(const FInputActionValue& Value)
{
	if (bIsMantling)
	{
		return;
	}

	// Input is a Vector2D (X = Right/Left, Y = Forward/Backward)
	const FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// Find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);

		// Get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// Get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// Add movement input
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void ALedgeCharacter::Look(const FInputActionValue& Value)
{
	// Input is a Vector2D (X = Yaw, Y = Pitch)
	const FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// Add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}
