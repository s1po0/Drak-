// Data-driven description of a single weapon.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "IronvowTypes.h"
#include "WeaponDataAsset.generated.h"

class UStaticMesh;

/**
 * Everything that makes one weapon feel different from another: mass, length,
 * balance point, grip sockets, edge type and the feedback it produces.
 * Create assets in the editor under /Game/Ironvow/Data/Weapons/.
 */
UCLASS(BlueprintType)
class IRONVOW_API UWeaponDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = "true"))
	FText LoreNote;

	/** Which preset this weapon was tuned from (used by the sandbox spawner). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	EWeaponId PresetId = EWeaponId::ArmingSword;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	EWeaponEdge Edge = EWeaponEdge::Slash;

	/** Total mass in kg. A longsword is ~1.5, a mace ~2.2, a dagger ~0.4. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.1", ClampMax = "12.0"))
	float MassKg = 1.4f;

	/** Tip-to-pommel length in cm, used for reach and torque. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "10.0", ClampMax = "300.0"))
	float LengthCm = 95.f;

	/** Centre of mass relative to the grip: negative = towards the pommel. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "-100.0", ClampMax = "100.0"))
	float BalanceOffsetCm = -10.f;

	/** Damage at the reference impact energy (see UCombatLibrary). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "1.0"))
	float BaseDamage = 22.f;

	/** Force in newtons the grip control may apply at the tip. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "10.0"))
	float TipControlForce = 1400.f;

	/** Extra force available when the weapon is held with two hands. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "1.0"))
	float TwoHandedStrengthScale = 1.45f;

	/** Accuracy multiplier for braced half-sword thrusts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "1.0"))
	float HalfSwordThrustScale = 1.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grip")
	bool bSupportsHalfSword = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grip")
	FName MainGripSocket = TEXT("Grip_Main");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grip")
	FName OffhandGripSocket = TEXT("Grip_Offhand");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grip")
	FName HalfSwordSocket = TEXT("Grip_HalfSword");

	/** Anim/attach bone on the wielder for the main hand. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grip")
	FName MainHandBone = TEXT("hand_r");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grip")
	FName OffHandBone = TEXT("hand_l");

	/** Multiplier applied to the swing stamina cost. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stamina", meta = (ClampMin = "0.1"))
	float SwingStaminaCost = 8.f;

	/** Visual mesh. If unset, the weapon falls back to an engine cube. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Local offset of the blade tip from the weapon origin, along +X. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	FVector TipOffset = FVector(90.f, 0.f, 0.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	FImpactFeedback Feedback;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetReferenceEnergyJoules() const;
};
