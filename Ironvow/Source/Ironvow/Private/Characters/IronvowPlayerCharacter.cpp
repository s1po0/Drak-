#include "Characters/IronvowPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Combat/WeaponBase.h"
#include "Components/ActiveRagdollComponent.h"
#include "Components/DamageComponent.h"
#include "Components/StaminaComponent.h"
#include "Components/WeaponControlComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AIronvowPlayerCharacter::AIronvowPlayerCharacter()
{
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 240.f;
	CameraBoom->SocketOffset = FVector(0.f, 55.f, 70.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = true;
	CameraBoom->CameraLagSpeed = 12.f;
	CameraBoom->bEnableCameraLag = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->FieldOfView = 90.f;
}

void AIronvowPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (OnWeaponImpact.IsBound() == false)
	{
		OnWeaponImpact.AddDynamic(this, &AIronvowPlayerCharacter::HandleLocalWeaponImpact);
	}

	if (APlayerController* PC = GetPlayerControllerSafe())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
}

void AIronvowPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &AIronvowPlayerCharacter::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &AIronvowPlayerCharacter::MoveRight);
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &AIronvowPlayerCharacter::TurnInput);
	PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &AIronvowPlayerCharacter::LookUpInput);
	PlayerInputComponent->BindAxis(TEXT("Lean"), this, &AIronvowPlayerCharacter::LeanInput);

	PlayerInputComponent->BindAction(TEXT("Sprint"), IE_Pressed, this, &AIronvowPlayerCharacter::InputSprintPressed);
	PlayerInputComponent->BindAction(TEXT("Sprint"), IE_Released, this, &AIronvowPlayerCharacter::InputSprintReleased);
	PlayerInputComponent->BindAction(TEXT("Guard"), IE_Pressed, this, &AIronvowPlayerCharacter::InputGuardPressed);
	PlayerInputComponent->BindAction(TEXT("Guard"), IE_Released, this, &AIronvowPlayerCharacter::InputGuardReleased);
	PlayerInputComponent->BindAction(TEXT("Thrust"), IE_Pressed, this, &AIronvowPlayerCharacter::InputThrustPressed);
	PlayerInputComponent->BindAction(TEXT("GripMode"), IE_Pressed, this, &AIronvowPlayerCharacter::InputGripPressed);
	PlayerInputComponent->BindAction(TEXT("Dodge"), IE_Pressed, this, &AIronvowPlayerCharacter::InputDodgePressed);
	PlayerInputComponent->BindAction(TEXT("LockOn"), IE_Pressed, this, &AIronvowPlayerCharacter::InputLockOnPressed);
	PlayerInputComponent->BindAction(TEXT("Recover"), IE_Pressed, this, &AIronvowPlayerCharacter::InputRecoverPressed);
}

void AIronvowPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IsAlive())
	{
		UpdateAimTarget(DeltaSeconds);
		UpdateLockOn(DeltaSeconds);
	}

	UpdateCameraShake(DeltaSeconds);
	UpdateLean(DeltaSeconds);
}

void AIronvowPlayerCharacter::UpdateLean(float DeltaSeconds)
{
	LeanAmount = FMath::FInterpTo(LeanAmount, LeanTarget, DeltaSeconds, 8.f);

	if (CameraBoom)
	{
		CameraBoom->SetRelativeRotation(FRotator(0.f, 0.f, LeanAmount * 11.f));
	}
}

void AIronvowPlayerCharacter::LeanInput(float Value)
{
	LeanTarget = FMath::Clamp(Value, -1.f, 1.f);
}

void AIronvowPlayerCharacter::MoveForward(float Value)
{
	if (!IsAlive() || IsRagdolled() || FMath::IsNearlyZero(Value))
	{
		return;
	}

	AddMovementInput(GetActorForwardVector(), Value);
}

void AIronvowPlayerCharacter::MoveRight(float Value)
{
	if (!IsAlive() || IsRagdolled() || FMath::IsNearlyZero(Value))
	{
		return;
	}

	AddMovementInput(GetActorRightVector(), Value);
}

