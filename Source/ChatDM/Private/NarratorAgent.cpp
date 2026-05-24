// Fill out your copyright notice in the Description page of Project Settings.

#include "NarratorAgent.h"

#include "ChatDM/Public/ChatPromptRow.h"

void UNarratorAgent::Initialize(const FString& InPrompt)
{
	UE_LOG(LogTemp, Log, TEXT("UNarratorAgent::Initialize(): NarratorAgent initialized."));

	static const FString DataTablePath = TEXT("/Game/Assets/DT_Prompts.DT_Prompts");
	const UDataTable* DataTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, *DataTablePath));
	if (!DataTable)
	{
		UE_LOG(LogTemp, Error, TEXT("UNarratorAgent::Initialize(): Failed to load DataTable at %s"), *DataTablePath);
		return;
	}

	// Load our system, startup, and evaluation prompts
	if (!TryLoadPromptRow(DataTable,TEXT("Narrator_SystemMessage"),    TEXT("UNarratorAgent::Initialize"), SystemPrompt)) return;
	if (!TryLoadPromptRow(DataTable, TEXT("Narrator_StartupPrompt"),    TEXT("UNarratorAgent::Initialize"), StartupPrompt)) return;
	if (!TryLoadPromptRow(DataTable, TEXT("Narrator_EvaluationPrompt"), TEXT("UNarratorAgent::Initialize"), EvaluationPrompt)) return;

	// System message should always be sent
	SystemMessage = FChatMessage("system", SystemPrompt);
	MessageLog.Push(SystemMessage);
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

void UNarratorAgent::SendMessage(const FString& PlayerInput, const FString& CurrentWorldStateJson,
	const FString& RulesResultJson, const FString& WorldReactionJson)
{
	// Wrap the World State, Rules Result, and Player Input into a single message.
	const FString WrappedMessage = BuildWrappedUserMessage(CurrentWorldStateJson, RulesResultJson, WorldReactionJson, PlayerInput);
	
	// Update the message log with the new message
	MessageLog.Push(FChatMessage("user", WrappedMessage));

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


	// Re-validate the narration if we're not starting up
	if (MessageLog.Num() > 2)
	{
		// Push narration as an assistant message and then validate
		MessageLog.Push(FChatMessage("assistant", ResponseContent));
		ValidateNarration(ResponseContent, PlayerInput);
	}
	// If we are, just use the result as-is
	else
	{
		if (OnNarratorResultReady.IsBound())
		{
			OnNarratorResultReady.Broadcast(ResponseContent, PlayerInput);
		}
	}
}

void UNarratorAgent::ValidateNarration(const FString& Narration, const FString& PlayerInput)
{
	MessageLog.Push(FChatMessage("user", EvaluationPrompt));

	Super::SendMessage(MessageLog, [this, Narration, PlayerInput](const FString& EvalResponse)
	{
		// Always remove the eval prompt from the history
		MessageLog.Pop();

		FString FinalNarration = Narration;

		TSharedPtr<FJsonObject> EvalJson;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(EvalResponse);
		if (FJsonSerializer::Deserialize(Reader, EvalJson) && EvalJson.IsValid())
		{
			const bool bApproved = EvalJson->GetBoolField(TEXT("approved"));
			if (!bApproved)
			{
				FString Revision = EvalJson->GetStringField(TEXT("revision"));
				if (!Revision.IsEmpty())
				{
					// Replace original narration with revision in the log
					MessageLog.Pop();
					MessageLog.Push(FChatMessage("assistant", Revision));
					FinalNarration = Revision;
					UE_LOG(LogTemp, Log, TEXT("[NarratorAgent] Narration revised: %s"), *Revision);
				}
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("[NarratorAgent] Narration approved."));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[NarratorAgent] Failed to parse eval response, using original."));
		}
		
		// Broadcast that our final narration is ready
		if (OnNarratorResultReady.IsBound())
		{
			OnNarratorResultReady.Broadcast(FinalNarration, PlayerInput);
		}
	});
}
