// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "LedgeDetectionTypes.h"
#include "LedgeCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class ULedgeDetectionComponent;

UCLASS(config=Game)
class LEDGEDETECTION_API ALedgeCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	/** Ledge Detection Component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ledge Detection", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULedgeDetectionComponent> LedgeDetectionComponent;

public:
	ALedgeCharacter();

	virtual void Jump() override;
	virtual void Tick(float DeltaTime) override;

	/** MappingContext for player input */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	// --- Mantle Configuration & State ---

	/** Montage used for low ledges (approx 1m) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mantle|Animations")
	TObjectPtr<UAnimMontage> Mantle1mMontage;

	/** Montage used for high ledges (approx 2m) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mantle|Animations")
	TObjectPtr<UAnimMontage> Mantle2mMontage;

	// --- ALS Authentic Mantle Parameters ---

	/** High mantle (2m) starting offset (X = lateral, Y = forward distance from target, Z = vertical height below target) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mantle|ALS Config")
	FVector HighMantleStartingOffset = FVector(0.0f, 75.0f, 200.0f);

	/** Low mantle (1m) starting offset (X = lateral, Y = forward distance from target, Z = vertical height below target) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mantle|ALS Config")
	FVector LowMantleStartingOffset = FVector(0.0f, 65.0f, 100.0f);

	/** Duration for 2m High Mantle timeline */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mantle|ALS Config")
	float HighMantleDuration = 1.5f;

	/** Play rate for 2m High Mantle montage */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mantle|ALS Config")
	float HighMantlePlayRate = 1.25f;

	/** Duration for 1m Low Mantle timeline */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mantle|ALS Config")
	float LowMantleDuration = 1.0f;

	/** Play rate for 1m Low Mantle montage */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mantle|ALS Config")
	float LowMantlePlayRate = 1.25f;

	/** Duration to procedurally blend actual in-game start offset into the animated start offset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mantle|ALS Config")
	float MantleBlendInDuration = 0.35f;

	/** Optional ALS Position/Correction Curve (CurveVector: X = PositionAlpha, Y = XYCorrectionAlpha, Z = ZCorrectionAlpha) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mantle|ALS Config")
	TObjectPtr<UCurveVector> MantlePositionCurve;

	// --- Hand & Elbow IK Targets ---

	/** Target location for Left Hand IK in Component Space */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mantle|Hand IK")
	FVector LeftHandIK_Location = FVector::ZeroVector;

	/** Target location for Right Hand IK in Component Space */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mantle|Hand IK")
	FVector RightHandIK_Location = FVector::ZeroVector;

	/** Target location for Left Elbow Joint Target (Pole Vector) in Component Space */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mantle|Hand IK")
	FVector LeftElbowIK_Location = FVector::ZeroVector;

	/** Target location for Right Elbow Joint Target (Pole Vector) in Component Space */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mantle|Hand IK")
	FVector RightElbowIK_Location = FVector::ZeroVector;

	/** Blending weight for Hand IK (0.0 = free, 1.0 = pinned to ledge) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mantle|Hand IK")
	float HandIK_Weight = 0.0f;

	/** Returns whether the character is currently performing a mantle */
	UFUNCTION(BlueprintCallable, Category = "Mantle")
	bool IsMantling() const { return bIsMantling; }

	/** Starts the mantle transition using the detected ledge info */
	UFUNCTION(BlueprintCallable, Category = "Mantle")
	void StartMantle(const FMantleLedgeInfo& LedgeInfo);

	/** Called each frame during mantle to interpolate the actor transform */
	void UpdateMantle(float DeltaTime);

	/** Ends the mantle and restores standard movement physics */
	UFUNCTION(BlueprintCallable, Category = "Mantle")
	void EndMantle();

	/** ALS Math Library Helpers for Component-Wise Transform Subtraction and Addition */
	FORCEINLINE static FTransform TransformSub(const FTransform& T1, const FTransform& T2)
	{
		return FTransform(T1.GetRotation().Rotator() - T2.GetRotation().Rotator(),
		                  T1.GetLocation() - T2.GetLocation(),
		                  T1.GetScale3D() - T2.GetScale3D());
	}

	FORCEINLINE static FTransform TransformAdd(const FTransform& T1, const FTransform& T2)
	{
		return FTransform(T1.GetRotation().Rotator() + T2.GetRotation().Rotator(),
		                  T1.GetLocation() + T2.GetLocation(),
		                  T1.GetScale3D() + T2.GetScale3D());
	}

protected:
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

private:
	/** Active mantle state */
	UPROPERTY(VisibleAnywhere, Category = "Mantle|State")
	bool bIsMantling = false;

	/** Current ledge geometry package */
	FMantleLedgeInfo CurrentMantleLedgeInfo;

	/** Mantle Target Transform on top of ledge */
	FTransform MantleTarget = FTransform::Identity;

	/** Actual starting offset of the actor relative to target transform */
	FTransform MantleActualStartOffset = FTransform::Identity;

	/** Target animated start offset relative to target transform */
	FTransform MantleAnimatedStartOffset = FTransform::Identity;

	/** Play rate used for the current mantle montage */
	float MantlePlayRate = 1.25f;

	/** Elapsed time since mantle started */
	float MantleElapsedTime = 0.0f;

	/** Total duration of current mantle transition */
	float MantleDuration = 1.5f;

	/** Target locations in World Space for Hand & Elbow IK */
	FVector LeftHandWorldTarget = FVector::ZeroVector;
	FVector RightHandWorldTarget = FVector::ZeroVector;
	FVector LeftElbowWorldTarget = FVector::ZeroVector;
	FVector RightElbowWorldTarget = FVector::ZeroVector;

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	/** Returns LedgeDetectionComponent subobject **/
	FORCEINLINE ULedgeDetectionComponent* GetLedgeDetectionComponent() const { return LedgeDetectionComponent; }
};
