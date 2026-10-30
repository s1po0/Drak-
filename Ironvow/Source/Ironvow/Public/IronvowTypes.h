// Shared enums and plain data for Ironvow combat.
#pragma once

#include "CoreMinimal.h"
#include "IronvowTypes.generated.h"

class USoundBase;
class UMaterialInterface;
class UStaticMesh;
class USkeletalMesh;
class UWeaponDataAsset;
class UArmorDataAsset;

/** How a weapon transfers energy into a body. */
UENUM(BlueprintType)
enum class EWeaponEdge : uint8
{
	Slash UMETA(DisplayName = "Slash"),
	Pierce UMETA(DisplayName = "Pierce"),
	Blunt UMETA(DisplayName = "Blunt")
};

/** Layered protection. Blades die on plate, blunt weapons do not. */
UENUM(BlueprintType)
enum class EArmorLayer : uint8
{
	None UMETA(DisplayName = "None"),
	Cloth UMETA(DisplayName = "Cloth / Gambeson"),
	Mail UMETA(DisplayName = "Mail"),
	Plate UMETA(DisplayName = "Plate")
};

/** Hit regions tracked separately by the damage component. */
UENUM(BlueprintType)
enum class EHitZone : uint8
{
	Head UMETA(DisplayName = "Head"),
	Torso UMETA(DisplayName = "Torso"),
	LeftArm UMETA(DisplayName = "Left Arm"),
	RightArm UMETA(DisplayName = "Right Arm"),
	LeftLeg UMETA(DisplayName = "Left Leg"),
	RightLeg UMETA(DisplayName = "Right Leg"),
	Count UMETA(Hidden)
};

/** Outcome of a resolved impact. */
UENUM(BlueprintType)
enum class EHitResult : uint8
{
	Glance UMETA(DisplayName = "Glance"),
	Solid UMETA(DisplayName = "Solid Hit"),
	Penetrated UMETA(DisplayName = "Penetrated"),
	Deflected UMETA(DisplayName = "Deflected by Armour")
};

/** How the wielder is holding the weapon. */
UENUM(BlueprintType)
enum class EWeaponGripMode : uint8
{
	OneHanded UMETA(DisplayName = "One Handed"),
	TwoHanded UMETA(DisplayName = "Two Handed"),
	HalfSword UMETA(DisplayName = "Half Sword")
};

/** The six shipped weapons. Order is used by the sandbox spawner. */
UENUM(BlueprintType)
enum class EWeaponId : uint8
{
	ArmingSword,
	Longsword,
	Axe,
	Mace,
	Spear,
	Dagger,
	Count UMETA(Hidden)
};

/** Enemy skill tiers. */
UENUM(BlueprintType)
enum class EEnemyDifficulty : uint8
{
	Novice UMETA(DisplayName = "Novice of the March"),
	Duelist UMETA(DisplayName = "March Duelist"),
	Vowbreaker UMETA(DisplayName = "Vowbreaker")
};

/** Native AI state, mirrored by the behaviour tree blackboard. */
UENUM(BlueprintType)
enum class EAICombatState : uint8
{
	Circling UMETA(DisplayName = "Circling"),
	Closing UMETA(DisplayName = "Closing"),
	Attacking UMETA(DisplayName = "Attacking"),
	Guarding UMETA(DisplayName = "Guarding"),
	Retreating UMETA(DisplayName = "Retreating"),
	Downed UMETA(DisplayName = "Downed")
};

/** The three shipped armour sets. */
UENUM(BlueprintType)
enum class EArmorSetId : uint8
{
	Homespun,
	Byrnie,
	VowPlate,
	Count UMETA(Hidden)
};

/** One layer of protection on one body region. */
USTRUCT(BlueprintType)
struct FArmorLayerSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armour")
	EArmorLayer Layer = EArmorLayer::None;

	/** 0..1 fraction of the region this layer covers. Gaps let thrusts through. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armour", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Coverage = 0.7f;

	/** Relative thickness. Softens the layer's protection curve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armour", meta = (ClampMin = "0.1", ClampMax = "4.0"))
	float Thickness = 1.0f;

	/** Accumulated damage. Collapses to 0 coverage once destroyed. */
	UPROPERTY(BlueprintReadOnly, Category = "Armour")
	float Integrity = 1.0f;
};

