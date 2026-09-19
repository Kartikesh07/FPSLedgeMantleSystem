// Copyright Epic Games, Inc. All Rights Reserved.

#include "LedgeCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "LedgeDetectionComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"

ALedgeCharacter::ALedgeCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate character when the controller rotates - let the controller rotate the camera and yaw only
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = false; // For first person, yaw follows controller
	GetCharacterMovement()->MaxWalkSpeed = 600.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->JumpZVelocity = 500.f;

	// Create a CameraComponent	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(0.f, 0.f, 64.f)); // Position at eye height
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	// Create Ledge Detection Component
	LedgeDetectionComponent = CreateDefaultSubobject<ULedgeDetectionComponent>(TEXT("LedgeDetector"));
}

void ALedgeCharacter::Jump()
{
	if (LedgeDetectionComponent)
	{
		FLedgeDetectionResult Result;
		if (LedgeDetectionComponent->DetectLedge(Result))
		{
			FString ActionName = TEXT("Unknown");
			switch (Result.ActionType)
			{
			case ELedgeActionType::Vault: ActionName = TEXT("Vault"); break;
			case ELedgeActionType::LowMantle: ActionName = TEXT("Low Mantle"); break;
			case ELedgeActionType::HighMantle: ActionName = TEXT("High Mantle"); break;
			default: break;
			}

			const FString Msg = FString::Printf(TEXT("[Ledge Detected] Type: %s | Height: %.1f cm | Target: %s"),
				*ActionName, Result.LedgeHeight, *Result.TargetLandingLocation.ToCompactString());

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Green, Msg);
			}
			UE_LOG(LogTemp, Log, TEXT("%s"), *Msg);

			return;
		}
	}

	Super::Jump();
}

void ALedgeCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Ensure the camera follows the controller rotation even if overridden in a Blueprint child
	if (FirstPersonCameraComponent)
	{
		FirstPersonCameraComponent->bUsePawnControlRotation = true;
	}
}

void ALedgeCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// Add Input Mapping Context to Enhanced Input Subsystem
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
	const FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// Find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// Get forward and right direction vectors
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// Add movement
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void ALedgeCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}
