// Pure functions for the impact and armour model.
#pragma once

#include "CoreMinimal.h"
#include "IronvowTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatLibrary.generated.h"

class UArmorDataAsset;

/**
 * The whole damage model lives here so it can be unit tested and tuned without
 * touching actors. Damage = weapon base damage * energy curve * stamina *
 * hit zone, pushed through each armour layer's coverage.
 */
UCLASS()
class IRONVOW_API UCombatLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Kinetic energy in joules for a cm/s impact. */
	UFUNCTION(BlueprintPure, Category = "Ironvow|Combat")
	static float EnergyFromImpact(float MassKg, float SpeedCmPerSec);

	/** 0.05 .. 3.0 curve on top of the weapon's base damage. */
	UFUNCTION(BlueprintPure, Category = "Ironvow|Combat")
	static float EnergyToDamageScale(float EnergyJoules);

	/** How well an edge type defeats an armour layer (1 = fully effective). */
	UFUNCTION(BlueprintPure, Category = "Ironvow|Combat")
	static float GetLayerEffectiveness(EWeaponEdge Edge, EArmorLayer Layer);

	UFUNCTION(BlueprintPure, Category = "Ironvow|Combat")
	static float GetZoneDamageMultiplier(EHitZone Zone);

	UFUNCTION(BlueprintPure, Category = "Ironvow|Combat")
	static float GetZoneMaxHealth(EHitZone Zone);

	/** Resolve one impact. Random01 is the caller's random roll in [0,1). */
	UFUNCTION(BlueprintCallable, Category = "Ironvow|Combat")
	static FDamageResult EvaluateImpact(const FWeaponImpactData& Impact, const UArmorDataAsset* Armor, EHitZone Zone, float Random01);

	/** Fraction of damage that reaches the body through the zone's armour. */
	UFUNCTION(BlueprintPure, Category = "Ironvow|Combat")
	static float GetArmorPassThrough(const UArmorDataAsset* Armor, EHitZone Zone, EWeaponEdge Edge, EArmorLayer& OutStrongestLayer);

	/** Maps a physics-asset bone name to a hit zone. */
	UFUNCTION(BlueprintPure, Category = "Ironvow|Combat")
	static EHitZone BoneNameToZone(FName BoneName);

	/** Sample bone per zone, used for AI aim points and HUD markers. */
	UFUNCTION(BlueprintPure, Category = "Ironvow|Combat")
	static FName ZoneToSampleBone(EHitZone Zone);

	/** Wears armour down. Mutable copy owned by the damage component. */
	static void ApplyArmorWear(UArmorDataAsset* Armor, EHitZone Zone, float Damage, EWeaponEdge Edge);
};
