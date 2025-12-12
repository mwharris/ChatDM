#pragma once

#include "CoreMinimal.h"
#include "Room.h"
#include "WorldState.generated.h"

// Structure used to contain the current World State.
// Serialized into JSON and sent to ChatGPT Rules and Narrator agents.
USTRUCT(BlueprintType)
struct FWorldState
{
	GENERATED_BODY()

	UPROPERTY(meta = (JsonProperty = "rooms"))
	TArray<FRoom> Rooms;

	UPROPERTY(meta = (JsonProperty = "currentRoomIndex"))
	int32 CurrentRoomIndex = 0;

	UPROPERTY(meta = (JsonProperty = "playerHeldItems"))
	TArray<FString> PlayerHeldItems;
};

// Reduced size struct for rules updates.
USTRUCT(BlueprintType)
struct FWorldStateUpdate
{
	GENERATED_BODY()

	UPROPERTY(meta = (JsonProperty = "currentRoomIndex"))
	int32 CurrentRoomIndex = 0;

	UPROPERTY(meta = (JsonProperty = "playerHeldItems"))
	TArray<FString> PlayerHeldItems;

	UPROPERTY(meta = (JsonProperty = "rooms"))
	TArray<FRoomUpdate> Rooms;
};