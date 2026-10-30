// C++ base for the pause menu.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "IronvowMenuWidget.generated.h"

class AIronvowPlayerController;

UCLASS()
class IRONVOW_API UIronvowMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintImplementableEvent, Category = "Menu")
	void OnMenuOpened();

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void ResumeGame();

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void RestartDuel();

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void QuitGame();

	UFUNCTION(BlueprintPure, Category = "Menu")
	AIronvowPlayerController* GetIronvowController() const;
};
