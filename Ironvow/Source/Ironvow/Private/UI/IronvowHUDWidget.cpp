#include "UI/IronvowHUDWidget.h"

#include "Characters/IronvowPlayerCharacter.h"
#include "Components/ActiveRagdollComponent.h"
#include "Components/DamageComponent.h"
#include "Components/StaminaComponent.h"
#include "Components/WeaponControlComponent.h"
#include "Kismet/GameplayStatics.h"

void UIronvowHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	AIronvowPlayerCharacter* Player = GetPlayerDuelist();
	if (!Player)
	{
		return;
	}

	const float Health = Player->GetHealthRatio();
	const float Stamina = Player->GetStaminaComponent() ? Player->GetStaminaComponent()->GetStaminaRatio() : 1.f;
	const float Balance = Player->GetRagdollComponent() ? Player->GetRagdollComponent()->GetBalance() : 1.f;
	const bool bRagdolled = Player->IsRagdolled();
	const bool bGuard = Player->GetWeaponControl() ? Player->GetWeaponControl()->IsGuardRaised() : false;

	OnHUDUpdated(Health, Stamina, Balance, bRagdolled, bGuard, Player->IsLockedOn());

	if (Player->GetDamageComponent())
	{
		OnBleedingChanged(Player->GetDamageComponent()->GetBleedRate());
	}

	if (MessageTimeRemaining > 0.f)
	{
		MessageTimeRemaining = FMath::Max(0.f, MessageTimeRemaining - InDeltaTime);
	}
}

void UIronvowHUDWidget::PushMessage(const FText& Message)
{
	MessageTimeRemaining = MessageDuration;
	OnMessageReceived(Message);
}

AIronvowPlayerCharacter* UIronvowHUDWidget::GetPlayerDuelist() const
{
	return Cast<AIronvowPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
}
