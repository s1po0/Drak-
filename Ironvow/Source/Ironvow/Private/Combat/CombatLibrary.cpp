#include "Combat/CombatLibrary.h"

#include "Data/ArmorDataAsset.h"

namespace
{
	constexpr float GReferenceEnergyJoules = 0.5f * 1.5f * 12.f * 12.f; // committed longsword cut

	float EdgeFactor(EWeaponEdge Edge)
	{
		switch (Edge)
		{
		case EWeaponEdge::Slash:
			return 1.0f;
		case EWeaponEdge::Pierce:
			return 0.92f;
		case EWeaponEdge::Blunt:
		default:
			return 1.15f;
		}
	}
}

float UCombatLibrary::EnergyFromImpact(float MassKg, float SpeedCmPerSec)
{
	const float SpeedMps = FMath::Max(0.f, SpeedCmPerSec) * 0.01f;
	return 0.5f * FMath::Max(0.05f, MassKg) * SpeedMps * SpeedMps;
}

float UCombatLibrary::EnergyToDamageScale(float EnergyJoules)
{
	if (EnergyJoules <= KINDA_SMALL_NUMBER)
	{
		return 0.05f;
	}

	// sqrt keeps heavy but slow blows meaningful without letting fast taps spike.
	const float Scale = FMath::Sqrt(EnergyJoules / GReferenceEnergyJoules);
	return FMath::Clamp(Scale, 0.05f, 3.f);
}

float UCombatLibrary::GetLayerEffectiveness(EWeaponEdge Edge, EArmorLayer Layer)
{
	if (Layer == EArmorLayer::None)
	{
		return 1.f;
	}

	switch (Edge)
	{
	case EWeaponEdge::Slash:
		switch (Layer)
		{
		case EArmorLayer::Cloth: return 0.95f;
		case EArmorLayer::Mail: return 0.35f;
		case EArmorLayer::Plate: return 0.10f;
		default: return 1.f;
		}

	case EWeaponEdge::Pierce:
		switch (Layer)
		{
		case EArmorLayer::Cloth: return 0.90f;
		case EArmorLayer::Mail: return 0.55f;
		case EArmorLayer::Plate: return 0.30f;
		default: return 1.f;
		}

	case EWeaponEdge::Blunt:
	default:
		switch (Layer)
		{
		case EArmorLayer::Cloth: return 0.95f;
		case EArmorLayer::Mail: return 0.85f;
		case EArmorLayer::Plate: return 0.60f;
		default: return 1.f;
		}
	}
}

float UCombatLibrary::GetZoneDamageMultiplier(EHitZone Zone)
{
	switch (Zone)
	{
	case EHitZone::Head: return 2.2f;
	case EHitZone::Torso: return 1.0f;
	case EHitZone::LeftArm:
	case EHitZone::RightArm: return 0.55f;
	case EHitZone::LeftLeg:
	case EHitZone::RightLeg: return 0.75f;
	default: return 1.f;
	}
}

float UCombatLibrary::GetZoneMaxHealth(EHitZone Zone)
{
	switch (Zone)
	{
	case EHitZone::Head: return 60.f;
	case EHitZone::Torso: return 130.f;
	case EHitZone::LeftArm:
	case EHitZone::RightArm: return 65.f;
	case EHitZone::LeftLeg:
	case EHitZone::RightLeg: return 85.f;
	default: return 100.f;
	}
}

float UCombatLibrary::GetArmorPassThrough(const UArmorDataAsset* Armor, EHitZone Zone, EWeaponEdge Edge, EArmorLayer& OutStrongestLayer)
{
	OutStrongestLayer = EArmorLayer::None;
	if (!Armor)
	{
		return 1.f;
	}

	const FHitZoneArmor* ZoneArmor = Armor->FindZone(Zone);
	if (!ZoneArmor)
	{
		return 1.f;
	}

	float Pass = 1.f;
	float StrongestResistance = 0.f;

	for (const FArmorLayerSpec& Layer : ZoneArmor->Layers)
	{
		if (Layer.Integrity <= 0.f || Layer.Coverage <= 0.f)
		{
			continue;
		}

		const float Effectiveness = GetLayerEffectiveness(Edge, Layer.Layer);
		const float ThicknessScale = FMath::Clamp(1.f / FMath::Max(0.35f, Layer.Thickness), 0.5f, 1.6f);
		const float Resistance = FMath::Clamp(Layer.Coverage * Layer.Integrity * ThicknessScale, 0.f, 1.f);
		const float LayerPass = 1.f - Resistance * (1.f - Effectiveness);
		Pass *= FMath::Max(0.02f, LayerPass);

		const float LayerResistance = (1.f - Effectiveness) * Resistance;
		if (LayerResistance > StrongestResistance)
		{
			StrongestResistance = LayerResistance;
			OutStrongestLayer = Layer.Layer;
		}
	}

	// Blunt force crosses the armour: it breaks the body underneath.
	if (Edge == EWeaponEdge::Blunt)
	{
		Pass = FMath::Max(Pass, 0.35f);
	}

	return FMath::Clamp(Pass, 0.02f, 1.f);
}

