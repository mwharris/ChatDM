// Fill out your copyright notice in the Description page of Project Settings.

#include "ChatAgent.h"

#include "ChatPromptRow.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

void UChatAgent::Initialize(const FString& InPrompt)
{
	SystemMessage = FChatMessage("system", InPrompt);
}

void UChatAgent::SendMessageWithTools(TArray<TSharedPtr<FJsonObject>> Messages,
	const TArray<TSharedPtr<FJsonObject>>& Tools,
	TFunction<FString(const FString& ToolName, const TSharedPtr<FJsonObject>& Args)> OnToolCall,
	TFunction<void(const FString& FinalContent)> OnComplete)
{
	// Create the HTTP request
	const TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(TEXT("https://api.openai.com/v1/chat/completions"));
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *GetAPIKey()));

	// Convert message objects to JSON values for the payload
	TArray<TSharedPtr<FJsonValue>> MessagesArray;
	for (const TSharedPtr<FJsonObject>& Message : Messages)
	{
		MessagesArray.Add(MakeShareable(new FJsonValueObject(Message)));
	}

	// Convert tool definitions to JSON values the same way
	TArray<TSharedPtr<FJsonValue>> ToolsArray;
	for (const TSharedPtr<FJsonObject>& Tool : Tools)
	{
		ToolsArray.Add(MakeShareable(new FJsonValueObject(Tool)));
	}

	// Set up the JSON object that will be sent in the POST request
	const TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
	// Specify the model of GPT we want to use
	JsonObject->SetStringField(TEXT("model"), TEXT("gpt-4o"));
	// We need to pass any message history along with every message since AI is stateless
	JsonObject->SetArrayField(TEXT("messages"), MessagesArray);
	// Also send out tools array so the AI knows that tools are available for it to use
	JsonObject->SetArrayField(TEXT("tools"), ToolsArray);

	// Serialize the JSON into a string for sending
	FString PayloadString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PayloadString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	HttpRequest->SetContentAsString(PayloadString);

	HttpRequest->OnProcessRequestComplete().BindLambda(
		[this, Messages, Tools, OnToolCall, OnComplete]
		(FHttpRequestPtr Request, const FHttpResponsePtr& Response, const bool bWasSuccessful) mutable
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

			// Attempt to parse the response
			TSharedPtr<FJsonObject> JsonResponse;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
			if (!FJsonSerializer::Deserialize(Reader, JsonResponse) || !JsonResponse.IsValid())
			{
				UE_LOG(LogTemp, Error, TEXT("SendMessageWithTools: Failed to parse response."));
				return;
			}

			// Find the choices array
			const TArray<TSharedPtr<FJsonValue>>* Choices;
			if (!JsonResponse->TryGetArrayField(TEXT("choices"), Choices) || Choices->IsEmpty())
			{
				UE_LOG(LogTemp, Error, TEXT("SendMessageWithTools: No choices in response."));
				return;
			}

			// Get the first choice, find it's finish_reason
			const TSharedPtr<FJsonObject> Choice = (*Choices)[0]->AsObject();
			const FString FinishReason = Choice->GetStringField(TEXT("finish_reason"));
			const TSharedPtr<FJsonObject> AssistantMessage = Choice->GetObjectField(TEXT("message"));

			// "tool_calls" means the AI wants to call one or more tools before it is done.
			// We must execute each tool and loop back with the results before the model can continue
			// reasoning toward it's final answer.
			if (FinishReason == TEXT("tool_calls"))
			{
				// Notify ChatGPTManager that we're now processing tool calls
				OnStatusUpdate.Broadcast(TEXT("tool_calls"));

				// Add assistant message to the log
				Messages.Add(AssistantMessage);

				// Pull the list of tools calls from the message and process each
				const TArray<TSharedPtr<FJsonValue>>* ToolCalls;
				AssistantMessage->TryGetArrayField(TEXT("tool_calls"), ToolCalls);
				
				for (const TSharedPtr<FJsonValue>& ToolCallValue : *ToolCalls)
				{
					const TSharedPtr<FJsonObject> ToolCall = ToolCallValue->AsObject();

					// Toll call ID ties this result back to the specific call the model made.
					// OpenAI requires it to be on the tools result message.
					FString ToolCallId = ToolCall->GetStringField(TEXT("id"));
					
					// Pull several properties from the tool call JSON object
					const TSharedPtr<FJsonObject> Function = ToolCall->GetObjectField(TEXT("function"));
					FString ToolName = Function->GetStringField(TEXT("name"));
					FString ArgumentsStr = Function->GetStringField(TEXT("arguments"));

					// Arguments come as a JSON string, so we need to deserialize them
					TSharedPtr<FJsonObject> Arguments;
					TSharedRef<TJsonReader<>> ArgReader = TJsonReaderFactory<>::Create(ArgumentsStr);
					FJsonSerializer::Deserialize(ArgReader, Arguments);

                    UE_LOG(LogTemp, Log, TEXT("[ChatAgent] Tool called: %s"), *ToolName);

					// Execute the tool and get the results
					FString ToolResult = OnToolCall(ToolName, Arguments);

					// Add tool result message to log.
					// The AI needs to see what each tool returned before it can decide what to do next.
					// The tool_call_id links the results to the specific call that triggered it.
					TSharedPtr<FJsonObject> ToolResultMessage = MakeShareable(new FJsonObject());
					ToolResultMessage->SetStringField(TEXT("role"), TEXT("tool"));
					ToolResultMessage->SetStringField(TEXT("tool_call_id"), ToolCallId);
					ToolResultMessage->SetStringField(TEXT("content"), ToolResult);
					Messages.Add(ToolResultMessage);
				}

				// Send the request again with the tool results now applied.
				// The AI will continue reasoning from where it left off, but now with the tool results.
				SendMessageWithTools(Messages, Tools, OnToolCall, OnComplete);
			}
			// The AI is done, no more tool calls
			else
			{
				FString FinalContent;
				AssistantMessage->TryGetStringField(TEXT("content"), FinalContent);
				OnComplete(FinalContent);
			}
		});

	// Send the HTTP request
	HttpRequest->ProcessRequest();
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
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *GetAPIKey()));

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

