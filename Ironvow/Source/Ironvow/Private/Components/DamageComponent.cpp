#include "Components/DamageComponent.h"

#include "Combat/CombatLibrary.h"
#include "Data/ArmorDataAsset.h"
#include "DrawDebugHelpers.h"
#include "UObject/UObjectGlobals.h"

UDamageComponent::UDamageComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UDamageComponent::BeginPlay()
{
	Super::BeginPlay();

	InitializeZones();

	if (ArmorTemplate)
	{
		SetArmorData(ArmorTemplate, true);
	}
}

void UDamageComponent::InitializeZones()
{
	Zones.Reset();
	const int32 ZoneCount = static_cast<int32>(EHitZone::Count);
	Zones.Reserve(ZoneCount);

	for (int32 Index = 0; Index < ZoneCount; ++Index)
	{
		const EHitZone Zone = static_cast<EHitZone>(Index);
		FZoneHealth State;
		State.Zone = Zone;
		State.MaxHealth = UCombatLibrary::GetZoneMaxHealth(Zone) * HealthScale;
		State.Health = State.MaxHealth;
		State.BleedingPerSecond = 0.f;
		State.bDestroyed = false;
		Zones.Add(State);
	}
}

void UDamageComponent::SetArmorData(UArmorDataAsset* NewArmor, bool bDuplicateForRuntime)
{
	if (!NewArmor)
	{
		RuntimeArmor = nullptr;
		return;
	}

	RuntimeArmor = bDuplicateForRuntime
		? DuplicateObject<UArmorDataAsset>(NewArmor, this)
		: NewArmor;
}

FDamageResult UDamageComponent::ApplyWeaponImpact(AActor* Source, const FWeaponImpactData& Impact, FName HitBone)
{
	FDamageResult Result;
	if (bDead)
	{
		return Result;
	}

	const EHitZone Zone = UCombatLibrary::BoneNameToZone(HitBone);
	const float Roll = FMath::FRand();

	Result = UCombatLibrary::EvaluateImpact(Impact, RuntimeArmor, Zone, Roll);

	FZoneHealth* State = FindZoneState(Zone);
	if (!State || State->bDestroyed)
	{
		return Result;
	}

	// Armour wears before the body does.
	UCombatLibrary::ApplyArmorWear(RuntimeArmor, Zone, Result.Damage, Impact.Edge);

	State->Health = FMath::Max(0.f, State->Health - Result.Damage);
	State->BleedingPerSecond = FMath::Min(6.f, State->BleedingPerSecond + Result.BleedingPerSecond * BleedScale);

	const bool bWasDestroyed = State->bDestroyed;
	if (State->Health <= 0.f)
	{
		State->bDestroyed = true;
		Result.bZoneDestroyed = true;
	}

	OnZoneDamaged.Broadcast(Zone, Result.Damage, Result.Result);

	if (Result.bZoneDestroyed && !bWasDestroyed)
	{
		if (Zone == EHitZone::Head)
		{
			// A destroyed head ends the duel immediately.
			ApplyDirectDamage(Source, Zone, State->MaxHealth, false);
		}
		else if (Result.StoppedBy != EArmorLayer::None)
		{
			OnArmorLayerBroken.Broadcast(Zone, Result.StoppedBy);
		}
	}

	if (!bDead && GetTotalHealthRatio() <= 0.f)
	{
		bDead = true;
		OnCombatantDied.Broadcast(Source);
	}

	return Result;
}

void UDamageComponent::ApplyDirectDamage(AActor* Source, EHitZone Zone, float Amount, bool bBleed)
{
	if (bDead || Amount <= 0.f)
	{
		return;
	}

	if (FZoneHealth* State = FindZoneState(Zone))
	{
		State->Health = FMath::Max(0.f, State->Health - Amount);
		if (bBleed)
		{
			State->BleedingPerSecond = FMath::Min(6.f, State->BleedingPerSecond + Amount * 0.05f);
		}

		OnZoneDamaged.Broadcast(Zone, Amount, Amount > 8.f ? EHitResult::Solid : EHitResult::Glance);

		if (Zone == EHitZone::Head && State->Health <= 0.f)
		{
			bDead = true;
			OnCombatantDied.Broadcast(Source);
			return;
		}
	}

	if (!bDead && GetTotalHealthRatio() <= 0.f)
	{
		bDead = true;
		OnCombatantDied.Broadcast(Source);
	}
}

void UDamageComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bDead)
	{
		return;
	}

	float TotalBleed = 0.f;
	for (FZoneHealth& State : Zones)
	{
		if (State.BleedingPerSecond <= 0.f || State.bDestroyed)
		{
			continue;
		}

		const float BleedDamage = State.BleedingPerSecond * DeltaTime * BleedScale;
		State.Health = FMath::Max(0.f, State.Health - BleedDamage);
		TotalBleed += BleedDamage;

		if (State.Health <= 0.f)
		{
			State.bDestroyed = true;
			if (State.Zone == EHitZone::Head)
			{
				bDead = true;
				OnCombatantDied.Broadcast(nullptr);
				return;
			}
		}

		// Wounds clot slowly so a duel does not always end in exsanguination.
		State.BleedingPerSecond = FMath::Max(0.f, State.BleedingPerSecond - 0.06f * DeltaTime);
	}

	if (TotalBleed > 0.f)
	{
		BleedAccumulator += TotalBleed;
		if (BleedAccumulator >= 1.f)
		{
			OnBleedingTick.Broadcast(BleedAccumulator, EHitZone::Torso);
			BleedAccumulator = 0.f;
		}
	}

	if (GetTotalHealthRatio() <= 0.f)
	{
		bDead = true;
		OnCombatantDied.Broadcast(nullptr);
	}
}

void UDamageComponent::ResetCombatant()
{
	bDead = false;
	BleedAccumulator = 0.f;
	InitializeZones();

	if (ArmorTemplate)
	{
		SetArmorData(ArmorTemplate, true);
	}
}

FZoneHealth* UDamageComponent::FindZoneState(EHitZone Zone)
{
	return Zones.FindByPredicate([Zone](const FZoneHealth& Entry)
	{
		return Entry.Zone == Zone;
	});
}

const FZoneHealth* UDamageComponent::FindZoneState(EHitZone Zone) const
{
	return Zones.FindByPredicate([Zone](const FZoneHealth& Entry)
	{
		return Entry.Zone == Zone;
	});
}

float UDamageComponent::GetZoneHealth(EHitZone Zone) const
{
	const FZoneHealth* State = FindZoneState(Zone);
	return State ? State->Health : 0.f;
}

float UDamageComponent::GetZoneHealthRatio(EHitZone Zone) const
{
	const FZoneHealth* State = FindZoneState(Zone);
	if (!State || State->MaxHealth <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}
	return FMath::Clamp(State->Health / State->MaxHealth, 0.f, 1.f);
}

float UDamageComponent::GetTotalHealthRatio() const
{
	float Current = 0.f;
	float Max = 0.f;
	for (const FZoneHealth& State : Zones)
	{
		Current += State.Health;
		Max += State.MaxHealth;
	}
	return Max > KINDA_SMALL_NUMBER ? FMath::Clamp(Current / Max, 0.f, 1.f) : 0.f;
}

bool UDamageComponent::IsZoneDestroyed(EHitZone Zone) const
{
	const FZoneHealth* State = FindZoneState(Zone);
	return State ? State->bDestroyed : true;
}

float UDamageComponent::GetLegIntegrity() const
{
	return (GetZoneHealthRatio(EHitZone::LeftLeg) + GetZoneHealthRatio(EHitZone::RightLeg)) * 0.5f;
}

float UDamageComponent::GetWeakestLegRatio() const
{
	return FMath::Min(GetZoneHealthRatio(EHitZone::LeftLeg), GetZoneHealthRatio(EHitZone::RightLeg));
}

float UDamageComponent::GetBleedRate() const
{
	float Total = 0.f;
	for (const FZoneHealth& State : Zones)
	{
		Total += State.BleedingPerSecond;
	}
	return Total;
}
