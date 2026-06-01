#include "ChatGPTManager.h"

#include "Enemy.h"
#include "JsonObjectConverter.h"
#include "NarratorAgent.h"
#include "RulesAgent.h"
#include "RulesUpdate.h"
#include "WorldReaction.h"
#include "WorldStateAgent.h"

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
	RulesAgent->OnStatusUpdate.AddDynamic(this, &UChatGPTManager::HandleRulesAgentStatus);

	NarratorAgent = NewObject<UNarratorAgent>(this, UNarratorAgent::StaticClass(), TEXT("NarratorAgent"));
	NarratorAgent->Initialize(TEXT(""));

	WorldStateAgent = NewObject<UWorldStateAgent>(this, UWorldStateAgent::StaticClass(), TEXT("WorldStateAgent"));
	WorldStateAgent->Initialize(TEXT(""));
	WorldStateAgent->OnStatusUpdate.AddDynamic(this, &UChatGPTManager::HandleWorldStateAgentStatus);
	
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
	WorldState.PlayerHeldItems.Add(TEXT("10 coins"));
}

void UChatGPTManager::Deinitialize()
{
	RulesAgent->OnRulesResultReady.RemoveAll(this);
	RulesAgent->OnStatusUpdate.RemoveAll(this);
	
	WorldStateAgent->OnWorldReactionReady.RemoveAll(this);
	WorldStateAgent->OnStatusUpdate.RemoveAll(this);

	NarratorAgent->OnNarratorResultReady.RemoveAll(this);
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

void UChatGPTManager::BroadcastStatus(const FString& Status) const
{
	if (OnPipelineStatusUpdate.IsBound())
	{
		OnPipelineStatusUpdate.Broadcast(Status);
	}
}

void UChatGPTManager::HandleRulesAgentStatus(const FString& Status)
{
	BroadcastStatus(TEXT("RulesAgent: Processing tool calls..."));
}

void UChatGPTManager::HandleWorldStateAgentStatus(const FString& Status)
{
	BroadcastStatus(TEXT("WorldStateAgent: Processing tool calls..."));
}

void UChatGPTManager::ExecuteRulesAgent(const FString& PlayerInput)
{
	if (!IsValid(RulesAgent))
	{
		UE_LOG(LogTemp, Error, TEXT("UChatGPTManager::ExecuteRulesAgent(): Rules agent is invalid."));
		return;
	}

	// Send a status update to the UI
	BroadcastStatus(TEXT("RulesAgent: Evaluating your action..."));

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

	// Add newly acquired items
	for (const FString& Item : RulesWorldStateUpdate.ItemsPickedUp)
	{
		WorldState.PlayerHeldItems.AddUnique(Item);
	}

	// Remove consumed or lost items
	for (const FString& Item : RulesWorldStateUpdate.ItemsRemoved)
	{
		WorldState.PlayerHeldItems.Remove(Item);
	}

	// Update the current room index if we changed rooms.
	if (RulesWorldStateUpdate.StateChanges.CurrentRoomIndex != WorldState.CurrentRoomIndex)
	{
		WorldState.CurrentRoomIndex = RulesWorldStateUpdate.StateChanges.CurrentRoomIndex;
	}

	ExecuteWorldStateAgent(PlayerInput, RulesResultJson);
}

void UChatGPTManager::ExecuteNarratorAgent(const FString& PlayerInput, const FString& RulesResultJson /*=TEXT("N/A")*/, const FString& WorldReactionJson /*=TEXT("N/A")*/)
{
	if (!IsValid(NarratorAgent))
	{
		UE_LOG(LogTemp, Error, TEXT("[UChatGPTManager::ExecuteNarratorAgent()] Narrator agent is invalid."));
		return;
	}
	
	// Send a status update to the UI
	BroadcastStatus(TEXT("NarratorAgent: Writing narration..."));

	// Bind so we can respond to the NarratorAgent completing its request and response processing.
	if (!NarratorAgent->OnNarratorResultReady.IsAlreadyBound(this, &UChatGPTManager::HandleNarratorResult))
	{
		NarratorAgent->OnNarratorResultReady.AddDynamic(this, &UChatGPTManager::HandleNarratorResult);
	}

	// Send the player's input with world state differences.
	const FString CurrentWorldStateJSON = WorldStateToJson(WorldState);
	NarratorAgent->SendMessage(PlayerInput, CurrentWorldStateJSON, RulesResultJson, WorldReactionJson);
}

void UChatGPTManager::ExecuteWorldStateAgent(const FString& PlayerInput, const FString& RulesResultJson)
{
	if (!IsValid(WorldStateAgent))
	{
		UE_LOG(LogTemp, Error, TEXT("UChatGPTManager::ExecuteWorldStateAgent(): WorldStateAgent is invalid."));
		return;
	}

	// Send a status update to the UI
	BroadcastStatus(TEXT("WorldStateAgent: Evaluating NPC reactions..."));

	// Make sure we bind a callback to receive the results
	if (!WorldStateAgent->OnWorldReactionReady.IsAlreadyBound(this, &UChatGPTManager::HandleWorldStateResult))
	{
		WorldStateAgent->OnWorldReactionReady.AddDynamic(this, &UChatGPTManager::HandleWorldStateResult);
	}

	// Send a message to our WorldStateAgent
	const FString WorldStateJson = WorldStateToJson(WorldState);
	WorldStateAgent->SendMessage(PlayerInput, WorldStateJson, RulesResultJson);
}

void UChatGPTManager::HandleNarratorResult(const FString& NarratorResult, const FString& PlayerInput)
{
	OnChatGptResponseReceived.Broadcast(NarratorResult, false);

	// Clear the status indicator now that the full pipeline is done
	BroadcastStatus(TEXT(""));
}

void UChatGPTManager::HandleWorldStateResult(const FWorldReaction& WorldReaction, const FString& RulesResultJson, const FString& WorldReactionJson,
	const FString& PlayerInput)
{
	// Apply NPC status/intent updates to WorldState
	for (const FEnemyReaction& Reaction : WorldReaction.EnemyReactions)
	{
		for (FRoom& Room : WorldState.Rooms)
		{
			if (FEnemy* Enemy = Room.Enemies.FindByPredicate(
				[&](const FEnemy& E){ return E.Name == Reaction.Name; }))
			{
				Enemy->Status      = Reaction.NewStatus;
				Enemy->IntentOrGoal = Reaction.NewIntent;
			}
		}
	}

	ExecuteNarratorAgent(PlayerInput, RulesResultJson, WorldReactionJson);
}

FString UChatGPTManager::WorldStateToJson(const FWorldState& State)
{
	FString OutputString;
	FJsonObjectConverter::UStructToJsonObjectString(State, OutputString);
	return OutputString;
}
