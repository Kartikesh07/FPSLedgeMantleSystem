// Copyright Epic Games, Inc. All Rights Reserved.

#include "LedgeGameMode.h"
#include "LedgeCharacter.h"

ALedgeGameMode::ALedgeGameMode()
	: Super()
{
	// Set default pawn class to our character
	DefaultPawnClass = ALedgeCharacter::StaticClass();
}
