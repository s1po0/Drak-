#include "Characters/IronvowCharacterBase.h"

#include "Combat/WeaponBase.h"
#include "Components/ActiveRagdollComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DamageComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaminaComponent.h"
#include "Components/WeaponControlComponent.h"
#include "Data/ArmorDataAsset.h"
#include "Data/IronvowPresets.h"
#include "Data/WeaponDataAsset.h"
#include "Game/IronvowGameModeBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"

AIronvowCharacterBase::AIronvowCharacterBase()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	Capsule->InitCapsuleSize(38.f, 88.f);

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = false;
	Movement->MaxWalkSpeed = WalkSpeed;
	Movement->BrakingDecelerationWalking = 1800.f;
	Movement->GravityScale = 1.f;

	PhysicalAnimation = CreateDefaultSubobject<UPhysicalAnimationComponent>(TEXT("PhysicalAnimation"));

	Ragdoll = CreateDefaultSubobject<UActiveRagdollComponent>(TEXT("ActiveRagdoll"));
	Stamina = CreateDefaultSubobject<UStaminaComponent>(TEXT("Stamina"));
	Damage = CreateDefaultSubobject<UDamageComponent>(TEXT("Damage"));
	WeaponControl = CreateDefaultSubobject<UWeaponControlComponent>(TEXT("WeaponControl"));
}

void AIronvowCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (Damage)
	{
		Damage->OnCombatantDied.AddDynamic(this, &AIronvowCharacterBase::HandleDeathInternal);
		Damage->OnZoneDamaged.AddDynamic(this, &AIronvowCharacterBase::HandleZoneDamaged);
	}

	if (Ragdoll)
	{
		Ragdoll->SetStaminaComponent(Stamina);
		Ragdoll->OnRagdollStateChanged.AddDynamic(this, &AIronvowCharacterBase::HandleRagdollStateChanged);
		Ragdoll->OnRagdollRecovered.AddDynamic(this, &AIronvowCharacterBase::HandleRecovered);
	}

	EquipArmor(StartingArmor, StartingArmorData);
	EquipWeapon(StartingWeapon, StartingWeaponData);

	RefreshMovementSpeed();
}

void AIronvowCharacterBase::EquipWeapon(EWeaponId WeaponId, UWeaponDataAsset* OverrideData)
{
	if (!bAlive)
	{
		return;
	}

	if (Weapon)
	{
		Weapon->DetachFromWielder();
		Weapon->Destroy();
		Weapon = nullptr;
	}

	UClass* SpawnClass = WeaponClass ? WeaponClass.Get() : AWeaponBase::StaticClass();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FVector SpawnLocation = GetMesh() ? GetMesh()->GetComponentLocation() : GetActorLocation();
	Weapon = GetWorld()->SpawnActor<AWeaponBase>(SpawnClass, SpawnLocation, GetActorRotation(), SpawnParams);
	if (!Weapon)
	{
		return;
	}

	UWeaponDataAsset* Data = OverrideData ? OverrideData : UIronvowPresetLibrary::MakeWeaponData(WeaponId, this);
	Weapon->InitializeWeapon(Data, WeaponId);
	Weapon->OnWeaponImpact.AddDynamic(this, &AIronvowCharacterBase::HandleWeaponImpactRelay);
	Weapon->AttachToWielder(this, EWeaponGripMode::OneHanded);

	if (WeaponControl)
	{
		WeaponControl->SetWeapon(Weapon);
		WeaponControl->SetGuardRaised(false);
	}

	RefreshMovementSpeed();
}

void AIronvowCharacterBase::EquipArmor(EArmorSetId ArmorId, UArmorDataAsset* OverrideData)
{
	UArmorDataAsset* Data = OverrideData ? OverrideData : UIronvowPresetLibrary::MakeArmorData(ArmorId, this);
	Armor = Data;

	if (Damage)
	{
		Damage->SetArmorData(Data, true);
	}

	if (Stamina && Data)
	{
		Stamina->ArmorCostMultiplier = Data->StaminaCostMultiplier;
	}

	if (Data && !Data->Mesh.IsNull())
	{
		if (USkeletalMesh* ArmorMesh = Data->Mesh.LoadSynchronous())
		{
			GetMesh()->SetSkeletalMeshAsset(ArmorMesh);
		}
	}

	RefreshMovementSpeed();
}

void AIronvowCharacterBase::SetGuardRaised(bool bRaised)
{
	if (WeaponControl && bAlive && !IsRagdolled())
	{
		WeaponControl->SetGuardRaised(bRaised);
	}
}

bool AIronvowCharacterBase::PerformThrust()
{
	return bAlive && !IsRagdolled() && WeaponControl ? WeaponControl->CommitThrust() : false;
}

