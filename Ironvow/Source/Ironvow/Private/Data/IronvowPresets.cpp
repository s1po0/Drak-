#include "Data/IronvowPresets.h"

#include "Data/ArmorDataAsset.h"
#include "Data/WeaponDataAsset.h"

namespace
{
	struct FWeaponPreset
	{
		EWeaponId Id;
		const TCHAR* Name;
		const TCHAR* Lore;
		EWeaponEdge Edge;
		float MassKg;
		float LengthCm;
		float BalanceOffsetCm;
		float BaseDamage;
		float TipControlForce;
		bool bHalfSword;
		int32 GripCount; // 1 or 2 hands
	};

	// The manual is deliberately short: mass, reach and balance do most of the
	// work, exactly as they would in a real duel.
	const FWeaponPreset GWeaponPresets[] =
	{
		{ EWeaponId::ArmingSword, TEXT("Vale Arming Sword"),
			TEXT("A one-handed blade carried by the levy of the Vale. Light enough to pair with a shield, quick enough to find a seam in mail."),
			EWeaponEdge::Slash, 1.15f, 92.f, -9.f, 20.f, 1300.f, false, 1 },

		{ EWeaponId::Longsword, TEXT("Harrowmere Longsword"),
			TEXT("The duelling blade of the March. Long grip, narrow point, and a balance meant for the thrust when the edge fails."),
			EWeaponEdge::Slash, 1.55f, 118.f, -12.f, 24.f, 1550.f, true, 2 },

		{ EWeaponId::Axe, TEXT("Kellwood Bearded Axe"),
			TEXT("A woodcutter's tool promoted to a knight's argument. Short reach, brutal mass behind the beard of the blade."),
			EWeaponEdge::Slash, 1.80f, 86.f, 4.f, 27.f, 1450.f, false, 1 },

		{ EWeaponId::Mace, TEXT("Marrow Mace of Saint Ord"),
			TEXT("Blessed iron on a haft of ash. It does not need to find a gap; it brings the gap with it."),
			EWeaponEdge::Blunt, 2.30f, 74.f, 8.f, 30.f, 1400.f, false, 1 },

		{ EWeaponId::Spear, TEXT("Reedmark Boar Spear"),
			TEXT("Long, patient and cruel. The March's answer to plate: hold the line, wait, and place the point."),
			EWeaponEdge::Pierce, 2.00f, 210.f, 30.f, 24.f, 1600.f, false, 2 },

		{ EWeaponId::Dagger, TEXT("Whisper Dagger"),
			TEXT("The last word in a close duel. It cannot cut through armour, but it does not need to."),
			EWeaponEdge::Pierce, 0.40f, 42.f, -2.f, 12.f, 900.f, false, 1 }
	};

	const FWeaponPreset& FindWeaponPreset(EWeaponId Id)
	{
		for (const FWeaponPreset& Preset : GWeaponPresets)
		{
			if (Preset.Id == Id)
			{
				return Preset;
			}
		}
		return GWeaponPresets[0];
	}

	void AddLayer(FHitZoneArmor& Zone, EArmorLayer Layer, float Coverage, float Thickness)
	{
		FArmorLayerSpec Spec;
		Spec.Layer = Layer;
		Spec.Coverage = Coverage;
		Spec.Thickness = Thickness;
		Spec.Integrity = 1.f;
		Zone.Layers.Add(Spec);
	}

	FHitZoneArmor MakeZone(EHitZone Zone, float Hardness)
	{
		FHitZoneArmor Entry;
		Entry.Zone = Zone;
		Entry.Hardness = Hardness;
		return Entry;
	}
}

FText UIronvowPresetLibrary::GetWeaponDisplayName(EWeaponId Id)
{
	return FText::FromString(FindWeaponPreset(Id).Name);
}

FText UIronvowPresetLibrary::GetArmorDisplayName(EArmorSetId Id)
{
	switch (Id)
	{
	case EArmorSetId::Homespun:
		return FText::FromString(TEXT("Homespun of the March"));
	case EArmorSetId::Byrnie:
		return FText::FromString(TEXT("Riveted Byrnie of Harrowmere"));
	case EArmorSetId::VowPlate:
		return FText::FromString(TEXT("Vow Plate"));
	default:
		return FText::FromString(TEXT("Unknown Harness"));
	}
}

