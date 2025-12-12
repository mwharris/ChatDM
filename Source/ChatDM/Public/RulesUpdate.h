#pragma once

#include "CoreMinimal.h"
#include "WorldState.h"
#include "RulesUpdate.generated.h"

// Represents a RulesAgent response.
USTRUCT(BlueprintType)
struct FRulesUpdate
{
	GENERATED_BODY()

	/** Whether the player's action succeeded or failed according to the Rules Agent. */	
	UPROPERTY(meta = (JsonProperty = "success"))
	bool bSuccess;

	/** The reason for why bSuccess is true or false. */
	UPROPERTY(meta = (JsonProperty = "reason"))
	FString Reason;

	/** The list of any new items picked up during the player's action. */
	UPROPERTY(meta = (JsonProperty = "itemsPickedUp"))
	TArray<FString> ItemsPickedUp;

	/** Struct of any changes that need to be made to the World State. */
	UPROPERTY(meta = (JsonProperty = "stateChanges"))
	FWorldStateUpdate StateChanges;
};