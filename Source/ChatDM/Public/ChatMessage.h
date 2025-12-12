#pragma once

#include "CoreMinimal.h"
#include "ChatMessage.generated.h"

/**
 * Represents a single message in a chat conversation
 * (for example: system, user, or assistant message)
 */
USTRUCT(BlueprintType)
struct FChatMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "ChatMessage")
	FString Role;

	UPROPERTY(BlueprintReadWrite, Category = "ChatMessage")
	FString Content;

	FChatMessage() {}
	FChatMessage(const FString& InRole, const FString& InContent) : Role(InRole), Content(InContent) {}
};