#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "UObject/NoExportTypes.h"
#include "ChatGPTManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnChatGptResponseReceived, const FString&, Response, bool, IsPlayer);

/* Struct for storing a message in message history */
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

/* Handle sending HTTP requests to ChatGPT API */
UCLASS(Blueprintable)
class CHATDM_API UChatGPTManager : public UObject
{
	GENERATED_BODY()

public:
	/* Delegate fired when we receive a response from ChatGPT */
	UPROPERTY(BlueprintAssignable, Category="ChatDM")
	FOnChatGptResponseReceived OnChatGptResponseReceived;

	/* Initialize fn for pulling the prompts DT and sending a setup message to ChatGPT */
	UFUNCTION(BlueprintCallable, Category = "ChatGPT")
	void Initialize();
	
	/* Send a chat request to ChatGPT */
	UFUNCTION(BlueprintCallable, Category = "ChatDM")
	void SendChatRequest(const FString& SystemMessageString, const FString& UserInput);

	/* Overloaded SendChatRequest() for sending messages that don't include a system message */
	void SendChatRequest(const FString& UserInput);

	/* Fake a message to ChatGPT just to test the message history and chat display */
	void SendTestChatRequest(const FString& UserInput);
	
private:
	/* We need to store all message history ourselves */
	bool bSystemMessageSent = false;

	/* For tracking the amount of tokens we use during a session */
	int32 TokenUsage = 0;
	
	/* We need to store all message history ourselves */
	TArray<FChatMessage> MessageLog;

	/* Add a message to our log */
	void AddMessageToLog(const FString& Role, const FString& Content);
	
	/* Handle a response from ChatGPT.  Deserialize and extract JSON */
	void HandleResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

};