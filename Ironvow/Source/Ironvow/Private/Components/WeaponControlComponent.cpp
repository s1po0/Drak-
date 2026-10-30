#include "Components/WeaponControlComponent.h"

#include "Combat/WeaponBase.h"
#include "Components/StaminaComponent.h"
#include "Data/WeaponDataAsset.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"

UWeaponControlComponent::UWeaponControlComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UWeaponControlComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		Stamina = Owner->FindComponentByClass<UStaminaComponent>();
	}
}

void UWeaponControlComponent::SetWeapon(AWeaponBase* InWeapon)
{
	Weapon = InWeapon;

	if (Weapon && Weapon->GetWeaponData())
	{
		GripMode = EWeaponGripMode::OneHanded;
		Weapon->SetGripMode(GripMode);
		OnGripModeChanged.Broadcast(GripMode);
	}
}

void UWeaponControlComponent::SetGripMode(EWeaponGripMode NewMode)
{
	if (!Weapon || !Weapon->IsHeld())
	{
		return;
	}

	const UWeaponDataAsset* Data = Weapon->GetWeaponData();
	if (NewMode == EWeaponGripMode::HalfSword && (!Data || !Data->bSupportsHalfSword))
	{
		return;
	}

	if (GripMode == NewMode)
	{
		return;
	}

	GripMode = NewMode;
	Weapon->SetGripMode(NewMode);
	OnGripModeChanged.Broadcast(GripMode);
}

void UWeaponControlComponent::CycleGripMode()
{
	if (!Weapon)
	{
		return;
	}

	const UWeaponDataAsset* Data = Weapon->GetWeaponData();
	const bool bHalfSword = Data && Data->bSupportsHalfSword;

	switch (GripMode)
	{
	case EWeaponGripMode::OneHanded:
		SetGripMode(bHalfSword ? EWeaponGripMode::TwoHanded : EWeaponGripMode::OneHanded);
		break;
	case EWeaponGripMode::TwoHanded:
		SetGripMode(bHalfSword ? EWeaponGripMode::HalfSword : EWeaponGripMode::OneHanded);
		break;
	case EWeaponGripMode::HalfSword:
	default:
		SetGripMode(EWeaponGripMode::OneHanded);
		break;
	}
}

void UWeaponControlComponent::SetGuardRaised(bool bRaised)
{
	if (bGuardRaised == bRaised)
	{
		return;
	}

	bGuardRaised = bRaised;
	OnGuardStateChanged.Broadcast(bGuardRaised);
}

bool UWeaponControlComponent::CommitThrust()
{
	if (!Weapon || !Weapon->IsHeld() || ThrustCooldown > 0.f)
	{
		return false;
	}

	if (Stamina && !Stamina->TryConsume(ThrustStaminaCost))
	{
		return false;
	}

	ThrustRemaining = ThrustDuration;
	ThrustCooldown = ThrustDuration + 0.18f;

	// The braced half-sword thrust is the one place where precision beats power.
	if (ThrustRemaining > 0.f && GripMode == EWeaponGripMode::HalfSword)
	{
		ThrustRemaining *= 1.15f;
	}

	OnThrustCommitted.Broadcast();
	return true;
}

void UWeaponControlComponent::SetAimTarget(const FVector& WorldTarget)
{
	AimTarget = WorldTarget;
}

void UWeaponControlComponent::SetAimFromScreen(APlayerController* PlayerController, const FVector2D& ScreenPosition)
{
	if (!PlayerController)
	{
		return;
	}

	FVector WorldOrigin;
	FVector WorldDirection;
	if (PlayerController->DeprojectScreenPositionToWorld(ScreenPosition.X, ScreenPosition.Y, WorldOrigin, WorldDirection))
	{
		AimTarget = WorldOrigin + WorldDirection * AimProjectionDistance;
	}
}

void UWeaponControlComponent::AimAtActor(AActor* Target, float HeightOffset)
{
	if (!Target)
	{
		return;
	}

	AimTarget = Target->GetActorLocation() + FVector(0.f, 0.f, HeightOffset);
}

void UWeaponControlComponent::SetControlEnabled(bool bEnabled)
{
	bControlEnabled = bEnabled;
}

void UWeaponControlComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (ThrustCooldown > 0.f)
	{
		ThrustCooldown = FMath::Max(0.f, ThrustCooldown - DeltaTime);
	}

	if (!Weapon || !Weapon->IsHeld())
	{
		ThrustRemaining = 0.f;
		return;
	}

	UpdateStaminaDrain(DeltaTime);
	ApplyTipControl(DeltaTime);
}

void UWeaponControlComponent::UpdateStaminaDrain(float DeltaTime)
{
	if (!Stamina || !Weapon)
	{
		return;
	}

	const float TipSpeed = Weapon->GetTipVelocity().Size();

	if (TipSpeed > 350.f)
	{
		Stamina->TryConsume((TipSpeed / 1000.f) * SwingStaminaDrain * DeltaTime);
	}

	if (bGuardRaised)
	{
		Stamina->TryConsume(GuardStaminaDrain * DeltaTime);
	}
}

float UWeaponControlComponent::GetControlPowerScale() const
{
	float Scale = Stamina ? Stamina->GetPowerScale() : 1.f;

	if (bGuardRaised)
	{
		Scale *= 0.9f;
	}

	switch (GripMode)
	{
	case EWeaponGripMode::TwoHanded:
		Scale *= Weapon && Weapon->GetWeaponData() ? Weapon->GetWeaponData()->TwoHandedStrengthScale : 1.2f;
		break;
	case EWeaponGripMode::HalfSword:
		Scale *= HalfSwordForceScale;
		break;
	case EWeaponGripMode::OneHanded:
	default:
		break;
	}

	return FMath::Clamp(Scale, 0.15f, 2.0f);
}

void UWeaponControlComponent::ApplyTipControl(float DeltaTime)
{
	const UWeaponDataAsset* Data = Weapon->GetWeaponData();
	if (!Data)
	{
		return;
	}

	const FVector TipLocation = Weapon->GetTipLocation();
	const FVector TipVelocity = Weapon->GetTipVelocity();

	FVector Target = AimTarget;
	if (bGuardRaised)
	{
		Target.Z += GuardRaiseOffset;
	}

	const float Power = GetControlPowerScale();

	// PD controller: spring towards the aim point, damp the blade's momentum.
	float Stiffness = TipStiffness * (bGuardRaised ? GuardStiffnessScale : 1.f);
	if (GripMode == EWeaponGripMode::HalfSword)
	{
		Stiffness *= HalfSwordStiffnessScale;
	}

	FVector Force = (Target - TipLocation) * Stiffness - TipVelocity * TipDamping;
	Force = Force.GetClampedToMaxSize(Data->TipControlForce * Power);

	if (bControlEnabled)
	{
		Weapon->AddTipForce(Force);
	}

	if (ThrustRemaining > 0.f)
	{
		ThrustRemaining = FMath::Max(0.f, ThrustRemaining - DeltaTime);

		const float ThrustPower = Data->Edge == EWeaponEdge::Pierce ? 1.f : 0.7f;
		const float HalfSwordBonus = GripMode == EWeaponGripMode::HalfSword ? Data->HalfSwordThrustScale : 1.f;

		if (bControlEnabled)
		{
			Weapon->AddTipForce(Weapon->GetBladeAxis() * ThrustForce * Power * ThrustPower * HalfSwordBonus);
		}
	}
}
