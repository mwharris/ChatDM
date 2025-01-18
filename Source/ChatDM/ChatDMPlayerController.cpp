#include "ChatDMPlayerController.h"

#include "ChatGPTManager.h"

AChatDMPlayerController::AChatDMPlayerController()
{
	ChatGptManager = nullptr;
}

void AChatDMPlayerController::BeginPlay()
{
	Super::BeginPlay();

	ChatGptManager = NewObject<UChatGPTManager>(this, UChatGPTManager::StaticClass(), TEXT("ChatGPTManager"));
	if (!ChatGptManager)
	{
		UE_LOG(LogTemp, Error, TEXT("AChatDMPlayerController::BeginPlay(): Failed to create ChatGPTManager in BeginPlay."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("AChatDMPlayerController::BeginPlay(): ChatGPTManager successfully created and initialized."));

	// Tell listeners the ChatGPTManager is created so they can bind to events
	OnChatGptManagerCreated.Broadcast();

	// Initialize the manager now that listeners are bound
	ChatGptManager->Initialize();
}

void AChatDMPlayerController::SendChatDMRequest(const FString& UserInput, bool bIsTest) const
{
	if (!ChatGptManager)
	{
		UE_LOG(LogTemp, Error, TEXT("AChatDMPlayerController::SendChatDMRequest(): ChatGPTManager is invalid."));
		return;
	}

	if (!bIsTest)
	{
		ChatGptManager->SendChatRequest(UserInput);
	}
	else
	{
		ChatGptManager->SendTestChatRequest(UserInput);
	}
}
