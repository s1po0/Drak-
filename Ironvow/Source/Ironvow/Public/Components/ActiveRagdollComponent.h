// Active ragdoll: physical animation motors plus a balance controller.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActiveRagdollComponent.generated.h"

class ACharacter;
class UCharacterMovementComponent;
class UPhysicalAnimationComponent;
class UStaminaComponent;
class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRagdollStateChanged, bool, bIsRagdoll);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRagdollRecovered, float, TimeTaken);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBalanceChanged, float, Balance);

/**
 * Keeps a skeletal mesh standing with physical animation motors and lets it
 * fall when the fighter is beaten. There is no canned knockdown animation:
 * balance is a scalar, and at zero the body becomes a Chaos ragdoll.
 */
UCLASS(ClassGroup = (Ironvow), meta = (BlueprintSpawnableComponent))
class IRONVOW_API UActiveRagdollComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UActiveRagdollComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ---- rig ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ragdoll|Rig")
	FName RootBone = TEXT("pelvis");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ragdoll|Rig")
	FName LeftFootBone = TEXT("foot_l");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ragdoll|Rig")
	FName RightFootBone = TEXT("foot_r");

	/** Physics asset constraint profile used for the motors. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ragdoll|Rig")
	FName PhysicalAnimationProfile = TEXT("Default");

	// ---- motors ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ragdoll|Motors", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float BaseMotorStrength = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ragdoll|Motors", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float StumbleMotorStrength = 0.22f;

	/** Newtons of corrective force at the pelvis to stay upright. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ragdoll|Balance", meta = (ClampMin = "0.0"))
	float UprightForce = 4200.f;

	/** Balance lost per second per metre of lean beyond tolerance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ragdoll|Balance", meta = (ClampMin = "0.0"))
	float BalanceDrainPerMeter = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ragdoll|Balance", meta = (ClampMin = "0.0"))
	float BalanceRecoveryPerSecond = 0.30f;

	/** Lean distance (cm) tolerated before balance starts draining. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ragdoll|Balance", meta = (ClampMin = "1.0"))
	float LeanToleranceCm = 14.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ragdoll|Balance", meta = (ClampMin = "1.0"))
	float LeanMaxDistanceCm = 55.f;

	/** Seconds a stumble lasts before the motors return to full strength. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ragdoll|Balance", meta = (ClampMin = "0.0"))
	float StumbleDuration = 0.85f;

	/** Balance lost per unit of impact severity (see ApplyImpactReaction). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ragdoll|Balance", meta = (ClampMin = "0.0"))
	float ImpactBalanceScale = 0.22f;

	/** Impulse scale applied to the bone on a hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ragdoll|Balance", meta = (ClampMin = "0.0"))
	float ImpactImpulseScale = 380.f;

	// ---- recovery ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ragdoll|Recovery", meta = (ClampMin = "0.1"))
	float RecoverDuration = 1.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ragdoll|Recovery", meta = (ClampMin = "0.0"))
	float RecoverStaminaCost = 28.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ragdoll|Recovery", meta = (ClampMin = "0.0"))
	float MinStaminaToRecover = 12.f;

	/** Legs below this ratio prevent standing up until healed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ragdoll|Recovery", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinLegIntegrityToRecover = 0.2f;

	UPROPERTY(BlueprintAssignable, Category = "Ragdoll")
	FOnRagdollStateChanged OnRagdollStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Ragdoll")
	FOnRagdollRecovered OnRagdollRecovered;

	UPROPERTY(BlueprintAssignable, Category = "Ragdoll")
	FOnBalanceChanged OnBalanceChanged;

	/** Full ragdoll on/off. Recovery is handled by TryRecover. */
	UFUNCTION(BlueprintCallable, Category = "Ragdoll")
	void SetRagdollActive(bool bActive);

	/** A hit landed. Impulse is a world-space impulse in kg*cm/s. */
	UFUNCTION(BlueprintCallable, Category = "Ragdoll")
	void ApplyImpactReaction(const FVector& Impulse, FName HitBone, float Severity);

	/** Attempts to stand back up. Returns false when too tired or too broken. */
	UFUNCTION(BlueprintCallable, Category = "Ragdoll")
	bool TryRecover();

	UFUNCTION(BlueprintCallable, Category = "Ragdoll")
	void SetLegIntegrity(float InLegIntegrity);

	UFUNCTION(BlueprintCallable, Category = "Ragdoll")
	void SetMotorStrengthScale(float Scale);

	UFUNCTION(BlueprintCallable, Category = "Ragdoll")
	void SetStaminaComponent(UStaminaComponent* InStamina);

	UFUNCTION(BlueprintPure, Category = "Ragdoll")
	bool IsRagdolled() const { return bRagdoll; }

	UFUNCTION(BlueprintPure, Category = "Ragdoll")
	bool IsRecovering() const { return bRecovering; }

	UFUNCTION(BlueprintPure, Category = "Ragdoll")
	float GetBalance() const { return Balance; }

	UFUNCTION(BlueprintPure, Category = "Ragdoll")
	float GetLegIntegrity() const { return LegIntegrity; }

	UFUNCTION(BlueprintPure, Category = "Ragdoll")
	float GetRecoveryAlpha() const;

	/** Pelvis location, used to keep the capsule with the body. */
	UFUNCTION(BlueprintPure, Category = "Ragdoll")
	FVector GetPelvisLocation() const;

protected:
	void EnsureComponents();
	void UpdateBalance(float DeltaTime);
	void UpdateRecovery(float DeltaTime);
	void ApplyMotorStrength();
	void SnapCapsuleToBody();

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwnerCharacter = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> Mesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPhysicalAnimationComponent> PhysicalAnimation = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UStaminaComponent> Stamina = nullptr;

	UPROPERTY(VisibleInstanceOnly, Category = "Ragdoll")
	float Balance = 1.f;

	UPROPERTY(VisibleInstanceOnly, Category = "Ragdoll")
	float LegIntegrity = 1.f;

	UPROPERTY(VisibleInstanceOnly, Category = "Ragdoll")
	bool bRagdoll = false;

	UPROPERTY(VisibleInstanceOnly, Category = "Ragdoll")
	bool bRecovering = false;

	float StumbleRemaining = 0.f;
	float RecoverElapsed = 0.f;
	float MotorScale = 1.f;
	bool bRecoveredThisFall = false;
};
