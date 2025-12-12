#pragma once

#include "CoreMinimal.h"
#include "Enemy.h"
#include "Room.generated.h"

// Holds metadata for a room in the World State JSON
USTRUCT(BlueprintType)
struct FRoom
{
	GENERATED_BODY()

	UPROPERTY(meta = (JsonProperty = "roomIndex"))
	int32 RoomIndex;
	
	UPROPERTY(meta = (JsonProperty = "name"))
	FString Name;

	UPROPERTY(meta = (JsonProperty = "description"))
	FString Description;

	UPROPERTY(meta = (JsonProperty = "items"))
	TArray<FString> Items;

	UPROPERTY(meta = (JsonProperty = "enemies"))
	TArray<FEnemy> Enemies;

	UPROPERTY(meta = (JsonProperty = "exits"))
	TArray<FString> Exits;
};

// Reduced size struct for rules updates.
USTRUCT(BlueprintType)
struct FRoomUpdate
{
	GENERATED_BODY()

	UPROPERTY(meta = (JsonProperty = "roomIndex"))
	int32 RoomIndex;
	
	UPROPERTY(meta = (JsonProperty = "items"))
	TArray<FString> Items;

	UPROPERTY(meta = (JsonProperty = "enemies"))
	TArray<FEnemyUpdate> Enemies;
};