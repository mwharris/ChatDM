// Fill out your copyright notice in the Description page of Project Settings.

#include "RulesAgent.h"

#include "ChatDM/Public/ChatPromptRow.h"
#include "Enemy.h"
#include "JsonObjectConverter.h"
#include "Room.h"
#include "RulesUpdate.h"
#include "WorldState.h"

void URulesAgent::Initialize(const FString& InPrompt)
{
	UE_LOG(LogTemp, Log, TEXT("URulesAgent::Initialize(): RulesAgent initialized."));

	// Get a reference to the prompts DT
	static const FString DataTablePath = TEXT("/Game/Assets/DT_Prompts.DT_Prompts");
	UDataTable* DataTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, *DataTablePath));
	if (!DataTable)
	{
		UE_LOG(LogTemp, Error, TEXT("URulesAgent::Initialize(): Failed to load DataTable at %s"), *DataTablePath);
		return;
	}

	// Get the initialize row from the DT
	if (!TryLoadPromptRow(DataTable,TEXT("Rules_SystemMessage"),    TEXT("URulesAgent::Initialize"), SystemPrompt))
	{
		return;
	}
	
	SystemMessage = FChatMessage("system", SystemPrompt);
}

void URulesAgent::SendMessage(const FString& PlayerInput, const FString& WorldStateJson)
{
	// Wrap the World State and Player Input into a single message.
	const FString WrappedMessage = BuildWrappedUserMessage(WorldStateJson, PlayerInput);
	
	// No need to track all messages, just send System message and the new message
	TArray<FChatMessage> SingleTurnMessages;
	SingleTurnMessages.Push(SystemMessage);
	SingleTurnMessages.Push(FChatMessage("user", WrappedMessage));

	// Pass a callback so we can handle the async response.
	Super::SendMessage(SingleTurnMessages,
		[this, PlayerInput](const FString& ResponseContent)
		{
			HandleResponse(ResponseContent, PlayerInput);
		});
}

void URulesAgent::HandleResponse(const FString& ResponseContent, const FString& PlayerInput)
{
	UE_LOG(LogTemp, Log, TEXT("[RulesAgent::HandleResponse] Response (Raw): %s"), *ResponseContent);

	FString RulesResultJson;
	FRulesUpdate RulesWorldStateUpdate;
	JsonToRulesUpdate(ResponseContent, RulesWorldStateUpdate, RulesResultJson);

	UE_LOG(LogTemp, Warning, TEXT("[RulesAgent::HandleResponse] Player action bSuccess=%s"), RulesWorldStateUpdate.bSuccess ? TEXT("true") : TEXT("false"));

	if (OnRulesResultReady.IsBound())
	{
		OnRulesResultReady.Broadcast(RulesWorldStateUpdate, RulesResultJson, PlayerInput);
	}
}

void URulesAgent::JsonToRulesUpdate(const FString& InJson, FRulesUpdate& OutRulesUpdate, FString& OutRulesResultJson)
{
	FString CleanJson = InJson;

	// Remove whitespace/newlines
	CleanJson.TrimStartAndEndInline();

	// Remove UTF-8 BOM if present
	if (CleanJson.Len() > 0 && CleanJson[0] == 0xFEFF)
	{
		CleanJson.RemoveAt(0);
	}

	// Strip markdown code fences
	CleanJson = CleanJson.Replace(TEXT("```json"), TEXT(""));
	CleanJson = CleanJson.Replace(TEXT("```JSON"), TEXT(""));
	CleanJson = CleanJson.Replace(TEXT("```"), TEXT(""));
	OutRulesResultJson = CleanJson;
	
	UE_LOG(LogTemp, Log, TEXT("[RulesAgent::JsonToRulesUpdate] Cleaned JSON: %s"), *CleanJson);
	
	TSharedPtr<FJsonObject> RootObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(CleanJson);
	if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[RulesAgent::JsonToRulesUpdate] Failed to parse CleanJson into root object: %s"), *CleanJson);
		return;
	}
	
	// Now convert the parsed object into FRulesUpdate
	if (!FJsonObjectConverter::JsonObjectToUStruct(
		RootObj.ToSharedRef(),
		FRulesUpdate::StaticStruct(),
		&OutRulesUpdate,
		0, 0))
	{
		UE_LOG(LogTemp, Error, TEXT("[RulesAgent::JsonToRulesUpdate] Failed to convert JSON to FRulesUpdate struct."));
	}

	if (!OutRulesUpdate.bSuccess)
	{
		__nop();
		OutRulesUpdate.bSuccess = RootObj->GetBoolField(TEXT("success"));
		__nop();
	}
}
