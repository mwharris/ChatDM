#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ChatDMPlayerController.generated.h"

class UChatGPTManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnChatGptManagerCreated);

UCLASS()
class CHATDM_API AChatDMPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AChatDMPlayerController();

	/* Delegate fired when ChatGPTManager is created */
	UPROPERTY(BlueprintAssignable, Category="ChatDM")
	FOnChatGptManagerCreated OnChatGptManagerCreated;
	
	/* Send a request through the ChatGptManager */
	UFUNCTION(BlueprintCallable, Category = "ChatDM")
	void SendChatDMRequest(const FString& PlayerPrompt, bool bIsInitialRequest) const;
	
	/* Accessor for ChatGptManager */
	UFUNCTION(BlueprintCallable, Category = "ChatDM")
	UChatGPTManager* GetChatGptManager() const { return ChatGptManager; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
private:
	/* ChatGptManager reference */
	UPROPERTY(BlueprintReadOnly, Category="ChatDM", meta = (AllowPrivateAccess = "true"))
	UChatGPTManager* ChatGptManager;
	
};
