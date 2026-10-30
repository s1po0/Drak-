// HUD, pause menu and sandbox console commands.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "IronvowPlayerController.generated.h"

class UIronvowHUDWidget;
class UIronvowMenuWidget;

UCLASS()
class IRONVOW_API AIronvowPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AIronvowPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UIronvowHUDWidget> HUDWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UIronvowMenuWidget> PauseMenuClass;

	UFUNCTION(BlueprintCallable, Category = "UI")
	void TogglePauseMenu();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowPauseMenu(bool bShow);

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ResumeGame();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void RestartDuel();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void QuitToDesktop();

	UFUNCTION(BlueprintPure, Category = "UI")
	UIronvowHUDWidget* GetHUDWidget() const { return HUDWidget; }

	// ---- sandbox console commands (press ~ in a development build) ----
	UFUNCTION(Exec)
	void IronvowSpawnEnemy(int32 WeaponIndex = 1);

	UFUNCTION(Exec)
	void IronvowGiveWeapon(int32 WeaponIndex = 0);

	UFUNCTION(Exec)
	void IronvowArmor(int32 ArmorIndex = 1);

	UFUNCTION(Exec)
	void IronvowKillAll();

	UFUNCTION(Exec)
	void IronvowSlowMo(float Scale = 0.25f);

protected:
	UPROPERTY(Transient)
	TObjectPtr<UIronvowHUDWidget> HUDWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UIronvowMenuWidget> PauseMenuWidget = nullptr;
};
