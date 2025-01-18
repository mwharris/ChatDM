#include "ChatGPTManager.h"

#include "ChatPromptRow.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

void UChatGPTManager::Initialize()
{
	UE_LOG(LogTemp, Log, TEXT("ChatGPTManager initialized."));

	// Get a reference to the prompts DT
	static const FString DataTablePath = TEXT("/Game/Assets/DT_Prompts.DT_Prompts");
	UDataTable* DataTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, *DataTablePath));
	if (!DataTable)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load DataTable at %s"), *DataTablePath);
		return;
	}

	// Get the initialize row from the DT
	const FName RowName = TEXT("Initialize");
	FChatPromptRow* Row = DataTable->FindRow<FChatPromptRow>(RowName, TEXT("ChatGPTManager::Initialize"));
	if (!Row)
	{
		UE_LOG(LogTemp, Error, TEXT("Row %s not found in DataTable."), *RowName.ToString());
		return;
	}
	
	// Pull out the initialize prompt
	FString InitializePrompt = Row->PromptText;
	UE_LOG(LogTemp, Log, TEXT("Loaded Prompt: %s"), *InitializePrompt);

	// Send the system message to set up ChatDM for player requests later
	SendChatRequest(InitializePrompt, "");
}

void UChatGPTManager::SendChatRequest(const FString& UserInput)
{
	// Send a chat request that doesn't include a system message
	SendChatRequest("", UserInput);
}

void UChatGPTManager::SendChatRequest(const FString& SystemMessageString, const FString& UserInput)
{
	const FString OpenAiUrl = TEXT("https://api.openai.com/v1/chat/completions");
	const FString ApiKey = TEXT("OPENAI_API_KEY_GOES_HERE");

	// Set up the HTTP request
	const TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(OpenAiUrl);
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));

	bool bPrintDebugMessageLog = false;
	TArray<TSharedPtr<FJsonValue>> MessagesArray;

	// Only send the system message once
	if (!bSystemMessageSent && !SystemMessageString.IsEmpty())
	{
		bSystemMessageSent = true;
		
		// System Message
		// Defines the model's behavior, tone, and context.
		// Sets up the rules and framing for how the AI should respond.
		const TSharedPtr<FJsonObject> SystemMessage = MakeShareable(new FJsonObject());
		SystemMessage->SetStringField(TEXT("role"), TEXT("system"));
		SystemMessage->SetStringField(TEXT("content"), SystemMessageString);
		MessagesArray.Add(MakeShareable(new FJsonValueObject(SystemMessage)));
		
		AddMessageToLog("system", SystemMessageString);
	}
	// Send along the message history so ChatGPT has the context needed
	else if (bSystemMessageSent) 
	{
		bPrintDebugMessageLog = true;
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

	// User Message
	// Represents the user's input/query to the API.
	if (!UserInput.IsEmpty())
	{
		const TSharedPtr<FJsonObject> UserMessage = MakeShareable(new FJsonObject());
		UserMessage->SetStringField(TEXT("role"), TEXT("user"));
		UserMessage->SetStringField(TEXT("content"), UserInput);
		MessagesArray.Add(MakeShareable(new FJsonValueObject(UserMessage)));
		
		if (bPrintDebugMessageLog)
		{
			UE_LOG(LogTemp, Log, TEXT("- user: %s"), *UserInput);
			UE_LOG(LogTemp, Log, TEXT("============= End message log ============="));
		}
	}

	// Set up the JSON object that will be sent in the POST request
	const TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
	// Specify the model of GPT we want to use
	JsonObject->SetStringField(TEXT("model"), TEXT("gpt-4-turbo"));
	// We need to pass the entire message array along with every message
	JsonObject->SetArrayField(TEXT("messages"), MessagesArray);

	// Serialize the JSON into a string for sending
	FString Payload;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	HttpRequest->SetContentAsString(Payload);

	// Bind to the response handler
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UChatGPTManager::HandleResponse);

	// Finally, send the request
	HttpRequest->ProcessRequest();

	// Add the user's message to the log
	AddMessageToLog("user", UserInput);

	// TODO: Remove this
	/*
	AddMessageToLog("assistant", "Message received.");
	OnChatGptResponseReceived.Broadcast("Message received.", false);
	*/
}

void UChatGPTManager::SendTestChatRequest(const FString& UserInput)
{
	// Pretend we sent a message by adding to our message history and broadcasting.
	AddMessageToLog("user", UserInput);
	AddMessageToLog("assistant", "Message received.");
	OnChatGptResponseReceived.Broadcast("Message received.", false);
}

void UChatGPTManager::HandleResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, const bool bWasSuccessful)
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
		OnChatGptResponseReceived.Broadcast(TEXT("Received failed or invalid response."), false);
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
					// Extract the message from the choice object
					if (const TSharedPtr<FJsonObject> MessageObject = ChoiceObject->AsObject()->GetObjectField(TEXT("message")); MessageObject.IsValid())
					{
						// Extract ChatGPT's response from inside the content field
						const FString Reply = MessageObject->GetStringField(TEXT("content"));
						UE_LOG(LogTemp, Log, TEXT("ChatGPT Response: %s"), *Reply);

						// Add ChatGPT's response to the log and broadcast our results
						AddMessageToLog("assistant", Reply);
						OnChatGptResponseReceived.Broadcast(Reply, false);
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT("UChatGPTManager::HandleResponse(): MessagesObject is invalid."));
					}
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("UChatGPTManager::HandleResponse(): ChoiceObject is invalid."));
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("UChatGPTManager::HandleResponse(): ChoicesArray is invalid or empty."));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UChatGPTManager::HandleResponse(): failed to find the 'choices' array in the JSON response."));
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
}

void UChatGPTManager::AddMessageToLog(const FString& Role, const FString& Content)
{
	MessageLog.Add(FChatMessage(Role, Content));
}