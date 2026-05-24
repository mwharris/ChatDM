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
	
	/** Override from ChatAgent */
	virtual void Initialize(const FString& InPrompt) override;

	/** Sends the initial prompt to start the dialogue and setup the scene for the player */
	void SendInitialMessage(const FString& WorldStateJson);
	
	/** Send a message to the Narrator Agent. */
	void SendMessage(const FString& PlayerInput, const FString& CurrentWorldStateJson, const FString& RulesResultJson, const FString& WorldReactionJson);

private:
	/** History of messages between the player and this agent. */
	TArray<FChatMessage> MessageLog;

	/** The prompt that will be sent in SendInitialMessage. */
	FString StartupPrompt;

	/** The prompt that will be used to tell Narrator to re-evaluate its original narration. */
	FString EvaluationPrompt;
	
	/** Handle the response from the agent and broadcast back to the UI. */
	virtual void HandleResponse(const FString& ResponseContent, const FString& PlayerInput) override;

	/** TODO: Comment */
	void ValidateNarration(const FString& Narration, const FString& PlayerInput);
};