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

TArray<TSharedPtr<FJsonObject>> URulesAgent::BuildToolDefinitions()
{
	TArray<TSharedPtr<FJsonObject>> Tools;

	auto MakeTool = [](const FString& Name, const FString& Description, TSharedPtr<FJsonObject> Parameters)
	{
		TSharedPtr<FJsonObject> Function = MakeShareable(new FJsonObject());
		Function->SetStringField(TEXT("name"), Name);
		Function->SetStringField(TEXT("description"), Description);
		Function->SetObjectField(TEXT("parameters"), Parameters);

		TSharedPtr<FJsonObject> Tool = MakeShareable(new FJsonObject());
		Tool->SetStringField(TEXT("type"), TEXT("function"));
		Tool->SetObjectField(TEXT("function"), Function);

		return Tool;
	};

	auto MakeParams = [](TSharedPtr<FJsonObject> Properties, TArray<FString> Required)
	{
		TSharedPtr<FJsonObject> Params = MakeShareable(new FJsonObject());
		Params->SetStringField(TEXT("type"), TEXT("object"));
		Params->SetObjectField(TEXT("properties"), Properties);

		TArray<TSharedPtr<FJsonValue>> ReqArray;
		for (const FString& Req : Required)
		{
			ReqArray.Add(MakeShareable(new FJsonValueString(Req)));
		}
		Params->SetArrayField(TEXT("required"), ReqArray);

		return Params;
	};

	auto MakeStringProp = [](const FString& Description)
	{
		TSharedPtr<FJsonObject> Prop = MakeShareable(new FJsonObject());
		Prop->SetStringField(TEXT("type"), TEXT("string"));
		Prop->SetStringField(TEXT("description"), Description);
		return Prop;
	};

	auto MakeIntProp = [](const FString& Description)
	{
		TSharedPtr<FJsonObject> Prop = MakeShareable(new FJsonObject());
		Prop->SetStringField(TEXT("type"), TEXT("integer"));
		Prop->SetStringField(TEXT("description"), Description);
		return Prop;
	};

	// update_enemy_health
	{
		TSharedPtr<FJsonObject> Props = MakeShareable(new FJsonObject());
		Props->SetObjectField(TEXT("enemy_name"), MakeStringProp(TEXT("The enemy's name, must match WORLDSTATE exactly.")));
		Props->SetObjectField(TEXT("new_health"), MakeIntProp(TEXT("The enemy's new health value.  If 0 or below, the enemy is incapacitated.")));

		Tools.Add(
			MakeTool(
				TEXT("update_enemy_health"),
				TEXT("Update an enemy's health after the player attacks them."),
				MakeParams(Props, {TEXT("enemy_name"), TEXT("new_health")})
			)
		);
	}

	// remove_item_from_room
	{
		TSharedPtr<FJsonObject> Props = MakeShareable(new FJsonObject());
		Props->SetObjectField(TEXT("room_index"), MakeIntProp(TEXT("The index of the room to remove the item from.")));
		Props->SetObjectField(TEXT("item_name"), MakeStringProp(TEXT("The name of the item to remove.")));

		Tools.Add(
			MakeTool(
				TEXT("remove_item_from_room"),
				TEXT("Remove an item from a room when it is picked up or consumed."),
				MakeParams(Props, {TEXT("room_index"), TEXT("item_name")})
			)
		);
	}

	// remove_item_from_player
	{
		TSharedPtr<FJsonObject> Props = MakeShareable(new FJsonObject());
		Props->SetObjectField(TEXT("item_name"), MakeStringProp(TEXT("The name of the item to remove from the player's inventory.")));
		
		Tools.Add(
			MakeTool(
				TEXT("remove_item_from_player"),
				TEXT("Remove an item from the player's inventory when it is used, dropped, or lost."),
				MakeParams(Props, {TEXT("item_name")})
			)
		);
	}

	// add_item_to_player
	{
		TSharedPtr<FJsonObject> Props = MakeShareable(new FJsonObject());
		Props->SetObjectField(TEXT("item_name"), MakeStringProp(TEXT("The name of the item to add to the player's inventory.")));

		Tools.Add(
			MakeTool(
				TEXT("add_item_to_player"),
				TEXT("Add an item to the player's inventory."),
				MakeParams(Props, {TEXT("item_name")})
			)
		);
	}

	// move_player
	{
		TSharedPtr<FJsonObject> Props = MakeShareable(new FJsonObject());
		Props->SetObjectField(TEXT("room_index"), MakeIntProp(TEXT("The index of the room to move the player to.")));

		Tools.Add(
			MakeTool(
				TEXT("move_player"),
				TEXT("Move the player to a different room."),
				MakeParams(Props, {TEXT("room_index")})
			)
		);
	}

	return Tools;
}