void AIronvowCharacterBase::CycleGripMode()
{
	if (WeaponControl && bAlive && !IsRagdolled())
	{
		WeaponControl->CycleGripMode();
	}
}

void AIronvowCharacterBase::SetWeaponAimTarget(const FVector& WorldTarget)
{
	if (WeaponControl)
	{
		WeaponControl->SetAimTarget(WorldTarget);
	}
}

void AIronvowCharacterBase::StartSprint()
{
	if (!bAlive || IsRagdolled() || !Stamina)
	{
		return;
	}

	if (Stamina->GetStaminaRatio() > 0.15f)
	{
		bSprinting = true;
	}
}

void AIronvowCharacterBase::StopSprint()
{
	bSprinting = false;
}

bool AIronvowCharacterBase::DodgeStep(const FVector& WorldDirection)
{
	if (!bAlive || IsRagdolled() || DodgeCooldownRemaining > 0.f)
	{
		return false;
	}

	if (!Stamina || !Stamina->TryConsume(DodgeStaminaCost))
	{
		return false;
	}

	const FVector Direction = FVector(WorldDirection.X, WorldDirection.Y, 0.f).GetSafeNormal();
	LaunchCharacter(Direction * DodgeImpulse + FVector(0.f, 0.f, 120.f), true, true);
	DodgeCooldownRemaining = DodgeCooldown;
	return true;
}

void AIronvowCharacterBase::AIStartAttack(AActor* Target)
{
	if (!bAlive || IsRagdolled() || !Weapon)
	{
		return;
	}

	bAIAttacking = true;
	AIAttackElapsed = 0.f;
	AITarget = Target;
	AIAttackOrigin = Target ? Target->GetActorLocation() : GetActorLocation() + GetActorForwardVector() * 150.f;

	if (WeaponControl)
	{
		WeaponControl->SetGuardRaised(false);

		const UWeaponDataAsset* Data = Weapon->GetWeaponData();
		if (Data && Data->Edge == EWeaponEdge::Pierce)
		{
			WeaponControl->CommitThrust();
		}
	}
}

void AIronvowCharacterBase::AIStopAttack()
{
	bAIAttacking = false;
	AITarget = nullptr;

	if (WeaponControl)
	{
		WeaponControl->SetGuardRaised(true);
	}
}

float AIronvowCharacterBase::GetDistanceToTarget(const AActor* Target) const
{
	return Target ? FVector::Dist(Target->GetActorLocation(), GetActorLocation()) : TNumericLimits<float>::Max();
}

void AIronvowCharacterBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bAlive)
	{
		return;
	}

	DodgeCooldownRemaining = FMath::Max(0.f, DodgeCooldownRemaining - DeltaSeconds);

	UpdateSprintDrain(DeltaSeconds);
	UpdateRagdollIntegrity();
	UpdateRecoveryAttempt(DeltaSeconds);
	RefreshMovementSpeed();

	// Drive the swing arc for AI-controlled fighters.
	if (bAIAttacking && WeaponControl)
	{
		AIAttackElapsed += DeltaSeconds;
		const float Alpha = FMath::Clamp(AIAttackElapsed / FMath::Max(0.1f, AIAttackDuration), 0.f, 1.f);

		FVector Center = AITarget.IsValid() ? AITarget->GetActorLocation() : AIAttackOrigin;
		Center.Z += 10.f;

		const FVector Lateral = GetActorRightVector() * FMath::Lerp(140.f, -140.f, Alpha);
		const FVector Arc = Center + Lateral + FVector(0.f, 0.f, FMath::Sin(Alpha * PI) * 35.f);
		WeaponControl->SetAimTarget(Arc);

		if (Alpha >= 1.f)
		{
			AIStopAttack();
		}
	}
}

void AIronvowCharacterBase::UpdateSprintDrain(float DeltaSeconds)
{
	if (!bSprinting || !Stamina)
	{
		return;
	}

	const bool bMoving = GetVelocity().Size2D() > 40.f;
	if (!bMoving || IsRagdolled() || !Stamina->TryConsume(SprintStaminaPerSecond * DeltaSeconds))
	{
		bSprinting = false;
	}
}

void AIronvowCharacterBase::UpdateRagdollIntegrity()
{
	if (!Ragdoll || !Damage || !bAlive)
	{
		return;
	}

	Ragdoll->SetLegIntegrity(Damage->GetLegIntegrity());

	// A shattered leg cannot be stood on: the body goes down.
	if (!IsRagdolled() && Damage->GetWeakestLegRatio() <= 0.f)
	{
		StopSprint();
		Ragdoll->SetRagdollActive(true);
	}
}

