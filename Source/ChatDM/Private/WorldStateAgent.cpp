// Fill out your copyright notice in the Description page of Project Settings.

#include "WorldStateAgent.h"

#include "ChatPromptRow.h"
#include "JsonObjectConverter.h"
#include "WorldReaction.h"

void UWorldStateAgent::Initialize(const FString& InPrompt)
{
	UE_LOG(LogTemp, Log, TEXT("UWorldStateAgent::Initialize(): WorldStateAgent initialized."));

	static const FString DataTablePath = TEXT("/Game/Assets/DT_Prompts.DT_Prompts");
	UDataTable* DataTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, *DataTablePath));
	if (!DataTable)
	{
		UE_LOG(LogTemp, Error, TEXT("UWorldStateAgent::Initialize(): Failed to load DataTable at %s"), *DataTablePath);
		return;
	}

	if (!TryLoadPromptRow(DataTable, TEXT("WorldState_SystemMessage"), TEXT("UWorldStateAgent::Initialize"), SystemPrompt))
	{
		return;
	}

	SystemMessage = FChatMessage("system", SystemPrompt);
}

TArray<TSharedPtr<FJsonObject>> UWorldStateAgent::BuildToolDefinitions()
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

	// update_enemy_reaction — called once per enemy that reacts this turn
	{
		TSharedPtr<FJsonObject> Props = MakeShareable(new FJsonObject());
		Props->SetObjectField(TEXT("enemy_name"),        MakeStringProp(TEXT("The enemy's name, must match WORLDSTATE exactly.")));
		Props->SetObjectField(TEXT("new_status"),        MakeStringProp(TEXT("The enemy's new status: Attacking, Fleeing, Hiding, or Idle.")));
		Props->SetObjectField(TEXT("new_intent"),        MakeStringProp(TEXT("Updated intent or goal sentence describing what the enemy plans to do.")));
		Props->SetObjectField(TEXT("action_description"), MakeStringProp(TEXT("One plain-English sentence describing the enemy's immediate action this turn, e.g. 'The goblin raises its club and charges.'")));

		Tools.Add(
			MakeTool(
				TEXT("update_enemy_reaction"),
				TEXT("Record how an enemy reacts to what just happened. Call once per enemy that changes status, intent, or takes an action."),
				MakeParams(Props, {TEXT("enemy_name"), TEXT("new_status"), TEXT("new_intent"), TEXT("action_description")})
			)
		);
	}

	return Tools;
}

void UWorldStateAgent::SendMessage(const FString& PlayerInput, const FString& WorldStateJson, const FString& RulesResultJson)
{
	CachedRulesResultJson = RulesResultJson;

	const FString WrappedMessage = BuildWrappedUserMessage(WorldStateJson, RulesResultJson, PlayerInput);

	TArray<TSharedPtr<FJsonObject>> Messages;
	Messages.Add(UChatAgent::ChatMessageToJSON(SystemMessage));
	Messages.Add(UChatAgent::ChatMessageToJSON(FChatMessage("user", WrappedMessage)));

	const TArray<TSharedPtr<FJsonObject>> ToolDefinitions = BuildToolDefinitions();

	TSharedPtr<FWorldReaction> AccumulatedReaction = MakeShareable(new FWorldReaction());

	auto HandleToolCall = [AccumulatedReaction](const FString& ToolName, const TSharedPtr<FJsonObject>& Args) -> FString
	{
		if (ToolName == TEXT("update_enemy_reaction"))
		{
			FEnemyReaction Reaction;
			Reaction.Name              = Args->GetStringField(TEXT("enemy_name"));
			Reaction.NewStatus         = Args->GetStringField(TEXT("new_status"));
			Reaction.NewIntent         = Args->GetStringField(TEXT("new_intent"));
			Reaction.ActionDescription = Args->GetStringField(TEXT("action_description"));

			// Replace existing entry for this enemy if the model calls the tool twice for the same enemy
			int32 Existing = AccumulatedReaction->EnemyReactions.IndexOfByPredicate(
				[&](const FEnemyReaction& R){ return R.Name == Reaction.Name; });
			if (Existing != INDEX_NONE)
			{
				AccumulatedReaction->EnemyReactions[Existing] = Reaction;
			}
			else
			{
				AccumulatedReaction->EnemyReactions.Add(Reaction);
			}
		}

		return TEXT("{\"result\": \"success\"}");
	};

	auto HandleComplete = [this, AccumulatedReaction, PlayerInput](const FString& FinalContent)
	{
		// Parse worldReactionSummary from the model's final stop message
		TSharedPtr<FJsonObject> ResultJson;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FinalContent);
		if (FJsonSerializer::Deserialize(Reader, ResultJson) && ResultJson.IsValid())
		{
			AccumulatedReaction->WorldReactionSummary = ResultJson->GetStringField(TEXT("worldReactionSummary"));
		}
		else
		{
			// Fall back to treating the raw content as the summary
			AccumulatedReaction->WorldReactionSummary = FinalContent;
			UE_LOG(LogTemp, Warning, TEXT("[WorldStateAgent] Could not parse final JSON; using raw content as summary."));
		}

		FString WorldReactionJson;
		FJsonObjectConverter::UStructToJsonObjectString(*AccumulatedReaction, WorldReactionJson);

		UE_LOG(LogTemp, Log, TEXT("[WorldStateAgent::SendMessage] Tool use complete. Reactions=%d"), AccumulatedReaction->EnemyReactions.Num());

		if (OnWorldReactionReady.IsBound())
		{
			OnWorldReactionReady.Broadcast(*AccumulatedReaction, CachedRulesResultJson, WorldReactionJson, PlayerInput);
		}
	};

	SendMessageWithTools(Messages, ToolDefinitions, HandleToolCall, HandleComplete);
}
