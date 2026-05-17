// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChatAgent.h"
#include "WorldStateAgent.generated.h"

/** Broadcasts the final JSON result of the rules evaluation back to the Manager */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnWorldReactionReady, const FWorldReaction&, WorldReaction, const FString&, RulesResultJson, const FString&, WorldReactionJson, const FString&, PlayerInput);

/**
 * Responsible for controlling enemies and environment
 */
UCLASS()
class CHATDM_API UWorldStateAgent : public UChatAgent
{
	GENERATED_BODY()

public:
	/** Fired when the WorldStateAgent has processed the HTTP response */
	UPROPERTY(BlueprintAssignable, Category="ChatDM | WorldStateAgent")
	FOnWorldReactionReady OnWorldReactionReady;
	
	/** Override from ChatAgent */
	virtual void Initialize(const FString& InPrompt) override;

	/** Send a message to the WorldState Agent. PlayerInput and RulesResultJson are passed through to the delegate so the manager can forward them to NarratorAgent. */
	void SendMessage(const FString& PlayerInput, const FString& WorldStateJson, const FString& RulesResultJson);

private:
	/** Cache the RulesResultJson so we don't need to pass it around too much. */
	FString CachedRulesResultJson;
	
	/** Message history. */
	TArray<FChatMessage> MessageLog;

	/** Handle response from ChatGPT. */
	virtual void HandleResponse(const FString& ResponseContent, const FString& PlayerInput) override;

	/** Helper function to parse the JSON from the response into structs. */
	void JsonToWorldReaction(const FString& InJson, FWorldReaction& OutReaction, FString& OutReactionJson);
};