void AIronvowPlayerCharacter::TurnInput(float Value)
{
	if (FMath::IsNearlyZero(Value))
	{
		return;
	}

	AddControllerYawInput(Value * MouseSensitivity);
	AimOffset.X = FMath::Clamp(AimOffset.X + Value * 0.65f, -AimOffsetLimit, AimOffsetLimit);
}

void AIronvowPlayerCharacter::LookUpInput(float Value)
{
	if (FMath::IsNearlyZero(Value))
	{
		return;
	}

	AddControllerPitchInput(Value * MouseSensitivity);
	AimOffset.Y = FMath::Clamp(AimOffset.Y + Value * 0.65f, -AimOffsetLimit, AimOffsetLimit);
}

void AIronvowPlayerCharacter::UpdateAimTarget(float DeltaSeconds)
{
	if (!WeaponControl)
	{
		return;
	}

	// The weapon tip follows a point in front of the camera, pushed by the mouse.
	AimOffset = FMath::Vector2DInterpTo(AimOffset, FVector2D::ZeroVector, DeltaSeconds, AimOffsetDecay);

	FVector ViewLocation;
	FRotator ViewRotation;
	if (APlayerController* PC = GetPlayerControllerSafe())
	{
		PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	else
	{
		ViewLocation = FollowCamera->GetComponentLocation();
		ViewRotation = FollowCamera->GetComponentRotation();
	}

	const FVector Forward = ViewRotation.Vector();
	const FVector Right = FRotationMatrix(ViewRotation).GetUnitAxis(EAxis::Y);
	const FVector Up = FRotationMatrix(ViewRotation).GetUnitAxis(EAxis::Z);

	FVector Target = ViewLocation
		+ Forward * 230.f
		+ Right * AimOffset.X * 4.f
		+ Up * AimOffset.Y * 4.f
		+ Right * LeanAmount * 40.f;

	if (LockOnTarget.IsValid())
	{
		Target = LockOnTarget->GetActorLocation() + FVector(0.f, 0.f, 20.f);
	}

	WeaponControl->SetAimTarget(Target);
}

void AIronvowPlayerCharacter::UpdateLockOn(float DeltaSeconds)
{
	if (!LockOnTarget.IsValid())
	{
		return;
	}

	AIronvowCharacterBase* TargetCharacter = Cast<AIronvowCharacterBase>(LockOnTarget.Get());
	if (TargetCharacter && !TargetCharacter->IsAlive())
	{
		ToggleLockOn();
		return;
	}

	AController* MyController = GetController();
	if (!MyController)
	{
		return;
	}

	const FVector TargetLocation = LockOnTarget->GetActorLocation() + FVector(0.f, 0.f, 20.f);
	const FRotator Desired = (TargetLocation - GetActorLocation()).Rotation();
	const FRotator Current = MyController->GetControlRotation();
	const FRotator Blended = FMath::RInterpTo(Current, FRotator(Desired.Pitch * 0.4f, Desired.Yaw, 0.f), DeltaSeconds, LockOnRotationSpeed);

	MyController->SetControlRotation(Blended);
}

AActor* AIronvowPlayerCharacter::FindBestLockOnTarget() const
{
	AActor* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();

	const FVector Origin = GetActorLocation();
	const FVector Forward = GetActorForwardVector();

	for (TActorIterator<AIronvowCharacterBase> It(GetWorld()); It; ++It)
	{
		AIronvowCharacterBase* Candidate = *It;
		if (!Candidate || Candidate == this || !Candidate->IsAlive())
		{
			continue;
		}

		const FVector ToCandidate = Candidate->GetActorLocation() - Origin;
		const float Distance = ToCandidate.Size();
		if (Distance > LockOnRange)
		{
			continue;
		}

		const float Facing = FVector::DotProduct(Forward, ToCandidate.GetSafeNormal());
		if (Facing < 0.15f)
		{
			continue;
		}

		// Prefer targets in front and close.
		const float Score = Distance * (2.f - Facing);
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Candidate;
		}
	}

	return Best;
}

void AIronvowPlayerCharacter::ToggleLockOn()
{
	if (LockOnTarget.IsValid())
	{
		LockOnTarget = nullptr;
		return;
	}

	if (AActor* Target = FindBestLockOnTarget())
	{
		LockOnTarget = Target;
	}
}

