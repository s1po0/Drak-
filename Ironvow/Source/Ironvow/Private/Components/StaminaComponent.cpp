#include "Components/StaminaComponent.h"

UStaminaComponent::UStaminaComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UStaminaComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TimeSinceSpend += DeltaTime;

	if (Current >= MaxStamina)
	{
		Current = MaxStamina;
		return;
	}

	if (TimeSinceSpend < RegenDelay)
	{
		return;
	}

	const float RegenScale = bExhausted ? ExhaustedRegenScale : 1.f;
	Current = FMath::Min(MaxStamina, Current + RegenPerSecond * RegenScale * DeltaTime);

	if (bExhausted && Current > MaxStamina * 0.35f)
	{
		bExhausted = false;
	}

	BroadcastChange();
}

bool UStaminaComponent::TryConsume(float Amount)
{
	if (Amount <= 0.f)
	{
		return true;
	}

	const float Cost = Amount * ArmorCostMultiplier;
	if (Current < Cost)
	{
		return false;
	}

	Current = FMath::Max(0.f, Current - Cost);
	TimeSinceSpend = 0.f;

	if (Current <= KINDA_SMALL_NUMBER && !bExhausted)
	{
		bExhausted = true;
		OnExhausted.Broadcast();
	}

	BroadcastChange();
	return true;
}

float UStaminaComponent::ConsumeUpTo(float Amount)
{
	const float Cost = FMath::Max(0.f, Amount) * ArmorCostMultiplier;
	const float Spent = FMath::Min(Current, Cost);
	Current -= Spent;
	TimeSinceSpend = 0.f;

	if (Current <= KINDA_SMALL_NUMBER)
	{
		bExhausted = true;
	}

	BroadcastChange();
	return Spent;
}

void UStaminaComponent::Restore(float Amount)
{
	if (Amount <= 0.f)
	{
		return;
	}

	Current = FMath::Min(MaxStamina, Current + Amount);
	BroadcastChange();
}

void UStaminaComponent::SetMaxStamina(float NewMax, bool bRefill)
{
	const float Ratio = GetStaminaRatio();
	MaxStamina = FMath::Max(1.f, NewMax);
	Current = bRefill ? MaxStamina : MaxStamina * Ratio;
	BroadcastChange();
}

void UStaminaComponent::ResetStamina()
{
	Current = MaxStamina;
	TimeSinceSpend = RegenDelay;
	bExhausted = false;
	BroadcastChange();
}

float UStaminaComponent::GetStaminaRatio() const
{
	return MaxStamina > KINDA_SMALL_NUMBER ? FMath::Clamp(Current / MaxStamina, 0.f, 1.f) : 0.f;
}

float UStaminaComponent::GetPowerScale() const
{
	// Even a spent fighter can still flail, just not well.
	const float Ratio = GetStaminaRatio();
	return FMath::Clamp(0.35f + 0.65f * Ratio, 0.35f, 1.f);
}

void UStaminaComponent::BroadcastChange()
{
	OnStaminaChanged.Broadcast(Current, MaxStamina);
}
