// Data-driven description of an armour set.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "IronvowTypes.h"
#include "ArmorDataAsset.generated.h"

class USkeletalMesh;

/**
 * Layered protection per hit zone. Zones without an entry are treated as bare
 * flesh (cloth only). Create assets in /Game/Ironvow/Data/Armour/.
 */
UCLASS(BlueprintType)
class IRONVOW_API UArmorDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = "true"))
	FText LoreNote;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	EArmorSetId PresetId = EArmorSetId::Homespun;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Armour")
	TArray<FHitZoneArmor> Zones;

	/** Total carried mass in kg. Slows movement and drains stamina. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Armour", meta = (ClampMin = "0.0"))
	float WeightKg = 6.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Armour", meta = (ClampMin = "0.4", ClampMax = "1.0"))
	float MoveSpeedMultiplier = 0.95f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Armour", meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float StaminaCostMultiplier = 1.05f;

	/** Optional skeletal mesh swapped onto the character when equipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TSoftObjectPtr<USkeletalMesh> Mesh;

	/** Returns the zone entry, or nullptr when the zone is bare. (C++ only.) */
	const FHitZoneArmor* FindZone(EHitZone Zone) const;
};
