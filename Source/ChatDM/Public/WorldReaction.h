// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WorldReaction.generated.h"

// A single NPC's reaction to what just happened
USTRUCT(BlueprintType)
struct FEnemyReaction
{
	GENERATED_BODY()

	/** Matched against FEnemy::Name */
	UPROPERTY(meta = (JsonProperty = "name"))
	FString Name;

	/** "Attacking", "Fleeing", "Hiding", "Idle" */
	UPROPERTY(meta = (JsonProperty = "newStatus"))
	FString NewStatus;

	/** Replaces FEnemy::IntentOrGoal */
	UPROPERTY(meta = (JsonProperty = "newIntent"))
	FString NewIntent;

	/** Plain English: "The goblin raises its club and charges" */
	UPROPERTY(meta = (JsonProperty = "actionDescription"))
	FString ActionDescription;
};

// Full output of WorldStateAgent
USTRUCT(BlueprintType)
struct FWorldReaction
{
	GENERATED_BODY()

	UPROPERTY(meta = (JsonProperty = "enemyReactions"))
	TArray<FEnemyReaction> EnemyReactions;

	/** Brief summary for the NarratorAgent */
	UPROPERTY(meta = (JsonProperty = "worldReactionSummary"))
	FString WorldReactionSummary;
};
