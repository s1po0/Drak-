// C++ base for the in-game HUD. Drop a WBP subclass of this in the world.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "IronvowHUDWidget.generated.h"

class AIronvowPlayerCharacter;

/**
 * Polls the player character every frame and forwards the numbers to Blueprint
 * so the widget art can stay entirely in UMG.
 */
UCLASS()
class IRONVOW_API UIronvowHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
	void OnHUDUpdated(float HealthRatio, float StaminaRatio, float BalanceRatio, bool bRagdolled, bool bGuardRaised, bool bLockedOn);

	UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
	void OnMessageReceived(const FText& Message);

	UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
	void OnBleedingChanged(float BleedPerSecond);

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void PushMessage(const FText& Message);

	UFUNCTION(BlueprintPure, Category = "HUD")
	AIronvowPlayerCharacter* GetPlayerDuelist() const;

	/** Seconds a pushed message stays visible on the HUD. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	float MessageDuration = 3.f;

private:
	float MessageTimeRemaining = 0.f;
};
