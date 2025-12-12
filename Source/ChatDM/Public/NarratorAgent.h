// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChatAgent.h"
#include "NarratorAgent.generated.h"

/** Broadcasts the final result of the narration back to the Manager */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNarratorResultReady, const FString&, Result, const FString&, PlayerInput);

/**
 * Responsible for sending messages to the AI Narrator Agent 
 */
UCLASS()
class CHATDM_API UNarratorAgent : public UChatAgent
{
	GENERATED_BODY()

public:
	/** Fired when the RulesAgent has processed the HTTP response */
	UPROPERTY(BlueprintAssignable, Category="ChatDM | NarratorAgent")
	FOnNarratorResultReady OnNarratorResultReady;
	
	virtual void Initialize(const FString& InPrompt) override;

	void SendInitialMessage(const FString& WorldStateJson);
	
	/** Send a message to the Narrator Agent. */
	void SendMessage(const FString& PlayerInput, const FString& CurrentWorldStateJson, const FString& RulesResultJson);

private:
	TArray<FChatMessage> MessageLog;

	FString StartupPrompt;
	
	virtual void HandleResponse(const FString& ResponseContent, const FString& PlayerInput) override;
	
};