bool UChatAgent::TryLoadPromptRow(const UDataTable* DataTable, const FName& RowName, const FString& CallerContext,
	FString& OutPrompt)
{
	FChatPromptRow* Row = DataTable->FindRow<FChatPromptRow>(RowName, *CallerContext, true);
	if (!Row)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: Row '%s' not found in DataTable."), *CallerContext, *RowName.ToString());
		return false;
	}
	OutPrompt = Row->PromptText;
	UE_LOG(LogTemp, Log, TEXT("UChatAgent::TryLoadPromptRow(%s): Loaded Prompt: %s"), *RowName.ToString(), *OutPrompt);
	return true;
}

TSharedPtr<FJsonObject> UChatAgent::ChatMessageToJSON(const FChatMessage& Message)
{
	TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject());
	Obj->SetStringField(TEXT("role"), Message.Role);
	Obj->SetStringField(TEXT("content"), Message.Content);
	return Obj;
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

FString UChatAgent::BuildWrappedUserMessage(const FString& CurrentWorldStateJson, const FString& RulesResultJson, const FString& WorldReactionJson, const FString& PlayerInput) const
{
	// Wrap the message into a format that contains the world state AND the player's input
	return FString::Printf(
		TEXT("WORLDSTATE:\n%s\n\nRULESRESULT:\n%s\n\nWORLDREACTION:\n%s\n\nPLAYERINPUT:\n%s"),
		*CurrentWorldStateJson,
		*RulesResultJson,
		*WorldReactionJson,
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

FString UChatAgent::GetAPIKey()
{
	// ProjectConfigDir() is the path to the config directory.
	// '/' operator acts as a path concatenation and joins the APIKeys.ini filename.
	const FString ConfigPath = FPaths::ProjectConfigDir() / TEXT("APIKeys.ini");

	// Load the file and pull the [ChatDM] section, OpenAIApiKey value.
	FString Key;
	GConfig->LoadFile(ConfigPath);
	GConfig->GetString(TEXT("ChatDM"), TEXT("OpenAIApiKey"), Key, ConfigPath);

	if (Key.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[ChatAgent] OpenAIApiKey not found in Config/APIKeys.ini"));
	}

	return Key;
}