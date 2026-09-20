// Copyright Epic Games, Inc. All Rights Reserved.

#include "LedgeGameMode.h"
#include "LedgeCharacter.h"
#include "UObject/ConstructorHelpers.h"

ALedgeGameMode::ALedgeGameMode()
{
	// Set default pawn class to our C++ character (or BP child if set in editor)
	DefaultPawnClass = ALedgeCharacter::StaticClass();
}
