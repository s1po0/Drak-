// C++ base for the main menu.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "IronvowMainMenuWidget.generated.h"

/**
 * Wires the three game modes to level names. Put this widget in a UI-only
 * "MainMenu" level, or show it on BeginPlay before a match starts.
 */
UCLASS()
class IRONVOW_API UIronvowMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Maps")
	FName DuelMapName = TEXT("Arena_Harrowmere");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Maps")
	FName SurvivalMapName = TEXT("Arena_Vellmoor");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Maps")
	FName SandboxMapName = TEXT("Sandbox_Yard");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu|Maps")
	FName MainMenuMapName = TEXT("MainMenu");

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void StartDuel();

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void StartSurvival();

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void StartSandbox();

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void ReturnToMainMenu();

	UFUNCTION(BlueprintCallable, Category = "Menu")
	void QuitGame();

	UFUNCTION(BlueprintImplementableEvent, Category = "Menu")
	void OnMenuReady();
};
