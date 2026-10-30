// Shared combatant: body, weapon, stamina, damage and ragdoll.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "IronvowTypes.h"
#include "Combat/WeaponBase.h"
#include "IronvowCharacterBase.generated.h"

class AWeaponBase;
class UActiveRagdollComponent;
class UArmorDataAsset;
class UDamageComponent;
class UPhysicalAnimationComponent;
class UStaminaComponent;
class UWeaponControlComponent;
class UWeaponDataAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnIronvowDeath, AActor*, Killer);

/**
 * Everything a duelist needs. Player and AI both derive from this so the AI
 * drives the exact same physics weapon through the same control component.
 */
UCLASS(Abstract)
class IRONVOW_API AIronvowCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	AIronvowCharacterBase();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ironvow|Components")
	TObjectPtr<UActiveRagdollComponent> Ragdoll;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ironvow|Components")
	TObjectPtr<UStaminaComponent> Stamina;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ironvow|Components")
	TObjectPtr<UDamageComponent> Damage;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ironvow|Components")
	TObjectPtr<UWeaponControlComponent> WeaponControl;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ironvow|Components")
	TObjectPtr<UPhysicalAnimationComponent> PhysicalAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ironvow|Loadout")
	TSubclassOf<AWeaponBase> WeaponClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ironvow|Loadout")
	EWeaponId StartingWeapon = EWeaponId::ArmingSword;

	/** Optional authored data asset; when null a transient preset is used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ironvow|Loadout")
	TObjectPtr<UWeaponDataAsset> StartingWeaponData = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ironvow|Loadout")
	EArmorSetId StartingArmor = EArmorSetId::Homespun;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ironvow|Loadout")
	TObjectPtr<UArmorDataAsset> StartingArmorData = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ironvow|Movement", meta = (ClampMin = "50.0"))
	float WalkSpeed = 320.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ironvow|Movement", meta = (ClampMin = "50.0"))
	float SprintSpeed = 560.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ironvow|Movement", meta = (ClampMin = "0.0"))
	float SprintStaminaPerSecond = 9.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ironvow|Movement", meta = (ClampMin = "0.0"))
	float DodgeStaminaCost = 16.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ironvow|Movement", meta = (ClampMin = "0.0"))
	float DodgeImpulse = 850.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ironvow|Movement", meta = (ClampMin = "0.1"))
	float DodgeCooldown = 0.65f;

	/** Seconds between automatic attempts to stand back up while ragdolled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ironvow|Recovery", meta = (ClampMin = "0.2"))
	float RecoverAttemptInterval = 1.4f;

	UPROPERTY(BlueprintAssignable, Category = "Ironvow")
	FOnIronvowDeath OnDeath;

	/** Re-broadcast of the held weapon's impact so HUD/camera can react. */
	UPROPERTY(BlueprintAssignable, Category = "Ironvow")
	FOnWeaponImpact OnWeaponImpact;

	UFUNCTION(BlueprintCallable, Category = "Ironvow|Loadout")
	void EquipWeapon(EWeaponId WeaponId, UWeaponDataAsset* OverrideData = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Ironvow|Loadout")
	void EquipArmor(EArmorSetId ArmorId, UArmorDataAsset* OverrideData = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Ironvow|Combat")
	void SetGuardRaised(bool bRaised);

	UFUNCTION(BlueprintCallable, Category = "Ironvow|Combat")
	bool PerformThrust();

	UFUNCTION(BlueprintCallable, Category = "Ironvow|Combat")
	void CycleGripMode();

	UFUNCTION(BlueprintCallable, Category = "Ironvow|Combat")
	void SetWeaponAimTarget(const FVector& WorldTarget);

	UFUNCTION(BlueprintCallable, Category = "Ironvow|Movement")
	void StartSprint();

	UFUNCTION(BlueprintCallable, Category = "Ironvow|Movement")
	void StopSprint();

	UFUNCTION(BlueprintCallable, Category = "Ironvow|Movement")
	bool DodgeStep(const FVector& WorldDirection);

	/** Swings the weapon at a target over SwingDuration. Used by AI. */
	UFUNCTION(BlueprintCallable, Category = "Ironvow|AI")
	void AIStartAttack(AActor* Target);

	UFUNCTION(BlueprintCallable, Category = "Ironvow|AI")
	void AIStopAttack();

	UFUNCTION(BlueprintCallable, Category = "Ironvow|AI")
	float GetDistanceToTarget(const AActor* Target) const;

	UFUNCTION(BlueprintPure, Category = "Ironvow|Combat")
	AWeaponBase* GetWeapon() const { return Weapon; }

	UFUNCTION(BlueprintPure, Category = "Ironvow|Components")
	UStaminaComponent* GetStaminaComponent() const { return Stamina; }

	UFUNCTION(BlueprintPure, Category = "Ironvow|Components")
	UWeaponControlComponent* GetWeaponControl() const { return WeaponControl; }

	UFUNCTION(BlueprintPure, Category = "Ironvow|Components")
	UDamageComponent* GetDamageComponent() const { return Damage; }

	UFUNCTION(BlueprintPure, Category = "Ironvow|Components")
	UActiveRagdollComponent* GetRagdollComponent() const { return Ragdoll; }

	UFUNCTION(BlueprintPure, Category = "Ironvow|Combat")
	bool IsAlive() const { return bAlive; }

	UFUNCTION(BlueprintPure, Category = "Ironvow|Combat")
	bool IsRagdolled() const;

	UFUNCTION(BlueprintPure, Category = "Ironvow|Combat")
	float GetReach() const;

	UFUNCTION(BlueprintPure, Category = "Ironvow|Combat")
	float GetHealthRatio() const;

	UFUNCTION(BlueprintPure, Category = "Ironvow|Movement")
	float GetCurrentMaxSpeed() const;

	/** Multiplier applied to walk/sprint speed (used by AI difficulty). */
	UFUNCTION(BlueprintCallable, Category = "Ironvow|Movement")
	void SetSpeedScale(float NewScale);

	UFUNCTION(BlueprintCallable, Category = "Ironvow|Combat")
	void Kill(AActor* Killer);

	/** Restores health, stamina and weapon after a round loss. */
	UFUNCTION(BlueprintCallable, Category = "Ironvow|Combat")
	void ResetCombatantState();

protected:
	UFUNCTION()
	void HandleDeathInternal(AActor* Killer);

	UFUNCTION()
	void HandleWeaponImpactRelay(AActor* HitActor, FDamageResult Result, FVector ImpactPoint);

	UFUNCTION()
	void HandleZoneDamaged(EHitZone Zone, float Amount, EHitResult Result);

	UFUNCTION()
	void HandleRagdollStateChanged(bool bIsRagdoll);

	UFUNCTION()
	void HandleRecovered(float TimeTaken);

	void UpdateRagdollIntegrity();
	void UpdateRecoveryAttempt(float DeltaTime);
	void UpdateSprintDrain(float DeltaTime);
	void RefreshMovementSpeed();

	UPROPERTY(Transient)
	TObjectPtr<AWeaponBase> Weapon = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UArmorDataAsset> Armor = nullptr;

	UPROPERTY(Transient)
	bool bAlive = true;

	UPROPERTY(Transient)
	bool bSprinting = false;

	UPROPERTY(Transient)
	bool bAIAttacking = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ironvow|Movement", meta = (ClampMin = "0.25", ClampMax = "2.0"))
	float SpeedScale = 1.f;

	float DodgeCooldownRemaining = 0.f;
	float RecoverAttemptCooldown = 0.f;
	float AIAttackElapsed = 0.f;
	float AIAttackDuration = 0.85f;
	TWeakObjectPtr<AActor> AITarget;
	FVector AIAttackOrigin = FVector::ZeroVector;
};
