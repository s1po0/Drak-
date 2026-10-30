// Per-zone health, armour layers and bleeding.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "IronvowTypes.h"
#include "DamageComponent.generated.h"

class UArmorDataAsset;
class UActiveRagdollComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnZoneDamaged, EHitZone, Zone, float, Damage, EHitResult, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBleedingTick, float, Amount, EHitZone, Zone);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatantDied, AActor*, Killer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnArmorLayerBroken, EHitZone, Zone, EArmorLayer, Layer);

/**
 * Owns the body. Every weapon impact is resolved here through UCombatLibrary,
 * applied to the right zone, and reported to the character, HUD, AI and ragdoll.
 */
UCLASS(ClassGroup = (Ironvow), meta = (BlueprintSpawnableComponent))
class IRONVOW_API UDamageComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDamageComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Armour template. Duplicated at BeginPlay so wear is per combatant. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	UArmorDataAsset* ArmorTemplate = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage", meta = (ClampMin = "0.25", ClampMax = "3.0"))
	float HealthScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float BleedScale = 1.f;

	/** Below this fraction of total health the combatant is visibly wounded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WoundedRatio = 0.35f;

	UPROPERTY(BlueprintAssignable, Category = "Damage")
	FOnZoneDamaged OnZoneDamaged;

	UPROPERTY(BlueprintAssignable, Category = "Damage")
	FOnBleedingTick OnBleedingTick;

	UPROPERTY(BlueprintAssignable, Category = "Damage")
	FOnCombatantDied OnCombatantDied;

	UPROPERTY(BlueprintAssignable, Category = "Damage")
	FOnArmorLayerBroken OnArmorLayerBroken;

	/** Resolves and applies one weapon impact. Returns the resolved numbers. */
	UFUNCTION(BlueprintCallable, Category = "Damage")
	FDamageResult ApplyWeaponImpact(AActor* Source, const FWeaponImpactData& Impact, FName HitBone);

	/** Direct damage without a weapon (bleeding, environment, fall). */
	UFUNCTION(BlueprintCallable, Category = "Damage")
	void ApplyDirectDamage(AActor* Source, EHitZone Zone, float Amount, bool bBleed = false);

	UFUNCTION(BlueprintCallable, Category = "Damage")
	void SetArmorData(UArmorDataAsset* NewArmor, bool bDuplicateForRuntime = true);

	UFUNCTION(BlueprintCallable, Category = "Damage")
	void ResetCombatant();

	UFUNCTION(BlueprintPure, Category = "Damage")
	UArmorDataAsset* GetRuntimeArmor() const { return RuntimeArmor; }

	UFUNCTION(BlueprintPure, Category = "Damage")
	float GetZoneHealth(EHitZone Zone) const;

	UFUNCTION(BlueprintPure, Category = "Damage")
	float GetZoneHealthRatio(EHitZone Zone) const;

	UFUNCTION(BlueprintPure, Category = "Damage")
	float GetTotalHealthRatio() const;

	UFUNCTION(BlueprintPure, Category = "Damage")
	bool IsDead() const { return bDead; }

	UFUNCTION(BlueprintPure, Category = "Damage")
	bool IsZoneDestroyed(EHitZone Zone) const;

	/** 0..1 average integrity of both legs. Drives ragdoll balance. */
	UFUNCTION(BlueprintPure, Category = "Damage")
	float GetLegIntegrity() const;

	/** 0..1 ratio of the weaker leg, used for stumble triggers. */
	UFUNCTION(BlueprintPure, Category = "Damage")
	float GetWeakestLegRatio() const;

	UFUNCTION(BlueprintPure, Category = "Damage")
	float GetBleedRate() const;

	UFUNCTION(BlueprintPure, Category = "Damage")
	const TArray<FZoneHealth>& GetZones() const { return Zones; }

protected:
	void InitializeZones();
	FZoneHealth* FindZoneState(EHitZone Zone);
	const FZoneHealth* FindZoneState(EHitZone Zone) const;

	UPROPERTY(VisibleInstanceOnly, Category = "Damage")
	TArray<FZoneHealth> Zones;

	UPROPERTY(Transient)
	TObjectPtr<UArmorDataAsset> RuntimeArmor = nullptr;

	bool bDead = false;
	float BleedAccumulator = 0.f;
};
