#include "ChatGPTManager.h"

#include "ChatPromptRow.h"
#include "Enemy.h"
#include "HttpModule.h"
#include "JsonObjectConverter.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "NarratorAgent.h"
#include "RulesAgent.h"
#include "RulesUpdate.h"

void UChatGPTManager::Initialize()
{
	UE_LOG(LogTemp, Log, TEXT("ChatGPTManager initialized."));

	InitializeAgents();

	SendInitialChatRequest();
}

void UChatGPTManager::InitializeAgents()
{
	UE_LOG(LogTemp, Log, TEXT("Initializing AI agents."));
	
	RulesAgent = NewObject<URulesAgent>(this, URulesAgent::StaticClass(), TEXT("RulesAgent"));
	RulesAgent->Initialize(TEXT(""));

	NarratorAgent = NewObject<UNarratorAgent>(this, UNarratorAgent::StaticClass(), TEXT("NarratorAgent"));
	NarratorAgent->Initialize(TEXT(""));
	
	FEnemy Goblin;
	Goblin.EnemyIndex = 0;
	Goblin.Name = TEXT("Goblin");
	Goblin.Health = 3;
	Goblin.Status = "Idle";
	Goblin.IntentOrGoal = TEXT("The Goblin believes every item in the room belongs to him and will move to attack anyone that attempts to take one.");
	
	FRoom StartingRoom;
	StartingRoom.RoomIndex = 0;
	StartingRoom.Name = TEXT("Pedestal Chamber");
	StartingRoom.Description = TEXT("A circular stone room with an old pedestal in the center.");
	StartingRoom.Exits.Add(TEXT("North"));
	StartingRoom.Items.Add(TEXT("Key"));
	StartingRoom.Enemies.Add(Goblin);
	WorldState.Rooms.Add(StartingRoom);

	WorldState.CurrentRoomIndex = 0;
	WorldState.PlayerHeldItems.Add(TEXT("Longsword"));
	WorldState.PlayerHeldItems.Add(TEXT("Wooden Shield"));
	WorldState.PlayerHeldItems.Add(TEXT("Healing Potion"));
}

void UChatGPTManager::Deinitialize()
{
	RulesAgent->OnRulesResultReady.RemoveAll(this);
}

void UChatGPTManager::SendInitialChatRequest()
{
	// Send a request to ChatGPT to open the game with narration.
	ExecuteNarratorAgent(TEXT(""));
}

void UChatGPTManager::SendAgentChatRequest(const FString& PlayerInput)
{
	// The agent chain starts off with the Rules Agent who needs to decide if the player's action can even happen.
	ExecuteRulesAgent(PlayerInput);
}

void UChatGPTManager::ExecuteRulesAgent(const FString& PlayerInput)
{
	if (!IsValid(RulesAgent))
	{
		UE_LOG(LogTemp, Error, TEXT("UChatGPTManager::SendAgenticChatRequest(): Rules agent is invalid."));
		return;
	}

	// Bind so we can respond to the RulesAgent completing its request and response processing.
	if (!RulesAgent->OnRulesResultReady.IsAlreadyBound(this, &UChatGPTManager::HandleRulesResult))
	{
		RulesAgent->OnRulesResultReady.AddDynamic(this, &UChatGPTManager::HandleRulesResult);
	}

	// Tell the RulesAgent to send its request.
	const FString WorldStateJSON = WorldStateToJson(WorldState);
	RulesAgent->SendMessage(PlayerInput, *WorldStateJSON);
}

void UChatGPTManager::HandleRulesResult(const FRulesUpdate& RulesWorldStateUpdate, const FString& RulesResultJson, const FString& PlayerInput)
{
	// Update room information (items, enemies, etc.).
	if (!RulesWorldStateUpdate.StateChanges.Rooms.IsEmpty())
	{
		for (const auto& RoomDiff : RulesWorldStateUpdate.StateChanges.Rooms)
		{
			if (!WorldState.Rooms.IsValidIndex(RoomDiff.RoomIndex))
			{
				continue;
			}
			
			FRoom& RoomToUpdate = WorldState.Rooms[RoomDiff.RoomIndex];

			// Replace all items with the diff's items
			if (RoomDiff.Items != RoomToUpdate.Items)
			{
				RoomToUpdate.Items = RoomDiff.Items;
			}

			// Replace enemies fully, match on name
			for (const auto& EnemyDiff : RoomDiff.Enemies)
			{
				if (FEnemy* EnemyToUpdate = RoomToUpdate.Enemies.FindByPredicate([&](const FEnemy& E) { return E.Name == EnemyDiff.Name; }))
				{
					EnemyToUpdate->Health = EnemyDiff.Health;
					EnemyToUpdate->Status = EnemyDiff.Status;
				}
			}
		}
	}

	// Update player held items when it changes.
	if (!RulesWorldStateUpdate.StateChanges.PlayerHeldItems.IsEmpty())
	{
		WorldState.PlayerHeldItems = RulesWorldStateUpdate.StateChanges.PlayerHeldItems;
	}

	// Update the current room index if we changed rooms.
	if (RulesWorldStateUpdate.StateChanges.CurrentRoomIndex != WorldState.CurrentRoomIndex)
	{
		WorldState.CurrentRoomIndex = RulesWorldStateUpdate.StateChanges.CurrentRoomIndex;
	}
	
	ExecuteNarratorAgent(PlayerInput, RulesResultJson);
}

void UChatGPTManager::ExecuteNarratorAgent(const FString& PlayerInput, const FString& RulesResultJson /*=TEXT("N/A")*/, const bool bIsInitial /*=false*/)
{
	if (!IsValid(NarratorAgent))
	{
		UE_LOG(LogTemp, Error, TEXT("[UChatGPTManager::SendAgenticChatRequest()] Narrator agent is invalid."));
		return;
	}

	// Bind so we can respond to the NarratorAgent completing its request and response processing.
	if (!NarratorAgent->OnNarratorResultReady.IsAlreadyBound(this, &UChatGPTManager::HandleNarratorResult))
	{
		NarratorAgent->OnNarratorResultReady.AddDynamic(this, &UChatGPTManager::HandleNarratorResult);
	}

	const FString CurrentWorldStateJSON = WorldStateToJson(WorldState);

	// Send an intro message when the game starts.
	if (bIsInitial)
	{
		NarratorAgent->SendInitialMessage(CurrentWorldStateJSON);
	}
	// Default case: send the player's input with world state differences.
	else
	{
		NarratorAgent->SendMessage(PlayerInput, CurrentWorldStateJSON, RulesResultJson);
	}
}

void UChatGPTManager::HandleNarratorResult(const FString& NarratorResult, const FString& PlayerInput)
{
	// TODO: We shouldn't need to update WorldState here because Narrator should not be changing state.
	
	OnChatGptResponseReceived.Broadcast(NarratorResult, false);
}

FString UChatGPTManager::WorldStateToJson(const FWorldState& State)
{
	FString OutputString;
	FJsonObjectConverter::UStructToJsonObjectString(State, OutputString);
	return OutputString;
}