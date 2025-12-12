// Fill out your copyright notice in the Description page of Project Settings.

#include "NarratorAgent.h"

#include "ChatDM/Public/ChatPromptRow.h"

void UNarratorAgent::Initialize(const FString& InPrompt)
{
	UE_LOG(LogTemp, Log, TEXT("UNarratorAgent::Initialize(): NarratorAgent initialized."));

	// Get a reference to the prompts DT
	static const FString DataTablePath = TEXT("/Game/Assets/DT_Prompts.DT_Prompts");
	UDataTable* DataTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, *DataTablePath));
	if (!DataTable)
	{
		UE_LOG(LogTemp, Error, TEXT("UNarratorAgent::Initialize(): Failed to load DataTable at %s"), *DataTablePath);
		return;
	}

	// Get the initialize row from the DT
	FName RowName = TEXT("Narrator_SystemMessage");
	FChatPromptRow* Row = DataTable->FindRow<FChatPromptRow>(RowName, TEXT("UNarratorAgent::Initialize"), true);
	if (!Row)
	{
		UE_LOG(LogTemp, Error, TEXT("UNarratorAgent::Initialize(): Row %s not found in DataTable."), *RowName.ToString());
		return;
	}

	// Pull out the system message prompt
	const FString InitializePrompt = Row->PromptText;
	UE_LOG(LogTemp, Log, TEXT("UNarratorAgent::Initialize(): Loaded Prompt: %s"), *InitializePrompt);

	// Call parent to finish initialization with the  system message
	SystemMessage = FChatMessage("system", InitializePrompt);
	MessageLog.Push(SystemMessage);

	// Also get the startup prompt for the initial message from the DT
	RowName = TEXT("Narrator_StartupPrompt");
	Row = DataTable->FindRow<FChatPromptRow>(RowName, TEXT("UNarratorAgent::SendInitialMessage"), true);
	if (!Row)
	{
		UE_LOG(LogTemp, Error, TEXT("UNarratorAgent::Initialize(): Row %s not found in DataTable."), *RowName.ToString());
		return;
	}

	StartupPrompt = Row->PromptText;
}

void UNarratorAgent::SendInitialMessage(const FString& CurrentWorldStateJson)
{
	if (StartupPrompt.IsEmpty())
	{
		StartupPrompt = TEXT("Describe the player's arrival into the room described by the WorldState. Set the scene and invite them to act.");
	}
	
	// Wrap the World State and the Startup Prompt into a single message.
	const FString WrappedMessage = BuildWrappedUserMessage(CurrentWorldStateJson, StartupPrompt);
	
	// Update the message log with the new message
	const FChatMessage NewMessage = FChatMessage("user", WrappedMessage);
	MessageLog.Push(NewMessage);

	// Call the parent to actually send the message to AI
	Super::SendMessage(MessageLog,
		[this](const FString& ResponseContent)
		{
			HandleResponse(ResponseContent, StartupPrompt);
		});
}

void UNarratorAgent::SendMessage(const FString& PlayerInput, const FString& CurrentWorldStateJson, const FString& RulesResultJson)
{
	// Wrap the World State and Player Input into a single message.
	const FString WrappedMessage = BuildWrappedUserMessage(CurrentWorldStateJson, RulesResultJson, PlayerInput);
	
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

void UNarratorAgent::HandleResponse(const FString& ResponseContent, const FString& PlayerInput)
{
	UE_LOG(LogTemp, Log, TEXT("[NarratorAgent] Response: %s"), *ResponseContent);

	// TODO: Actually process the response...

	if (OnNarratorResultReady.IsBound())
	{
		OnNarratorResultReady.Broadcast(ResponseContent, PlayerInput);
	}
}