#pragma once

#include "CoreMinimal.h"
#include "Enemy.generated.h"

// Holds metadata for an enemy that might appear in the dungeon.
USTRUCT(BlueprintType)
struct FEnemy
{
	GENERATED_BODY()

	/** Unique ID (for the room the enemy is in). */
	UPROPERTY(meta = (JsonProperty = "enemyIndex"))
	int32 EnemyIndex;

	/** The enemy's name. */
	UPROPERTY(meta = (JsonProperty = "name"))
	FString Name;

	/** The enemy's current HP. */
	UPROPERTY(meta = (JsonProperty = "health"))
	int32 Health;

	/** Indicator of the enemy's current status: Idle, Hostile, Incapacitated, etc. */
	UPROPERTY(meta = (JsonProperty = "status"))
	FString Status;

	/** Short description of the enemy's intent or goal. */
	UPROPERTY(meta = (JsonProperty = "intentOrGoal"))
	FString IntentOrGoal;
};

// Reduced size struct for rules updates.
USTRUCT(BlueprintType)
struct FEnemyUpdate
{
	GENERATED_BODY()

	UPROPERTY(meta = (JsonProperty = "enemyIndex"))
	int32 EnemyIndex;

	UPROPERTY(meta = (JsonProperty = "name"))
	FString Name;

	UPROPERTY(meta = (JsonProperty = "health"))
	int32 Health;

	UPROPERTY(meta = (JsonProperty = "status"))
	FString Status;
};