FDamageResult UCombatLibrary::EvaluateImpact(const FWeaponImpactData& Impact, const UArmorDataAsset* Armor, EHitZone Zone, float Random01)
{
	FDamageResult Result;
	Result.Zone = Zone;

	const float Energy = EnergyFromImpact(Impact.MassKg, Impact.SpeedCmPerSec);
	float PowerScale = EnergyToDamageScale(Energy);
	PowerScale *= EdgeFactor(Impact.Edge);

	// A flat or edge-aligned contact loses most of its cutting power.
	PowerScale *= FMath::Lerp(1.f, 0.30f, FMath::Clamp(Impact.Flatness, 0.f, 1.f));
	PowerScale *= FMath::Clamp(Impact.StaminaScale, 0.35f, 1.15f);

	const float RawDamage = FMath::Max(0.f, Impact.BaseDamage) * PowerScale * GetZoneDamageMultiplier(Zone);

	EArmorLayer StrongestLayer = EArmorLayer::None;
	float Pass = GetArmorPassThrough(Armor, Zone, Impact.Edge, StrongestLayer);
	Result.StoppedBy = StrongestLayer;

	// Thrusts hunt for gaps: a lucky line slides between plate and mail.
	if (Impact.Edge == EWeaponEdge::Pierce && StrongestLayer == EArmorLayer::Plate)
	{
		const float GapChance = Impact.bHalfSword ? 0.45f : 0.22f;
		if (Random01 < GapChance)
		{
			Pass = FMath::Max(Pass, 0.65f);
			Result.StoppedBy = EArmorLayer::None;
		}
	}

	float Damage = RawDamage * Pass;

	if (Damage < 1.5f)
	{
		Result.Result = EHitResult::Glance;
		Damage = FMath::Max(0.f, Damage * 0.5f);
	}
	else if (Pass < 0.25f && Damage < RawDamage * 0.3f)
	{
		Result.Result = EHitResult::Deflected;
	}
	else if (Impact.Edge == EWeaponEdge::Pierce && Pass > 0.5f)
	{
		Result.Result = EHitResult::Penetrated;
	}
	else
	{
		Result.Result = EHitResult::Solid;
	}

	// Fatigued wielders cannot drive the point home.
	if (Impact.StaminaScale < 0.55f && Result.Result == EHitResult::Penetrated)
	{
		Result.Result = EHitResult::Solid;
	}

	Result.Damage = Damage;

	if (Pass > 0.22f)
	{
		float Bleed = 0.f;
		switch (Impact.Edge)
		{
		case EWeaponEdge::Slash: Bleed = 0.60f; break;
		case EWeaponEdge::Pierce: Bleed = 0.34f; break;
		case EWeaponEdge::Blunt:
		default: Bleed = 0.05f; break;
		}

		const float Severity = FMath::Clamp(Damage / 25.f, 0.2f, 3.f);
		Result.BleedingPerSecond = Bleed * Severity;
	}

	return Result;
}

EHitZone UCombatLibrary::BoneNameToZone(FName BoneName)
{
	const FString Name = BoneName.ToString().ToLower();

	if (Name.Contains(TEXT("head")) || Name.Contains(TEXT("neck")))
	{
		return EHitZone::Head;
	}
	if (Name.Contains(TEXT("hand_l")) || Name.Contains(TEXT("lowerarm_l")) || Name.Contains(TEXT("upperarm_l")) || Name.Contains(TEXT("clavicle_l")))
	{
		return EHitZone::LeftArm;
	}
	if (Name.Contains(TEXT("hand_r")) || Name.Contains(TEXT("lowerarm_r")) || Name.Contains(TEXT("upperarm_r")) || Name.Contains(TEXT("clavicle_r")))
	{
		return EHitZone::RightArm;
	}
	if (Name.Contains(TEXT("thigh_l")) || Name.Contains(TEXT("calf_l")) || Name.Contains(TEXT("foot_l")) || Name.Contains(TEXT("ball_l")))
	{
		return EHitZone::LeftLeg;
	}
	if (Name.Contains(TEXT("thigh_r")) || Name.Contains(TEXT("calf_r")) || Name.Contains(TEXT("foot_r")) || Name.Contains(TEXT("ball_r")))
	{
		return EHitZone::RightLeg;
	}
	if (Name.Contains(TEXT("spine")) || Name.Contains(TEXT("pelvis")) || Name.Contains(TEXT("root")))
	{
		return EHitZone::Torso;
	}

	// Unknown bones default to the torso: the safest assumption for damage.
	return EHitZone::Torso;
}

FName UCombatLibrary::ZoneToSampleBone(EHitZone Zone)
{
	switch (Zone)
	{
	case EHitZone::Head: return TEXT("head");
	case EHitZone::LeftArm: return TEXT("lowerarm_l");
	case EHitZone::RightArm: return TEXT("lowerarm_r");
	case EHitZone::LeftLeg: return TEXT("calf_l");
	case EHitZone::RightLeg: return TEXT("calf_r");
	case EHitZone::Torso:
	default: return TEXT("spine_02");
	}
}

void UCombatLibrary::ApplyArmorWear(UArmorDataAsset* Armor, EHitZone Zone, float Damage, EWeaponEdge Edge)
{
	if (!Armor || Damage <= 0.f)
	{
		return;
	}

	FHitZoneArmor* ZoneArmor = Armor->Zones.FindByPredicate([Zone](const FHitZoneArmor& Entry)
	{
		return Entry.Zone == Zone;
	});

	if (!ZoneArmor)
	{
		return;
	}

	const float WearScale = Edge == EWeaponEdge::Blunt ? 2.2f : (Edge == EWeaponEdge::Pierce ? 0.5f : 0.8f);

	for (FArmorLayerSpec& Layer : ZoneArmor->Layers)
	{
		if (Layer.Layer == EArmorLayer::None)
		{
			continue;
		}

		// Plate shrugs off wear, cloth and mail do not.
		const float Toughness = Layer.Layer == EArmorLayer::Plate ? 0.25f : (Layer.Layer == EArmorLayer::Mail ? 1.f : 1.6f);
		const float IntegrityLoss = (Damage / 400.f) * WearScale * Toughness / FMath::Max(0.5f, Layer.Thickness);
		Layer.Integrity = FMath::Clamp(Layer.Integrity - IntegrityLoss, 0.f, 1.f);
	}
}
