// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LedgeDetectionTypes.generated.h"

UENUM(BlueprintType)
enum class ELedgeActionType : uint8
{
	None,
	Vault,
	LowMantle,
	HighMantle
};

USTRUCT(BlueprintType)
struct LEDGEDETECTION_API FLedgeDetectionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ledge Detection")
	bool bLedgeFound = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ledge Detection")
	ELedgeActionType ActionType = ELedgeActionType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Ledge Detection")
	FVector WallLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Ledge Detection")
	FVector WallNormal = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Ledge Detection")
	FVector LedgeLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Ledge Detection")
	FVector LedgeNormal = FVector::UpVector;

	UPROPERTY(BlueprintReadOnly, Category = "Ledge Detection")
	float LedgeHeight = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Ledge Detection")
	bool bIsVaultable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ledge Detection")
	FVector TargetLandingLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Ledge Detection")
	FVector VaultLandingLocation = FVector::ZeroVector;
};
