// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChatMessage.h"
#include "ChatAgent.generated.h"

/**
 * Base class for the different AI agents
 */
UCLASS(Blueprintable)
class CHATDM_API UChatAgent : public UObject
{
	GENERATED_BODY()

public:
	const FString Model = TEXT("gpt-4o-mini");
	FChatMessage SystemMessage;

	UFUNCTION()
	virtual void Initialize(const FString& InPrompt);

	/** Generic send message function which will handle creating, sending, and passing back the result of the HTTP Request. */
	void SendMessage(TArray<FChatMessage>& MessageLog, TFunction<void(const FString& ResponseContent)> OnResponseCallback);

protected:
	/** Meant for children to override so they can handle the response as they see fit. */
	virtual void HandleResponse(const FString& ResponseContent, const FString& PlayerInput) {};

	/** Helper function that takes World State JSON and the Player's Input and wraps it in a single message. */
	FString BuildWrappedUserMessage(const FString& CurrentWorldStateJson, const FString& UpdatedWorldStateJson, const FString& PlayerInput) const;
	FString BuildWrappedUserMessage(const FString& CurrentWorldStateJson, const FString& PlayerInput) const;
	
};
