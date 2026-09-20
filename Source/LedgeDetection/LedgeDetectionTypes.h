// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LedgeDetectionTypes.generated.h"

/** Categorization of mantle based on obstacle height */
UENUM(BlueprintType)
enum class EMantleType : uint8
{
	None UMETA(DisplayName = "None"),
	LowMantle UMETA(DisplayName = "Low Mantle (1m)"),
	HighMantle UMETA(DisplayName = "High Mantle (2m)")
};

/** Data package containing all geometric information needed for mantling */
USTRUCT(BlueprintType)
struct FMantleLedgeInfo
{
	GENERATED_BODY()

	/** Target world transform where the character capsule should end up */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge Detection")
	FTransform TargetTransform = FTransform::Identity;

	/** Location of the wall impact */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge Detection")
	FVector WallLocation = FVector::ZeroVector;

	/** Surface normal of the wall face */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge Detection")
	FVector WallNormal = FVector::ZeroVector;

	/** Top surface location of the ledge */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge Detection")
	FVector LedgeLocation = FVector::ZeroVector;

	/** Front corner lip of the ledge where wall face meets top surface */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge Detection")
	FVector LedgeFrontLip = FVector::ZeroVector;

	/** Height of the ledge relative to the character's feet */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge Detection")
	float MantleHeight = 0.0f;

	/** Categorized mantle type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge Detection")
	EMantleType MantleType = EMantleType::None;

	/** Whether a valid mantle ledge was found and verified */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ledge Detection")
	bool bSuccess = false;
};
