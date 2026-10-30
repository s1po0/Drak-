// Preset stats for the shipped content, usable before any .uasset exists.
#pragma once

#include "CoreMinimal.h"
#include "IronvowTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "IronvowPresets.generated.h"

class UWeaponDataAsset;
class UArmorDataAsset;

/**
 * Builds transient data assets from the tuning tables in
 * IronvowPresets.cpp. Designers can either use these directly (weapons spawned
 * from a preset id) or create real DataAssets in the editor and override them.
 */
UCLASS()
class IRONVOW_API UIronvowPresetLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Ironvow|Presets", meta = (WorldContext = "WorldContextObject"))
	static UWeaponDataAsset* MakeWeaponData(EWeaponId Id, UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Ironvow|Presets", meta = (WorldContext = "WorldContextObject"))
	static UArmorDataAsset* MakeArmorData(EArmorSetId Id, UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Ironvow|Presets")
	static FText GetWeaponDisplayName(EWeaponId Id);

	UFUNCTION(BlueprintPure, Category = "Ironvow|Presets")
	static FText GetArmorDisplayName(EArmorSetId Id);

	UFUNCTION(BlueprintPure, Category = "Ironvow|Presets")
	static FAIDifficulty GetDifficultyPreset(EEnemyDifficulty Level);

	/** Cycles weapon ids for the sandbox spawner. */
	UFUNCTION(BlueprintPure, Category = "Ironvow|Presets")
	static EWeaponId GetNextWeaponId(EWeaponId Current);
};
