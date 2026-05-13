// Fill out your copyright notice in the Description page of Project Settings.


#include "WorldStateAgent.h"

#include "ChatPromptRow.h"

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

void UWorldStateAgent::SendMessage(const FString& PlayerInput, const FString& WorldStateJson)
{
	// Wrap the World State and Player Input into a single message.
	const FString WrappedMessage = BuildWrappedUserMessage(WorldStateJson, PlayerInput);
	
	// Update the message log with the new message
	const FChatMessage NewMessage = FChatMessage("user", WrappedMessage);
	MessageLog.Push(NewMessage);

	// Call the parent to actually send the message to AI
	Super::SendMessage(MessageLog,
		[this, PlayerInput](const FString& ResponseContent)
		{
			HandleResponse(ResponseContent, PlayerInput);
		});
}

void UWorldStateAgent::HandleResponse(const FString& ResponseContent, const FString& PlayerInput)
{
	
}
