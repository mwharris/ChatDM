// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChatAgent.h"
#include "WorldStateAgent.generated.h"

/** Broadcasts the final JSON result of the rules evaluation back to the Manager */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWorldStateResultReady, const FString&, Result, const FString&, PlayerInput);

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
	FOnWorldStateResultReady OnWorldStateResultReady;
	
	virtual void Initialize(const FString& InPrompt) override;

	/** Send a message to the WorldState Agent. */
	void SendMessage(const FString& PlayerInput, const FString& WorldStateJson);

private:
	TArray<FChatMessage> MessageLog;

	virtual void HandleResponse(const FString& ResponseContent, const FString& PlayerInput) override;

};