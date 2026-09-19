// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "LedgeDetectionTypes.h"
#include "LedgeCharacter.generated.h"

class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class ULedgeDetectionComponent;

UCLASS(config=Game)
class LEDGEDETECTION_API ALedgeCharacter : public ACharacter
{
	GENERATED_BODY()

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FirstPersonCameraComponent;

	/** Ledge Detection Component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ledge Detection", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULedgeDetectionComponent> LedgeDetectionComponent;

public:
	ALedgeCharacter();

	virtual void Jump() override;

	/** MappingContext for player input */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

protected:
	virtual void BeginPlay() override;
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

public:
	/** Returns true if currently mantling or vaulting */
	UFUNCTION(BlueprintCallable, Category = "Ledge Detection")
	bool IsLedgeTransitioning() const;

	/** Returns FirstPersonCameraComponent subobject **/
	FORCEINLINE UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }
};
