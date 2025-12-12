// Fill out your copyright notice in the Description page of Project Settings.

#include "ChatAgent.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

void UChatAgent::Initialize(const FString& InPrompt)
{
	SystemMessage = FChatMessage("system", InPrompt);
}

void UChatAgent::SendMessage(TArray<FChatMessage>& MessageLog,
                             TFunction<void(const FString& ResponseContent)> OnResponseCallback)
{
	const FString OpenAiUrl = TEXT("https://api.openai.com/v1/chat/completions");

	// Set up the HTTP request
	const TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(OpenAiUrl);
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), TEXT("")));

	TArray<TSharedPtr<FJsonValue>> MessagesArray;

	// Translate the MessageLog to JSON for the upcoming HTTP request.
	// NOTE: We expect the System and the latest User messages to be in the received MessageLog.
	if (MessageLog.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("========== Translating message log =========="));
		for (const FChatMessage& Message : MessageLog)
		{
			TSharedPtr<FJsonObject> MessageObject = MakeShareable(new FJsonObject());
			MessageObject->SetStringField(TEXT("role"), Message.Role);
			MessageObject->SetStringField(TEXT("content"), Message.Content);
			MessagesArray.Add(MakeShareable(new FJsonValueObject(MessageObject)));
			UE_LOG(LogTemp, Log, TEXT("- %s: %s"), *Message.Role, *Message.Content);
		}
	}

	// Set up the JSON object that will be sent in the POST request
	const TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
	// Specify the model of GPT we want to use
	JsonObject->SetStringField(TEXT("model"), TEXT("gpt-4o"));
	// We need to pass the entire message array along with every message
	JsonObject->SetArrayField(TEXT("messages"), MessagesArray);

	// Serialize the JSON into a string for sending
	FString Payload;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	HttpRequest->SetContentAsString(Payload);

	// Bind to the response handler
	// HttpRequest->OnProcessRequestComplete().BindUObject(this, &UChatGPTManager::HandleResponse);

	HttpRequest->OnProcessRequestComplete().BindLambda(
		[OnResponseCallback](FHttpRequestPtr Request, const FHttpResponsePtr& Response, const bool bWasSuccessful)
		{
			// Ensure our HTTP was successful and we received a valid response
			if (!bWasSuccessful || !Response.IsValid())
			{
				if (Response.IsValid())
				{
					UE_LOG(LogTemp, Error, TEXT("HTTP Request failed: %d - %s"), Response->GetResponseCode(), *Response->GetContentAsString());
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("HTTP Request failed and response is invalid."));
				}
				return;
			}
			
			UE_LOG(LogTemp, Log, TEXT("HTTP Request successful: %s"), *Response->GetContentAsString());

			// Deserialize the response into JSON
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
			if (TSharedPtr<FJsonObject> JsonResponse; FJsonSerializer::Deserialize(Reader, JsonResponse))
			{
				// Extract the choices array from the JSON
				if (const TArray<TSharedPtr<FJsonValue>>* ChoicesArray; JsonResponse->TryGetArrayField(TEXT("choices"), ChoicesArray))
				{
					if (ChoicesArray && ChoicesArray->Num() > 0)
					{
						// Get the first choice object
						if (const TSharedPtr<FJsonValue> ChoiceObject = (*ChoicesArray)[0]; ChoiceObject.IsValid())
						{
							const TSharedPtr<FJsonObject> MessageObject = ChoiceObject->AsObject()->GetObjectField(TEXT("message"));

							// Extract the message from the choice object
							if (MessageObject.IsValid())
							{
								// Extract ChatGPT's response from inside the content field
								const FString Reply = MessageObject->GetStringField(TEXT("content"));
								UE_LOG(LogTemp, Log, TEXT("ChatGPT Response: %s"), *Reply);

								OnResponseCallback(Reply);
							}
							else
							{
								UE_LOG(LogTemp, Error, TEXT("UChatAgent::SendMessage(): MessagesObject is invalid."));
							}
						}
						else
						{
							UE_LOG(LogTemp, Error, TEXT("UChatAgent::SendMessage(): ChoiceObject is invalid."));
						}
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT("UChatAgent::SendMessage(): ChoicesArray is invalid or empty."));
					}
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("UChatAgent::SendMessage(): failed to find the 'choices' array in the JSON response."));
				}

				// Track token usage
				if (const TSharedPtr<FJsonObject>* UsageObjectPtr; JsonResponse->TryGetObjectField(TEXT("usage"), UsageObjectPtr))
				{
					const TSharedPtr<FJsonObject> UsageObject = *UsageObjectPtr;
					
					// Access token usage fields within the usage object
					int32 PromptTokens = UsageObject->GetIntegerField(TEXT("prompt_tokens"));
					int32 CompletionTokens = UsageObject->GetIntegerField(TEXT("completion_tokens"));
					int32 TotalTokens = UsageObject->GetIntegerField(TEXT("total_tokens"));

					UE_LOG(LogTemp, Log, TEXT("Prompt Tokens: %d, Completion Tokens: %d, Total Tokens: %d"), PromptTokens, CompletionTokens, TotalTokens);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("Failed to retrieve 'usage' field as an object."));
				}
			}
		});

	// Finally, send the request
	HttpRequest->ProcessRequest();
}

FString UChatAgent::BuildWrappedUserMessage(const FString& CurrentWorldStateJson, const FString& RulesResultJson, const FString& PlayerInput) const
{
	// Wrap the message into a format that contains the world state AND the player's input
	return FString::Printf(
		TEXT("WORLDSTATE:\n%s\n\nRULESRESULT:\n%s\n\nPLAYERINPUT:\n%s"),
		*CurrentWorldStateJson,
		*RulesResultJson,
		*PlayerInput
	);
}

FString UChatAgent::BuildWrappedUserMessage(const FString& CurrentWorldStateJson, const FString& PlayerInput) const
{
	// Wrap the message into a format that contains the world state AND the player's input
	return FString::Printf(
		TEXT("WORLDSTATE:\n%s\n\nPLAYERINPUT:\n%s"),
		*CurrentWorldStateJson,
		*PlayerInput
	);
}
