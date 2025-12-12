#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ChatPromptRow.generated.h"

USTRUCT(BlueprintType)
struct FChatPromptRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prompt")
	FString PromptText;
};
