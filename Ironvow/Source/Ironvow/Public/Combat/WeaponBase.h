// A physically simulated weapon held by constraints.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IronvowTypes.h"
#include "WeaponBase.generated.h"

class AIronvowCharacterBase;
class UDamageComponent;
class UPhysicsConstraintComponent;
class UStaticMeshComponent;
class UWeaponDataAsset;

/** What the blade actually struck, used to pick feedback. */
UENUM(BlueprintType)
enum class EWeaponImpactSurface : uint8
{
	Flesh,
	Metal,
	Wood
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWeaponImpact, AActor*, HitActor, FDamageResult, Result, FVector, ImpactPoint);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWeaponParried, AActor*, OtherWielder, float, ImpactSpeed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWeaponDropped);

/**
 * The weapon is never animated. It is a rigid body driven by a constraint in
 * the wielder's hand; the blade's mass, length and balance decide what a swing
 * does. All hits are resolved from real contact.
 */
UCLASS(BlueprintType, Blueprintable)
class IRONVOW_API AWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	AWeaponBase();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UPhysicsConstraintComponent> MainGripConstraint;

	/** Secondary grip used for two hands and for half-swording. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UPhysicsConstraintComponent> OffhandGripConstraint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UWeaponDataAsset> WeaponData;

	/** Used to build a transient data asset when WeaponData is unset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	EWeaponId PresetId = EWeaponId::ArmingSword;

	/** Contact speed (cm/s) below which nothing is registered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	float MinImpactSpeed = 220.f;

	/** Seconds before the same victim can be hit again by this weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	float HitCooldown = 0.22f;

	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnWeaponImpact OnWeaponImpact;

	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnWeaponParried OnWeaponParried;

	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnWeaponDropped OnWeaponDropped;

	/** Builds the runtime data asset if needed and applies mass / centre of mass. */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void InitializeWeapon(UWeaponDataAsset* InData, EWeaponId InPresetId);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void AttachToWielder(AIronvowCharacterBase* InWielder, EWeaponGripMode Mode);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void DetachFromWielder();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void SetGripMode(EWeaponGripMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void DropWeapon(const FVector& Impulse = FVector::ZeroVector);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void AddTipForce(const FVector& Force);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void AddGripForce(const FVector& Force);

	UFUNCTION(BlueprintPure, Category = "Weapon")
	UWeaponDataAsset* GetWeaponData() const;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	AIronvowCharacterBase* GetWielder() const { return Wielder; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsHeld() const { return Wielder != nullptr && bHeld; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	EWeaponGripMode GetGripMode() const { return GripMode; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	FVector GetTipLocation() const;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	FVector GetTipVelocity() const;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	FVector GetBladeAxis() const;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	FVector GetGripLocation() const;

	/** Fastest point on the weapon, cm/s. */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetImpactSpeed() const;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetEffectiveReach() const;

protected:
	UFUNCTION()
	void HandleWeaponHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

	void ConfigureConstraint(UPhysicsConstraintComponent* Constraint, FName WeaponSocket, FName HandBone, float AngularStiffness);
	void UpdateMassProperties();
	void ResolveWeaponCollision(AWeaponBase* OtherWeapon, const FHitResult& Hit, float RelativeSpeed);
	void SpawnImpactFeedback(const FWeaponImpactData& Impact, const FHitResult& Hit, EWeaponImpactSurface Surface);

	UPROPERTY(Transient)
	TObjectPtr<AIronvowCharacterBase> Wielder = nullptr;

	UPROPERTY(Transient)
	bool bHeld = false;

	UPROPERTY(Transient)
	EWeaponGripMode GripMode = EWeaponGripMode::OneHanded;

	TMap<TWeakObjectPtr<AActor>, float> RecentHitTimes;

	UPROPERTY(Transient)
	TObjectPtr<UDamageComponent> CachedWielderDamage = nullptr;

	float NextFeedbackTime = 0.f;
};
