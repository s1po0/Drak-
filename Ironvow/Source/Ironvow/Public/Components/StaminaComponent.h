// Stamina pool with exhaustion, armour cost and power scaling.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StaminaComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStaminaChanged, float, Current, float, Max);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStaminaExhausted);

UCLASS(ClassGroup = (Ironvow), meta = (BlueprintSpawnableComponent))
class IRONVOW_API UStaminaComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStaminaComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina", meta = (ClampMin = "1.0"))
	float MaxStamina = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina", meta = (ClampMin = "0.0"))
	float RegenPerSecond = 11.f;

	/** Delay after spending before regeneration resumes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina", meta = (ClampMin = "0.0"))
	float RegenDelay = 0.9f;

	/** Regeneration multiplier while exhausted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float ExhaustedRegenScale = 0.45f;

	/** Set from the equipped armour's StaminaCostMultiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina", meta = (ClampMin = "0.5", ClampMax = "3.0"))
	float ArmorCostMultiplier = 1.f;

	UPROPERTY(BlueprintAssignable, Category = "Stamina")
	FOnStaminaChanged OnStaminaChanged;

	UPROPERTY(BlueprintAssignable, Category = "Stamina")
	FOnStaminaExhausted OnExhausted;

	/** Spends stamina. Returns false when there is not enough. */
	UFUNCTION(BlueprintCallable, Category = "Stamina")
	bool TryConsume(float Amount);

	/** Spends whatever is available and returns how much was actually spent. */
	UFUNCTION(BlueprintCallable, Category = "Stamina")
	float ConsumeUpTo(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Stamina")
	void Restore(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Stamina")
	void SetMaxStamina(float NewMax, bool bRefill = true);

	UFUNCTION(BlueprintCallable, Category = "Stamina")
	void ResetStamina();

	UFUNCTION(BlueprintPure, Category = "Stamina")
	float GetCurrent() const { return Current; }

	UFUNCTION(BlueprintPure, Category = "Stamina")
	float GetMax() const { return MaxStamina; }

	UFUNCTION(BlueprintPure, Category = "Stamina")
	float GetStaminaRatio() const;

	/** 0.35 (spent) .. 1.0 (fresh). Scales swing force and damage. */
	UFUNCTION(BlueprintPure, Category = "Stamina")
	float GetPowerScale() const;

	UFUNCTION(BlueprintPure, Category = "Stamina")
	bool IsExhausted() const { return bExhausted; }

private:
	void BroadcastChange();

	UPROPERTY(VisibleInstanceOnly, Category = "Stamina")
	float Current = 100.f;

	float TimeSinceSpend = 0.f;
	bool bExhausted = false;
};
