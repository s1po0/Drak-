#include "UI/IronvowMainMenuWidget.h"

#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

void UIronvowMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);
	if (Controller)
	{
		Controller->SetInputMode(FInputModeUIOnly());
		Controller->bShowMouseCursor = true;
	}

	OnMenuReady();
}

void UIronvowMainMenuWidget::StartDuel()
{
	UGameplayStatics::OpenLevel(this, DuelMapName);
}

void UIronvowMainMenuWidget::StartSurvival()
{
	UGameplayStatics::OpenLevel(this, SurvivalMapName);
}

void UIronvowMainMenuWidget::StartSandbox()
{
	UGameplayStatics::OpenLevel(this, SandboxMapName);
}

void UIronvowMainMenuWidget::ReturnToMainMenu()
{
	UGameplayStatics::OpenLevel(this, MainMenuMapName);
}

void UIronvowMainMenuWidget::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, UGameplayStatics::GetPlayerController(this, 0), EQuitPreference::Quit, false);
}