void URulesAgent::SendMessage(const FString& PlayerInput, const FString& WorldStateJson)
{
	const FString WrappedMessage = BuildWrappedUserMessage(WorldStateJson, PlayerInput);
	
	// Build a single-turn message (no history needed for rules checking)
	TArray<TSharedPtr<FJsonObject>> Messages;
	Messages.Add(UChatAgent::ChatMessageToJSON(SystemMessage));
	Messages.Add(UChatAgent::ChatMessageToJSON(FChatMessage("user", WrappedMessage)));

	// Build our tool definitions
	const TArray<TSharedPtr<FJsonObject>> ToolDefinitions = BuildToolDefinitions();

	// Use a shared pointer so both callbacks can access and modify the same result across
	// async calls without it needing to be a member variable or going out of scope.
    TSharedPtr<FRulesUpdate> AccumulatedResult = MakeShareable(new FRulesUpdate());
	AccumulatedResult->bSuccess = false;

	// Called once for each tool the model invokes during the loop.
    // Responsible for recording each tool call into AccumulatedResult.
    // Returns a result string sent back to the model so it knows the call succeeded.
    auto HandleToolCall = [AccumulatedResult](const FString& ToolName, const TSharedPtr<FJsonObject>& Args) -> FString
    {
    	// The AI calls this to update player health, name, and status
        if (ToolName == TEXT("update_enemy_health"))
        {
            FEnemyUpdate Update;
            Update.Name = Args->GetStringField(TEXT("enemy_name"));
            Update.Health = Args->GetIntegerField(TEXT("new_health"));
            Update.Status = Update.Health <= 0 ? TEXT("Incapacitated") : TEXT("");

            // Default to room 0 for now
        	// TODO: Update when multiple rooms are added
            if (AccumulatedResult->StateChanges.Rooms.IsEmpty())
            {
            	AccumulatedResult->StateChanges.Rooms.Add(FRoomUpdate());
            }
            AccumulatedResult->StateChanges.Rooms[0].Enemies.Add(Update);
        }
    	// The AI calls this to remove an item from a given room
        else if (ToolName == TEXT("remove_item_from_room"))
        {
            int32 RoomIndex = Args->GetIntegerField(TEXT("room_index"));
            const FString ItemName = Args->GetStringField(TEXT("item_name"));

            // Find the matching room update or create one if this is the first change to it.
            FRoomUpdate* RoomUpdate = AccumulatedResult->StateChanges.Rooms.FindByPredicate(
                [RoomIndex](const FRoomUpdate& R){ return R.RoomIndex == RoomIndex; });
            if (!RoomUpdate)
            {
                FRoomUpdate NewRoom;
                NewRoom.RoomIndex = RoomIndex;
                AccumulatedResult->StateChanges.Rooms.Add(NewRoom);
                RoomUpdate = &AccumulatedResult->StateChanges.Rooms.Last();
            }
            RoomUpdate->Items.Remove(ItemName);
        }
		// The AI calls this to remove an item from the player's inventory
    	else if (ToolName == TEXT("remove_item_from_player"))
        {
        	AccumulatedResult->ItemsRemoved.Add(Args->GetStringField(TEXT("item_name")));
        }
    	// The AI calls this to add an item to the player's inventory
        else if (ToolName == TEXT("add_item_to_player"))
        {
            const FString ItemName = Args->GetStringField(TEXT("item_name"));
            AccumulatedResult->ItemsPickedUp.Add(ItemName);
        }
    	// The AI calls this to transition a player from one room to another
        else if (ToolName == TEXT("move_player"))
        {
            AccumulatedResult->StateChanges.CurrentRoomIndex = Args->GetIntegerField(TEXT("room_index"));
        }

        // Tell the model the tool ran successfully so it can continue reasoning.
        return TEXT("{\"result\": \"success\"}");
    };
	
	// Called once when the AI's finish_reason is "stop".
	// All tool calls have been recorded into AccumulatedResult.
	auto HandleComplete = [this, AccumulatedResult, PlayerInput](const FString& FinalContent)
	{
		// Parse success and reason from the model's final content message
		TSharedPtr<FJsonObject> ResultJson;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FinalContent);
		if (FJsonSerializer::Deserialize(Reader, ResultJson) && ResultJson.IsValid())
		{
			AccumulatedResult->bSuccess = ResultJson->GetBoolField(TEXT("success"));
			AccumulatedResult->Reason   = ResultJson->GetStringField(TEXT("reason"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[RulesAgent] Failed to parse final result: %s"), *FinalContent);
		}
		
		FString OutputJson;
		FJsonObjectConverter::UStructToJsonObjectString(*AccumulatedResult, OutputJson);

		UE_LOG(LogTemp, Log, TEXT("[RulesAgent::SendMessage] Tool use complete. success=%s"), AccumulatedResult->bSuccess ? TEXT("true") : TEXT("false"));

		if (OnRulesResultReady.IsBound())
		{
			OnRulesResultReady.Broadcast(*AccumulatedResult, OutputJson, PlayerInput);
		}
	};

	// Send our message along with our tools definitions and callbacks
	SendMessageWithTools(Messages, ToolDefinitions, HandleToolCall, HandleComplete);
}