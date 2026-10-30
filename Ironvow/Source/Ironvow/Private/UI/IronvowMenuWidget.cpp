#include "UI/IronvowMenuWidget.h"

#include "Game/IronvowPlayerController.h"
#include "Kismet/GameplayStatics.h"

void UIronvowMenuWidget::ResumeGame()
{
	if (AIronvowPlayerController* Controller = GetIronvowController())
	{
		Controller->ResumeGame();
	}
}

void UIronvowMenuWidget::RestartDuel()
{
	if (AIronvowPlayerController* Controller = GetIronvowController())
	{
		Controller->RestartDuel();
	}
}

void UIronvowMenuWidget::QuitGame()
{
	if (AIronvowPlayerController* Controller = GetIronvowController())
	{
		Controller->QuitToDesktop();
	}
}

AIronvowPlayerController* UIronvowMenuWidget::GetIronvowController() const
{
	return Cast<AIronvowPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
}
