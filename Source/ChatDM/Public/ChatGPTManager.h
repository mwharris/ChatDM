#pragma once

#include "CoreMinimal.h"
#include "ChatMessage.h"
#include "Interfaces/IHttpRequest.h"
#include "NarratorAgent.h"
#include "RulesAgent.h"
#include "WorldState.h"

#include "ChatGPTManager.generated.h"

struct FRulesUpdate;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnChatGptResponseReceived, const FString&, Response, bool, IsPlayer);

/* Handle sending HTTP requests to ChatGPT API */
UCLASS(Blueprintable)
class CHATDM_API UChatGPTManager : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	URulesAgent* RulesAgent;

	UPROPERTY()
	UNarratorAgent* NarratorAgent;

	UPROPERTY()
	FWorldState WorldState;

	/* Delegate fired when we receive a response from ChatGPT */
	UPROPERTY(BlueprintAssignable, Category="ChatDM")
	FOnChatGptResponseReceived OnChatGptResponseReceived;

	/* Initialize fn for pulling the prompts DT and sending a setup message to ChatGPT */
	UFUNCTION(BlueprintCallable, Category = "ChatGPT")
	void Initialize();

	/* Helper function to initialize our AI agents */
	void InitializeAgents();

	UFUNCTION(BlueprintCallable, Category = "ChatGPT")
	void Deinitialize();
	
	/* Send the initial chat request to ChatGPT, which only required the Narrator Agent. */
	UFUNCTION(BlueprintCallable, Category = "ChatDM")
	void SendInitialChatRequest();
	
	/* Send a chat request to ChatGPT */
	UFUNCTION(BlueprintCallable, Category = "ChatDM")
	void SendAgentChatRequest(const FString& PlayerInput);

	/* Send player input to the Rules Agent for processing */
	void ExecuteRulesAgent(const FString& PlayerInput);
	
	/* Send player input to the Narrator Agent for processing */
	void ExecuteNarratorAgent(const FString& PlayerInput, const FString& RulesResultJson = TEXT("N/A"), const bool bIsInitial = false);
	
private:
	/* We need to store all message history ourselves */
	bool bSystemMessageSent = false;

	/* For tracking the amount of tokens we use during a session */
	int32 TokenUsage = 0;
	
	/* We need to store all message history ourselves */
	TArray<FChatMessage> MessageLog;

	/* Handle the result of the RulesAgent processing AI's response */
	UFUNCTION()
	void HandleRulesResult(const FRulesUpdate& RulesWorldStateUpdate, const FString& RulesResultJson, const FString& PlayerInput);

	/* Handle the result of the NarratorAgent processing AI's response */
	UFUNCTION()
	void HandleNarratorResult(const FString& NarratorResult, const FString& PlayerInput);

	/* Helper function to convert FWorldState into JSON needed for HTTP requests. */
	FString WorldStateToJson(const FWorldState& State);

};