UWeaponDataAsset* UIronvowPresetLibrary::MakeWeaponData(EWeaponId Id, UObject* WorldContextObject)
{
	const FWeaponPreset& Preset = FindWeaponPreset(Id);

	UWeaponDataAsset* Data = NewObject<UWeaponDataAsset>(WorldContextObject ? WorldContextObject : GetTransientPackage());
	Data->PresetId = Preset.Id;
	Data->DisplayName = FText::FromString(Preset.Name);
	Data->LoreNote = FText::FromString(Preset.Lore);
	Data->Edge = Preset.Edge;
	Data->MassKg = Preset.MassKg;
	Data->LengthCm = Preset.LengthCm;
	Data->BalanceOffsetCm = Preset.BalanceOffsetCm;
	Data->BaseDamage = Preset.BaseDamage;
	Data->TipControlForce = Preset.TipControlForce;
	Data->bSupportsHalfSword = Preset.bHalfSword;
	Data->TwoHandedStrengthScale = Preset.GripCount > 1 ? 1.45f : 1.05f;
	Data->HalfSwordThrustScale = Preset.bHalfSword ? 1.6f : 1.f;
	Data->SwingStaminaCost = 4.f + Preset.MassKg * 7.f;
	Data->TipOffset = FVector(Preset.LengthCm * 0.47f, 0.f, 0.f);
	return Data;
}

UArmorDataAsset* UIronvowPresetLibrary::MakeArmorData(EArmorSetId Id, UObject* WorldContextObject)
{
	UArmorDataAsset* Data = NewObject<UArmorDataAsset>(WorldContextObject ? WorldContextObject : GetTransientPackage());
	Data->PresetId = Id;
	Data->DisplayName = GetArmorDisplayName(Id);

	switch (Id)
	{
	case EArmorSetId::Homespun:
	{
		Data->LoreNote = FText::FromString(TEXT("Layered wool and linen, quilted by the households of the March. Cheap, quiet, and better than nothing."));
		Data->WeightKg = 5.f;
		Data->MoveSpeedMultiplier = 0.98f;
		Data->StaminaCostMultiplier = 1.02f;

		FHitZoneArmor Head = MakeZone(EHitZone::Head, 0.2f);
		AddLayer(Head, EArmorLayer::Cloth, 0.6f, 1.0f);
		Data->Zones.Add(Head);

		FHitZoneArmor Torso = MakeZone(EHitZone::Torso, 0.3f);
		AddLayer(Torso, EArmorLayer::Cloth, 0.95f, 1.4f);
		Data->Zones.Add(Torso);

		for (EHitZone Arm : { EHitZone::LeftArm, EHitZone::RightArm })
		{
			FHitZoneArmor Zone = MakeZone(Arm, 0.25f);
			AddLayer(Zone, EArmorLayer::Cloth, 0.75f, 1.1f);
			Data->Zones.Add(Zone);
		}

		for (EHitZone Leg : { EHitZone::LeftLeg, EHitZone::RightLeg })
		{
			FHitZoneArmor Zone = MakeZone(Leg, 0.25f);
			AddLayer(Zone, EArmorLayer::Cloth, 0.7f, 1.1f);
			Data->Zones.Add(Zone);
		}
		break;
	}

	case EArmorSetId::Byrnie:
	{
		Data->LoreNote = FText::FromString(TEXT("Riveted rings over a padded coat, the harness of the March's sergeants. Blades slide, hammers still bruise."));
		Data->WeightKg = 13.f;
		Data->MoveSpeedMultiplier = 0.90f;
		Data->StaminaCostMultiplier = 1.12f;

		FHitZoneArmor Head = MakeZone(EHitZone::Head, 0.6f);
		AddLayer(Head, EArmorLayer::Cloth, 0.8f, 1.0f);
		AddLayer(Head, EArmorLayer::Mail, 0.7f, 1.2f);
		Data->Zones.Add(Head);

		FHitZoneArmor Torso = MakeZone(EHitZone::Torso, 0.8f);
		AddLayer(Torso, EArmorLayer::Cloth, 1.0f, 1.6f);
		AddLayer(Torso, EArmorLayer::Mail, 0.95f, 1.6f);
		Data->Zones.Add(Torso);

		for (EHitZone Arm : { EHitZone::LeftArm, EHitZone::RightArm })
		{
			FHitZoneArmor Zone = MakeZone(Arm, 0.6f);
			AddLayer(Zone, EArmorLayer::Cloth, 0.9f, 1.2f);
			AddLayer(Zone, EArmorLayer::Mail, 0.8f, 1.3f);
			Data->Zones.Add(Zone);
		}

		for (EHitZone Leg : { EHitZone::LeftLeg, EHitZone::RightLeg })
		{
			FHitZoneArmor Zone = MakeZone(Leg, 0.5f);
			AddLayer(Zone, EArmorLayer::Cloth, 0.9f, 1.3f);
			AddLayer(Zone, EArmorLayer::Mail, 0.45f, 1.0f);
			Data->Zones.Add(Zone);
		}
		break;
	}

	case EArmorSetId::VowPlate:
	default:
	{
		Data->LoreNote = FText::FromString(TEXT("Plate of the Vow, fitted to the man and not to the March. It turns edges and makes a target of every seam."));
		Data->WeightKg = 24.f;
		Data->MoveSpeedMultiplier = 0.80f;
		Data->StaminaCostMultiplier = 1.28f;

		FHitZoneArmor Head = MakeZone(EHitZone::Head, 1.0f);
		AddLayer(Head, EArmorLayer::Cloth, 1.0f, 1.2f);
		AddLayer(Head, EArmorLayer::Mail, 0.9f, 1.2f);
		AddLayer(Head, EArmorLayer::Plate, 0.85f, 2.2f);
		Data->Zones.Add(Head);

		FHitZoneArmor Torso = MakeZone(EHitZone::Torso, 1.0f);
		AddLayer(Torso, EArmorLayer::Cloth, 1.0f, 1.8f);
		AddLayer(Torso, EArmorLayer::Mail, 1.0f, 1.8f);
		AddLayer(Torso, EArmorLayer::Plate, 0.95f, 2.6f);
		Data->Zones.Add(Torso);

		for (EHitZone Arm : { EHitZone::LeftArm, EHitZone::RightArm })
		{
			FHitZoneArmor Zone = MakeZone(Arm, 0.9f);
			AddLayer(Zone, EArmorLayer::Cloth, 1.0f, 1.2f);
			AddLayer(Zone, EArmorLayer::Mail, 0.95f, 1.4f);
			AddLayer(Zone, EArmorLayer::Plate, 0.6f, 2.0f);
			Data->Zones.Add(Zone);
		}

		for (EHitZone Leg : { EHitZone::LeftLeg, EHitZone::RightLeg })
		{
			FHitZoneArmor Zone = MakeZone(Leg, 0.85f);
			AddLayer(Zone, EArmorLayer::Cloth, 1.0f, 1.4f);
			AddLayer(Zone, EArmorLayer::Mail, 0.8f, 1.2f);
			AddLayer(Zone, EArmorLayer::Plate, 0.7f, 2.2f);
			Data->Zones.Add(Zone);
		}
		break;
	}
	}

	return Data;
}