/** Armour carried by one hit zone. */
USTRUCT(BlueprintType)
struct FHitZoneArmor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armour")
	EHitZone Zone = EHitZone::Torso;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armour")
	TArray<FArmorLayerSpec> Layers;

	/** Hardness used for edge dulling and parry sparks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armour")
	float Hardness = 1.0f;
};

/** Everything the damage model needs to know about one impact. */
USTRUCT(BlueprintType)
struct FWeaponImpactData
{
	GENERATED_BODY()

	/** Contact speed in cm/s (relative closing speed, not the attacker's speed). */
	UPROPERTY(BlueprintReadWrite, Category = "Impact")
	float SpeedCmPerSec = 0.f;

	/** Weapon mass in kg. */
	UPROPERTY(BlueprintReadWrite, Category = "Impact")
	float MassKg = 1.2f;

	/** Base damage of the weapon at its reference energy. */
	UPROPERTY(BlueprintReadWrite, Category = "Impact")
	float BaseDamage = 22.f;

	UPROPERTY(BlueprintReadWrite, Category = "Impact")
	EWeaponEdge Edge = EWeaponEdge::Slash;

	/** Impact point in world space. */
	UPROPERTY(BlueprintReadWrite, Category = "Impact")
	FVector Location = FVector::ZeroVector;

	/** Normalised travel direction of the weapon at contact. */
	UPROPERTY(BlueprintReadWrite, Category = "Impact")
	FVector Direction = FVector::ForwardVector;

	/** 0 = edge-on, 1 = flat/parallel. Drives glacing hits. */
	UPROPERTY(BlueprintReadWrite, Category = "Impact")
	float Flatness = 0.f;

	/** True when the blade was gripped on the steel for a braced thrust. */
	UPROPERTY(BlueprintReadWrite, Category = "Impact")
	bool bHalfSword = false;

	/** Wielder's stamina power scale at impact (0.35 - 1.0). */
	UPROPERTY(BlueprintReadWrite, Category = "Impact")
	float StaminaScale = 1.f;
};

/** Result of resolving an impact against armour. */
USTRUCT(BlueprintType)
struct FDamageResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	float Damage = 0.f;

	/** Bleeding added, in health points per second. */
	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	float BleedingPerSecond = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	EHitResult Result = EHitResult::Glance;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	EHitZone Zone = EHitZone::Torso;

	/** True when the zone's health reached zero. */
	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	bool bZoneDestroyed = false;

	/** Armour layer that stopped or slowed the blow, if any. */
	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	EArmorLayer StoppedBy = EArmorLayer::None;
};

/** Runtime state of one hit zone. */
USTRUCT(BlueprintType)
struct FZoneHealth
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Health")
	EHitZone Zone = EHitZone::Torso;

	UPROPERTY(BlueprintReadOnly, Category = "Health")
	float Health = 100.f;

	UPROPERTY(BlueprintReadOnly, Category = "Health")
	float MaxHealth = 100.f;

	UPROPERTY(BlueprintReadOnly, Category = "Health")
	float BleedingPerSecond = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Health")
	bool bDestroyed = false;
};

/** Sounds and decals triggered by an impact, kept on the weapon data asset. */
USTRUCT(BlueprintType)
struct FImpactFeedback
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	TSoftObjectPtr<USoundBase> MetalImpactSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	TSoftObjectPtr<USoundBase> FleshImpactSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	TSoftObjectPtr<USoundBase> WoodImpactSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	TSoftObjectPtr<UMaterialInterface> BloodDecalMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	TSoftObjectPtr<UMaterialInterface> WoodChipDecalMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	float DecalSizeCm = 24.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	float DecalLifeSpan = 18.f;
};

/** Difficulty knobs shared by the AI controller and its behaviour trees. */
USTRUCT(BlueprintType)
struct FAIDifficulty
{
	GENERATED_BODY()

	/** Seconds between perceiving a threat and acting on it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float ReactionTime = 0.45f;

	/** Aim error in cm applied to the AI weapon target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float AimErrorCm = 18.f;

	/** 0..1 chance to guard when the player winds up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float BlockChance = 0.5f;

	/** 0..1 how often the AI commits to an attack instead of circling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float Aggression = 0.5f;

	/** Stamina ratio below which the AI disengages. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float RetreatStaminaRatio = 0.25f;

	/** Multiplier on walk / sprint speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float SpeedScale = 1.f;
};