void AIronvowPlayerCharacter::RequestCameraShake(float Intensity, float Duration)
{
	ShakeAmplitude = FRotator(Intensity * 0.6f, Intensity, Intensity * 0.8f);
	ShakeDuration = FMath::Max(0.05f, Duration);
	ShakeRemaining = ShakeDuration;
}

void AIronvowPlayerCharacter::UpdateCameraShake(float DeltaSeconds)
{
	if (ShakeRemaining <= 0.f || !CameraBoom)
	{
		return;
	}

	ShakeRemaining = FMath::Max(0.f, ShakeRemaining - DeltaSeconds);
	const float Alpha = ShakeDuration > 0.f ? ShakeRemaining / ShakeDuration : 0.f;
	const float Intensity = Alpha * Alpha;

	const FVector Noise(
		FMath::PerlinNoise1D(GetWorld()->GetTimeSeconds() * 22.f + 1.7f),
		FMath::PerlinNoise1D(GetWorld()->GetTimeSeconds() * 19.f + 5.3f),
		FMath::PerlinNoise1D(GetWorld()->GetTimeSeconds() * 17.f + 9.1f));

	CameraBoom->AddRelativeRotation(FRotator(
		ShakeAmplitude.Pitch * Intensity * Noise.Y,
		ShakeAmplitude.Yaw * Intensity * Noise.Z,
		ShakeAmplitude.Roll * Intensity * Noise.X));
}

void AIronvowPlayerCharacter::RequestSlowMotion(float Dilation, float Duration)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(DilationTimer);
	UGameplayStatics::SetGlobalTimeDilation(this, FMath::Clamp(Dilation, 0.05f, 1.f));

	FTimerDelegate ResetDelegate;
	ResetDelegate.BindWeakLambda(this, [this]()
	{
		UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
	});

	World->GetTimerManager().SetTimer(DilationTimer, ResetDelegate, FMath::Max(0.01f, Duration), false);
}

void AIronvowPlayerCharacter::HandleLocalWeaponImpact(AActor* HitActor, FDamageResult Result, FVector ImpactPoint)
{
	const float Intensity = FMath::GetMappedRangeValueClamped(FVector2D(0.f, 40.f), FVector2D(0.35f, 2.2f), Result.Damage);
	RequestCameraShake(Intensity, 0.28f);

	if (Result.Damage >= HeavyHitThreshold)
	{
		RequestSlowMotion(HitStopDilation, HitStopDuration);
	}

	if (AIronvowCharacterBase* Victim = Cast<AIronvowCharacterBase>(HitActor))
	{
		if (!Victim->IsAlive())
		{
			RequestSlowMotion(KillSlowMoDilation, KillSlowMoDuration);
		}
	}
}

void AIronvowPlayerCharacter::InputSprintPressed()
{
	StartSprint();
}

void AIronvowPlayerCharacter::InputSprintReleased()
{
	StopSprint();
}

void AIronvowPlayerCharacter::InputGuardPressed()
{
	SetGuardRaised(true);
}

void AIronvowPlayerCharacter::InputGuardReleased()
{
	SetGuardRaised(false);
}

void AIronvowPlayerCharacter::InputThrustPressed()
{
	PerformThrust();
}

void AIronvowPlayerCharacter::InputGripPressed()
{
	CycleGripMode();
}

void AIronvowPlayerCharacter::InputDodgePressed()
{
	if (!IsAlive() || IsRagdolled())
	{
		return;
	}

	FVector Direction = GetLastMovementInputVector();
	if (Direction.IsNearlyZero())
	{
		Direction = -GetActorForwardVector();
	}

	DodgeStep(Direction.GetSafeNormal());
}

void AIronvowPlayerCharacter::InputLockOnPressed()
{
	ToggleLockOn();
}

void AIronvowPlayerCharacter::InputRecoverPressed()
{
	if (Ragdoll)
	{
		Ragdoll->TryRecover();
	}
}

APlayerController* AIronvowPlayerCharacter::GetPlayerControllerSafe() const
{
	return Cast<APlayerController>(GetController());
}
