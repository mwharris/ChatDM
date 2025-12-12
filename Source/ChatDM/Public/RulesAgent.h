// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChatAgent.h"
#include "ChatMessage.h"
#include "RulesAgent.generated.h"

/** Broadcasts the final JSON result of the rules evaluation back to the Manager */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnRulesResultReady, const FRulesUpdate&, RulesWorldStateUpdate, const FString&, RulesResultJson, const FString&, PlayerInput);

/**
 * Responsible for sending messages to the AI Rules Agent 
 */
UCLASS()
class CHATDM_API URulesAgent : public UChatAgent
{
	GENERATED_BODY()
	
public:
	/** Fired when the RulesAgent has processed the HTTP response */
	UPROPERTY(BlueprintAssignable, Category="ChatDM | RulesAgent")
	FOnRulesResultReady OnRulesResultReady;
	
	virtual void Initialize(const FString& InPrompt) override;

	/** Send a message to the Rules Agent. */
	void SendMessage(const FString& PlayerInput, const FString& WorldStateJson);

private:
	TArray<FChatMessage> MessageLog;

	virtual void HandleResponse(const FString& ResponseContent, const FString& PlayerInput) override;

	void JsonToRulesUpdate(const FString& InJson, FRulesUpdate& OutRulesUpdate, FString& OutRulesResultJson);
};