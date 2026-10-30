#include "Components/ActiveRagdollComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaminaComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"

UActiveRagdollComponent::UActiveRagdollComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UActiveRagdollComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureComponents();

	if (Mesh && PhysicalAnimation)
	{
		PhysicalAnimation->SetSkeletalMeshComponent(Mesh);
		PhysicalAnimation->ApplyPhysicalAnimationProfileBelow(RootBone, PhysicalAnimationProfile, true, true);
		PhysicalAnimation->SetStrengthMultiplyer(BaseMotorStrength);
	}

	Balance = 1.f;
	LegIntegrity = 1.f;
}

void UActiveRagdollComponent::EnsureComponents()
{
	if (!OwnerCharacter)
	{
		OwnerCharacter = Cast<ACharacter>(GetOwner());
	}

	if (OwnerCharacter && !Mesh)
	{
		Mesh = OwnerCharacter->GetMesh();
	}

	if (OwnerCharacter && !Stamina)
	{
		Stamina = OwnerCharacter->FindComponentByClass<UStaminaComponent>();
	}

	if (OwnerCharacter && !PhysicalAnimation)
	{
		PhysicalAnimation = OwnerCharacter->FindComponentByClass<UPhysicalAnimationComponent>();
		if (!PhysicalAnimation)
		{
			PhysicalAnimation = NewObject<UPhysicalAnimationComponent>(OwnerCharacter, TEXT("IronvowPhysicalAnimation"));
			PhysicalAnimation->RegisterComponent();
			if (Mesh)
			{
				PhysicalAnimation->SetSkeletalMeshComponent(Mesh);
				PhysicalAnimation->ApplyPhysicalAnimationProfileBelow(RootBone, PhysicalAnimationProfile, true, true);
			}
		}
	}
}

void UActiveRagdollComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Mesh)
	{
		return;
	}

	if (bRagdoll)
	{
		if (bRecovering)
		{
			UpdateRecovery(DeltaTime);
		}
		return;
	}

	UpdateBalance(DeltaTime);
}

void UActiveRagdollComponent::UpdateBalance(float DeltaTime)
{
	const FVector Support = (Mesh->GetSocketLocation(LeftFootBone) + Mesh->GetSocketLocation(RightFootBone)) * 0.5f;
	const FVector Pelvis = Mesh->GetSocketLocation(RootBone);
	const float LeanDistance = FVector::Dist2D(Pelvis, Support);

	const float LeanRange = FMath::Max(1.f, LeanMaxDistanceCm - LeanToleranceCm);
	const float LeanSeverity = FMath::Clamp((LeanDistance - LeanToleranceCm) / LeanRange, 0.f, 1.f);
	const float LegPenalty = FMath::Lerp(1.f, 2.2f, 1.f - LegIntegrity);

	const float Drain = LeanSeverity * BalanceDrainPerMeter * LegPenalty;
	const float Recovery = (1.f - LeanSeverity) * BalanceRecoveryPerSecond * FMath::Lerp(0.4f, 1.f, LegIntegrity);

	const float PreviousBalance = Balance;
	Balance = FMath::Clamp(Balance - Drain * DeltaTime + Recovery * DeltaTime, 0.f, 1.f);

	if (!FMath::IsNearlyEqual(PreviousBalance, Balance, 0.01f))
	{
		OnBalanceChanged.Broadcast(Balance);
	}

	if (Balance <= 0.f && !bRagdoll)
	{
		SetRagdollActive(true);
		return;
	}

	if (StumbleRemaining > 0.f)
	{
		StumbleRemaining = FMath::Max(0.f, StumbleRemaining - DeltaTime);
	}

	// Pull the body back over its feet. Force grows with the lean.
	if (LeanDistance > LeanToleranceCm)
	{
		FVector ToSupport = Support - Pelvis;
		ToSupport.Z = 0.f;

		if (!ToSupport.IsNearlyZero())
		{
			const float ForceMagnitude = UprightForce
				* FMath::Clamp(LeanDistance / FMath::Max(1.f, LeanMaxDistanceCm), 0.f, 1.5f)
				* FMath::Lerp(1.f, 0.45f, LegIntegrity >= 1.f ? 0.f : 1.f - LegIntegrity)
				* MotorScale;

			Mesh->AddForceAtLocation(ToSupport.GetSafeNormal() * ForceMagnitude, Pelvis, RootBone);
		}
	}

	ApplyMotorStrength();
}