void AIronvowCharacterBase::UpdateRecoveryAttempt(float DeltaSeconds)
{
	if (!Ragdoll || !bAlive || !IsRagdolled())
	{
		return;
	}

	RecoverAttemptCooldown = FMath::Max(0.f, RecoverAttemptCooldown - DeltaSeconds);
	if (RecoverAttemptCooldown > 0.f)
	{
		return;
	}

	RecoverAttemptCooldown = RecoverAttemptInterval;
	Ragdoll->TryRecover();
}

void AIronvowCharacterBase::RefreshMovementSpeed()
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = GetCurrentMaxSpeed();
	}
}

float AIronvowCharacterBase::GetCurrentMaxSpeed() const
{
	if (!bAlive || IsRagdolled())
	{
		return 0.f;
	}

	const float Base = bSprinting ? SprintSpeed : WalkSpeed;
	const float ArmorScale = Armor ? Armor->MoveSpeedMultiplier : 1.f;
	const float FatigueScale = Stamina ? FMath::Lerp(0.72f, 1.f, Stamina->GetStaminaRatio()) : 1.f;
	return Base * ArmorScale * FatigueScale * SpeedScale;
}

void AIronvowCharacterBase::SetSpeedScale(float NewScale)
{
	SpeedScale = FMath::Clamp(NewScale, 0.25f, 2.f);
	RefreshMovementSpeed();
}

float AIronvowCharacterBase::GetReach() const
{
	return Weapon ? Weapon->GetEffectiveReach() : 120.f;
}

float AIronvowCharacterBase::GetHealthRatio() const
{
	return Damage ? Damage->GetTotalHealthRatio() : 1.f;
}

bool AIronvowCharacterBase::IsRagdolled() const
{
	return Ragdoll && Ragdoll->IsRagdolled();
}

void AIronvowCharacterBase::Kill(AActor* Killer)
{
	HandleDeathInternal(Killer);
}

void AIronvowCharacterBase::ResetCombatantState()
{
	bAlive = true;
	bSprinting = false;
	bAIAttacking = false;
	RecoverAttemptCooldown = 0.f;
	DodgeCooldownRemaining = 0.f;

	if (Damage)
	{
		Damage->ResetCombatant();
	}

	if (Stamina)
	{
		Stamina->ResetStamina();
	}

	if (Ragdoll)
	{
		if (Ragdoll->IsRagdolled())
		{
			Ragdoll->SetRagdollActive(false);
		}
		Ragdoll->SetLegIntegrity(1.f);
	}

	if (WeaponControl)
	{
		WeaponControl->SetControlEnabled(true);
		WeaponControl->SetGuardRaised(false);
	}

	if (!Weapon)
	{
		EquipWeapon(StartingWeapon, StartingWeaponData);
	}
	else
	{
		Weapon->AttachToWielder(this, EWeaponGripMode::OneHanded);
		WeaponControl->SetWeapon(Weapon);
	}

	RefreshMovementSpeed();
}

void AIronvowCharacterBase::HandleDeathInternal(AActor* Killer)
{
	if (!bAlive)
	{
		return;
	}

	bAlive = false;
	bSprinting = false;
	bAIAttacking = false;

	if (WeaponControl)
	{
		WeaponControl->SetControlEnabled(false);
	}

	if (Weapon)
	{
		Weapon->DropWeapon(GetActorForwardVector() * 200.f);
	}

	if (Ragdoll)
	{
		Ragdoll->SetRagdollActive(true);
	}

	OnDeath.Broadcast(Killer);

	// Let the active game mode score the duel.
	if (AIronvowGameModeBase* GameMode = Cast<AIronvowGameModeBase>(UGameplayStatics::GetGameMode(this)))
	{
		GameMode->HandleCombatantDeath(this, Killer);
	}
}

void AIronvowCharacterBase::HandleZoneDamaged(EHitZone Zone, float Amount, EHitResult Result)
{
	// Leg destruction is handled through UpdateRagdollIntegrity so the ragdoll
	// reacts exactly once per fall.
}

void AIronvowCharacterBase::HandleWeaponImpactRelay(AActor* HitActor, FDamageResult Result, FVector ImpactPoint)
{
	OnWeaponImpact.Broadcast(HitActor, Result, ImpactPoint);
}

void AIronvowCharacterBase::HandleRagdollStateChanged(bool bIsRagdoll)
{
	if (bIsRagdoll)
	{
		StopSprint();
		if (WeaponControl)
		{
			WeaponControl->SetGuardRaised(false);
			WeaponControl->SetControlEnabled(false);
		}
	}
	else if (bAlive && WeaponControl)
	{
		WeaponControl->SetControlEnabled(true);
		RecoverAttemptCooldown = RecoverAttemptInterval;
	}

	RefreshMovementSpeed();
}

void AIronvowCharacterBase::HandleRecovered(float TimeTaken)
{
	RecoverAttemptCooldown = RecoverAttemptInterval * 0.5f;
	RefreshMovementSpeed();
}
