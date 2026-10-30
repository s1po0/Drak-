// Mouse / AI driven tip control for a physically simulated weapon.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "IronvowTypes.h"
#include "WeaponControlComponent.generated.h"

class APlayerController;
class AWeaponBase;
class UStaminaComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGripModeChanged, EWeaponGripMode, NewMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGuardStateChanged, bool, bGuardRaised);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnThrustCommitted);

/**
 * Turns an aim point into forces on the weapon tip. Nothing here animates the
 * weapon: it pulls the real body until the blade goes where the wielder wants,
 * and the mass of the weapon decides how fast that can happen.
 */
UCLASS(ClassGroup = (Ironvow), meta = (BlueprintSpawnableComponent))
class IRONVOW_API UWeaponControlComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponControlComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Tip force per cm of aim error. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control", meta = (ClampMin = "0.0"))
	float TipStiffness = 22.f;

	/** Tip force per cm/s of tip velocity (damping keeps the blade from oscillating). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control", meta = (ClampMin = "0.0"))
	float TipDamping = 2.6f;

	/** Extra stiffness while the guard is raised. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control", meta = (ClampMin = "0.0"))
	float GuardStiffnessScale = 1.35f;

	/** Vertical offset (cm) applied to the aim point when guarding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control")
	float GuardRaiseOffset = 34.f;

	/** Forward impulse (N) applied during a thrust. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control", meta = (ClampMin = "0.0"))
	float ThrustForce = 1700.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control", meta = (ClampMin = "0.0"))
	float ThrustStaminaCost = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control", meta = (ClampMin = "0.05"))
	float ThrustDuration = 0.22f;

	/** Stamina per second while the guard is held high. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control", meta = (ClampMin = "0.0"))
	float GuardStaminaDrain = 3.2f;

	/** Stamina per second per 1000 cm/s of tip speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control", meta = (ClampMin = "0.0"))
	float SwingStaminaDrain = 4.5f;

	/** How far in front of the camera the aim point is projected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control", meta = (ClampMin = "20.0"))
	float AimProjectionDistance = 230.f;

	/** Half-sword is slower to swing but far more precise. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control", meta = (ClampMin = "0.1"))
	float HalfSwordStiffnessScale = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control", meta = (ClampMin = "0.1"))
	float HalfSwordForceScale = 0.7f;

	UPROPERTY(BlueprintAssignable, Category = "Control")
	FOnGripModeChanged OnGripModeChanged;

	UPROPERTY(BlueprintAssignable, Category = "Control")
	FOnGuardStateChanged OnGuardStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Control")
	FOnThrustCommitted OnThrustCommitted;

	UFUNCTION(BlueprintCallable, Category = "Control")
	void SetWeapon(AWeaponBase* InWeapon);

	UFUNCTION(BlueprintCallable, Category = "Control")
	void SetGripMode(EWeaponGripMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "Control")
	void CycleGripMode();

	UFUNCTION(BlueprintCallable, Category = "Control")
	void SetGuardRaised(bool bRaised);

	/** Spends stamina and drives the point forward. Returns false if too tired. */
	UFUNCTION(BlueprintCallable, Category = "Control")
	bool CommitThrust();

	UFUNCTION(BlueprintCallable, Category = "Control")
	void SetAimTarget(const FVector& WorldTarget);

	/** Projects a screen position into the world and sets it as aim point. */
	UFUNCTION(BlueprintCallable, Category = "Control")
	void SetAimFromScreen(APlayerController* PlayerController, const FVector2D& ScreenPosition);

	/** Keeps the aim point on the actor's chest, used by the AI. */
	UFUNCTION(BlueprintCallable, Category = "Control")
	void AimAtActor(AActor* Target, float HeightOffset = 20.f);

	UFUNCTION(BlueprintCallable, Category = "Control")
	void SetControlEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Control")
	AWeaponBase* GetWeapon() const { return Weapon; }

	UFUNCTION(BlueprintPure, Category = "Control")
	EWeaponGripMode GetGripMode() const { return GripMode; }

	UFUNCTION(BlueprintPure, Category = "Control")
	bool IsGuardRaised() const { return bGuardRaised; }

	UFUNCTION(BlueprintPure, Category = "Control")
	bool IsThrusting() const { return ThrustRemaining > 0.f; }

	UFUNCTION(BlueprintPure, Category = "Control")
	FVector GetAimTarget() const { return AimTarget; }

	UFUNCTION(BlueprintPure, Category = "Control")
	float GetControlPowerScale() const;

protected:
	void UpdateStaminaDrain(float DeltaTime);
	void ApplyTipControl(float DeltaTime);

	UPROPERTY(Transient)
	TObjectPtr<AWeaponBase> Weapon = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UStaminaComponent> Stamina = nullptr;

	UPROPERTY(Transient)
	FVector AimTarget = FVector::ZeroVector;

	UPROPERTY(Transient)
	EWeaponGripMode GripMode = EWeaponGripMode::OneHanded;

	UPROPERTY(Transient)
	bool bGuardRaised = false;

	UPROPERTY(Transient)
	bool bControlEnabled = true;

	float ThrustRemaining = 0.f;
	float ThrustCooldown = 0.f;
};