FAIDifficulty UIronvowPresetLibrary::GetDifficultyPreset(EEnemyDifficulty Level)
{
	FAIDifficulty Difficulty;
	switch (Level)
	{
	case EEnemyDifficulty::Novice:
		Difficulty.ReactionTime = 0.75f;
		Difficulty.AimErrorCm = 34.f;
		Difficulty.BlockChance = 0.30f;
		Difficulty.Aggression = 0.35f;
		Difficulty.RetreatStaminaRatio = 0.35f;
		Difficulty.SpeedScale = 0.85f;
		break;

	case EEnemyDifficulty::Duelist:
		Difficulty.ReactionTime = 0.42f;
		Difficulty.AimErrorCm = 16.f;
		Difficulty.BlockChance = 0.60f;
		Difficulty.Aggression = 0.60f;
		Difficulty.RetreatStaminaRatio = 0.25f;
		Difficulty.SpeedScale = 1.0f;
		break;

	case EEnemyDifficulty::Vowbreaker:
	default:
		Difficulty.ReactionTime = 0.22f;
		Difficulty.AimErrorCm = 6.f;
		Difficulty.BlockChance = 0.80f;
		Difficulty.Aggression = 0.80f;
		Difficulty.RetreatStaminaRatio = 0.18f;
		Difficulty.SpeedScale = 1.12f;
		break;
	}
	return Difficulty;
}

EWeaponId UIronvowPresetLibrary::GetNextWeaponId(EWeaponId Current)
{
	const int32 Count = static_cast<int32>(EWeaponId::Count);
	const int32 Next = (static_cast<int32>(Current) + 1) % FMath::Max(1, Count);
	return static_cast<EWeaponId>(Next);
}
