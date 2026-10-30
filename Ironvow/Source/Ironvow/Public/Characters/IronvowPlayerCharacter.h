// Player duelist: over-the-shoulder camera, mouse-driven weapon, slow motion.
#pragma once

#include "CoreMinimal.h"
#include "Characters/IronvowCharacterBase.h"
#include "IronvowPlayerCharacter.generated.h"

class APlayerController;
class UCameraComponent;
class USpringArmComponent;

UCLASS()
class IRONVOW_API AIronvowPlayerCharacter : public AIronvowCharacterBase
{
	GENERATED_BODY()

public:
	AIronvowPlayerCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "0.1", ClampMax = "4.0"))
	float MouseSensitivity = 1.f;

	/** How fast the weapon aim offset returns to the camera centre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "0.0"))
	float AimOffsetDecay = 3.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "5.0"))
	float AimOffsetLimit = 55.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LockOn", meta = (ClampMin = "100.0"))
	float LockOnRange = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LockOn", meta = (ClampMin = "1.0"))
	float LockOnRotationSpeed = 7.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feel", meta = (ClampMin = "0.0"))
	float HeavyHitThreshold = 14.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feel", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float HitStopDilation = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feel", meta = (ClampMin = "0.01"))
	float HitStopDuration = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feel", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float KillSlowMoDilation = 0.28f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feel", meta = (ClampMin = "0.05"))
	float KillSlowMoDuration = 0.85f;

	UFUNCTION(BlueprintCallable, Category = "LockOn")
	void ToggleLockOn();

	UFUNCTION(BlueprintPure, Category = "LockOn")
	bool IsLockedOn() const { return LockOnTarget.IsValid(); }

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void RequestCameraShake(float Intensity, float Duration);

	UFUNCTION(BlueprintCallable, Category = "Feel")
	void RequestSlowMotion(float Dilation, float Duration);

	void MoveForward(float Value);
	void MoveRight(float Value);
	void TurnInput(float Value);
	void LookUpInput(float Value);
	void LeanInput(float Value);

protected:
	void InputSprintPressed();
	void InputSprintReleased();
	void InputGuardPressed();
	void InputGuardReleased();
	void InputThrustPressed();
	void InputGripPressed();
	void InputDodgePressed();
	void InputLockOnPressed();
	void InputRecoverPressed();

	UFUNCTION()
	void HandleLocalWeaponImpact(AActor* HitActor, FDamageResult Result, FVector ImpactPoint);

	void UpdateAimTarget(float DeltaSeconds);
	void UpdateLean(float DeltaSeconds);
	void UpdateCameraShake(float DeltaSeconds);
	void UpdateLockOn(float DeltaSeconds);
	AActor* FindBestLockOnTarget() const;
	APlayerController* GetPlayerControllerSafe() const;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> LockOnTarget;

	UPROPERTY(Transient)
	FVector2D AimOffset = FVector2D::ZeroVector;

	/** Smoothed lean, -1 (left shoulder) .. 1 (right shoulder). */
	UPROPERTY(Transient)
	float LeanAmount = 0.f;

	float LeanTarget = 0.f;
	FRotator ShakeAmplitude = FRotator::ZeroRotator;
	float ShakeRemaining = 0.f;
	float ShakeDuration = 0.f;

	FTimerHandle DilationTimer;
};
