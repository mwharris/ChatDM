// Fill out your copyright notice in the Description page of Project Settings.

#include "WorldStateAgent.h"

#include "ChatPromptRow.h"
#include "JsonObjectConverter.h"
#include "WorldReaction.h"

void UWorldStateAgent::Initialize(const FString& InPrompt)
{
	UE_LOG(LogTemp, Log, TEXT("UWorldStateAgent::Initialize(): NarratorAgent initialized."));

	// Get a reference to the prompts DT
	static const FString DataTablePath = TEXT("/Game/Assets/DT_Prompts.DT_Prompts");
	UDataTable* DataTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, *DataTablePath));
	if (!DataTable)
	{
		UE_LOG(LogTemp, Error, TEXT("UWorldStateAgent::Initialize(): Failed to load DataTable at %s"), *DataTablePath);
		return;
	}

	// Get the initialize row from the DT
	FName RowName = TEXT("WorldState_SystemMessage");
	FChatPromptRow* Row = DataTable->FindRow<FChatPromptRow>(RowName, TEXT("UWorldStateAgent::Initialize"), true);
	if (!Row)
	{
		UE_LOG(LogTemp, Error, TEXT("UWorldStateAgent::Initialize(): Row %s not found in DataTable."), *RowName.ToString());
		return;
	}

	// Pull out the system message prompt
	const FString InitializePrompt = Row->PromptText;
	UE_LOG(LogTemp, Log, TEXT("UWorldStateAgent::Initialize(): Loaded Prompt: %s"), *InitializePrompt);

	// Call parent to finish initialization with the  system message
	SystemMessage = FChatMessage("system", InitializePrompt);
	MessageLog.Push(SystemMessage);
}

void UWorldStateAgent::SendMessage(const FString& PlayerInput, const FString& WorldStateJson, const FString& RulesResultJson)
{
	CachedRulesResultJson = RulesResultJson;

	const FString WrappedMessage = BuildWrappedUserMessage(WorldStateJson, RulesResultJson, PlayerInput);
	MessageLog.Push(FChatMessage("user", WrappedMessage));

	Super::SendMessage(MessageLog,
		[this, PlayerInput](const FString& ResponseContent)
		{
			HandleResponse(ResponseContent, PlayerInput);
		});
}

void UWorldStateAgent::HandleResponse(const FString& ResponseContent, const FString& PlayerInput)
{
	FString WorldReactionJson;
	FWorldReaction WorldReaction;
	JsonToWorldReaction(ResponseContent, WorldReaction, WorldReactionJson);

	if (OnWorldReactionReady.IsBound())
	{
		OnWorldReactionReady.Broadcast(WorldReaction, CachedRulesResultJson, WorldReactionJson, PlayerInput);
	}
}

void UWorldStateAgent::JsonToWorldReaction(const FString& InJson, FWorldReaction& OutReaction, FString& OutReactionJson)
{
	FString CleanJson = InJson;

	CleanJson.TrimStartAndEndInline();

	if (CleanJson.Len() > 0 && CleanJson[0] == 0xFEFF)
	{
		CleanJson.RemoveAt(0);
	}

	CleanJson = CleanJson.Replace(TEXT("```json"), TEXT(""));
	CleanJson = CleanJson.Replace(TEXT("```JSON"), TEXT(""));
	CleanJson = CleanJson.Replace(TEXT("```"), TEXT(""));
	OutReactionJson = CleanJson;

	UE_LOG(LogTemp, Log, TEXT("[WorldStateAgent::JsonToWorldReaction] Cleaned JSON: %s"), *CleanJson);

	TSharedPtr<FJsonObject> RootObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(CleanJson);
	if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[WorldStateAgent::JsonToWorldReaction] Failed to parse JSON: %s"), *CleanJson);
		return;
	}

	if (!FJsonObjectConverter::JsonObjectToUStruct(
		RootObj.ToSharedRef(),
		FWorldReaction::StaticStruct(),
		&OutReaction,
		0, 0))
	{
		UE_LOG(LogTemp, Error, TEXT("[WorldStateAgent::JsonToWorldReaction] Failed to convert JSON to FWorldReaction."));
	}
}