void UActiveRagdollComponent::ApplyMotorStrength()
{
	if (!PhysicalAnimation)
	{
		return;
	}

	const float StumbleScale = StumbleRemaining > 0.f ? 0.35f : 1.f;
	const float BalanceScale = FMath::Lerp(StumbleMotorStrength, BaseMotorStrength, Balance);
	const float LegScale = FMath::Lerp(0.35f, 1.f, LegIntegrity);

	PhysicalAnimation->SetStrengthMultiplyer(BalanceScale * StumbleScale * LegScale * MotorScale);
}

void UActiveRagdollComponent::SetRagdollActive(bool bActive)
{
	EnsureComponents();
	if (!Mesh || bRagdoll == bActive)
	{
		return;
	}

	if (bActive)
	{
		bRagdoll = true;
		bRecovering = false;
		bRecoveredThisFall = false;

		if (PhysicalAnimation)
		{
			PhysicalAnimation->SetStrengthMultiplyer(0.f);
		}

		if (OwnerCharacter)
		{
			if (UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
			{
				Movement->DisableMovement();
			}
			if (UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent())
			{
				Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}

		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetCollisionProfileName(TEXT("Ragdoll"));
		Mesh->SetAllBodiesBelowSimulatePhysics(RootBone, true, true);
		Mesh->SetAllBodiesBelowPhysicsBlendWeight(RootBone, 1.f, false, true);
		Mesh->SetSimulatePhysics(true);
		Mesh->WakeAllRigidBodies();

		OnRagdollStateChanged.Broadcast(true);
		return;
	}

	bRagdoll = false;
	bRecovering = false;

	const FVector PelvisLocation = Mesh->GetSocketLocation(RootBone);

	Mesh->SetAllBodiesBelowPhysicsBlendWeight(RootBone, 0.f, false, true);
	Mesh->SetAllBodiesBelowSimulatePhysics(RootBone, false, true);
	Mesh->SetSimulatePhysics(false);
	Mesh->SetRelativeLocationAndRotation(FVector::ZeroVector, FQuat::Identity);
	Mesh->SetCollisionProfileName(TEXT("CharacterMesh"));

	if (OwnerCharacter)
	{
		if (UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent())
		{
			Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}

		SnapCapsuleToBody();

		if (UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
		{
			Movement->SetMovementMode(MOVE_Walking);
		}

		OwnerCharacter->SetActorLocation(
			FVector(PelvisLocation.X, PelvisLocation.Y, OwnerCharacter->GetActorLocation().Z),
			false, nullptr, ETeleportType::TeleportPhysics);
	}

	if (PhysicalAnimation)
	{
		PhysicalAnimation->SetSkeletalMeshComponent(Mesh);
		PhysicalAnimation->ApplyPhysicalAnimationProfileBelow(RootBone, PhysicalAnimationProfile, true, true);
		PhysicalAnimation->SetStrengthMultiplyer(BaseMotorStrength);
	}

	Balance = FMath::Max(Balance, 0.5f);
	StumbleRemaining = StumbleDuration * 0.5f;

	OnRagdollStateChanged.Broadcast(false);
}

void UActiveRagdollComponent::SnapCapsuleToBody()
{
	if (!OwnerCharacter || !Mesh)
	{
		return;
	}

	UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
	if (!Capsule)
	{
		return;
	}

	const FVector Pelvis = Mesh->GetSocketLocation(RootBone);
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	FVector Target(Pelvis.X, Pelvis.Y, Pelvis.Z);

	// Keep the capsule above the floor.
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(IronvowSnapCapsule), false, OwnerCharacter);
	const FVector TraceStart = Target + FVector(0.f, 0.f, 120.f);
	const FVector TraceEnd = Target - FVector(0.f, 0.f, 400.f);

	if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
	{
		Target.Z = Hit.ImpactPoint.Z + HalfHeight;
	}

	Capsule->SetWorldLocation(Target, false, nullptr, ETeleportType::TeleportPhysics);
}

void UActiveRagdollComponent::ApplyImpactReaction(const FVector& Impulse, FName HitBone, float Severity)
{
	EnsureComponents();
	if (!Mesh)
	{
		return;
	}

	if (bRagdoll)
	{
		Mesh->AddImpulse(Impulse, HitBone, false);
		return;
	}

	const float ClampedSeverity = FMath::Clamp(Severity, 0.f, 3.f);
	const float LegPenalty = FMath::Lerp(1.f, 1.9f, 1.f - LegIntegrity);

	StumbleRemaining = FMath::Max(StumbleRemaining, StumbleDuration * FMath::Clamp(ClampedSeverity, 0.35f, 1.6f));
	Balance = FMath::Clamp(Balance - ImpactBalanceScale * ClampedSeverity * LegPenalty, 0.f, 1.f);

	// A partial impulse keeps the standing body visibly rocked by the blow.
	Mesh->AddImpulse(Impulse * 0.30f, HitBone, false);
	OnBalanceChanged.Broadcast(Balance);

	if (Balance <= 0.f)
	{
		SetRagdollActive(true);
		Mesh->AddImpulse(Impulse, HitBone, false);
	}
}

bool UActiveRagdollComponent::TryRecover()
{
	if (!bRagdoll || bRecovering)
	{
		return false;
	}

	if (LegIntegrity < MinLegIntegrityToRecover)
	{
		return false;
	}

	EnsureComponents();

	if (Stamina)
	{
		if (Stamina->GetCurrent() < MinStaminaToRecover)
		{
			return false;
		}
		Stamina->TryConsume(RecoverStaminaCost);
	}

	bRecovering = true;
	RecoverElapsed = 0.f;
	return true;
}

void UActiveRagdollComponent::UpdateRecovery(float DeltaTime)
{
	RecoverElapsed += DeltaTime;

	const float Alpha = FMath::Clamp(RecoverElapsed / FMath::Max(0.1f, RecoverDuration), 0.f, 1.f);
	if (Mesh)
	{
		Mesh->SetAllBodiesBelowPhysicsBlendWeight(RootBone, 1.f - Alpha, false, true);
	}

	if (Alpha >= 1.f)
	{
		const float TimeTaken = RecoverElapsed;
		SetRagdollActive(false);
		bRecoveredThisFall = true;
		OnRagdollRecovered.Broadcast(TimeTaken);
	}
}

void UActiveRagdollComponent::SetLegIntegrity(float InLegIntegrity)
{
	LegIntegrity = FMath::Clamp(InLegIntegrity, 0.f, 1.f);
}

void UActiveRagdollComponent::SetMotorStrengthScale(float Scale)
{
	MotorScale = FMath::Clamp(Scale, 0.f, 2.f);
}

void UActiveRagdollComponent::SetStaminaComponent(UStaminaComponent* InStamina)
{
	Stamina = InStamina;
}

float UActiveRagdollComponent::GetRecoveryAlpha() const
{
	if (!bRagdoll)
	{
		return 1.f;
	}
	if (!bRecovering)
	{
		return 0.f;
	}
	return FMath::Clamp(RecoverElapsed / FMath::Max(0.1f, RecoverDuration), 0.f, 1.f);
}

FVector UActiveRagdollComponent::GetPelvisLocation() const
{
	return Mesh ? Mesh->GetSocketLocation(RootBone) : FVector::ZeroVector;
}
