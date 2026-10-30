#include "Combat/WeaponBase.h"

#include "Characters/IronvowCharacterBase.h"
#include "Components/ActiveRagdollComponent.h"
#include "Components/DamageComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaminaComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/IronvowPresets.h"
#include "Data/WeaponDataAsset.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "Sound/SoundBase.h"

AWeaponBase::AWeaponBase()
{
	PrimaryActorTick.bCanEverTick = true;

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	RootComponent = WeaponMesh;
	WeaponMesh->SetSimulatePhysics(false);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	WeaponMesh->SetNotifyRigidBodyCollision(true);
	WeaponMesh->SetGenerateOverlapEvents(false);
	WeaponMesh->SetLinearDamping(0.05f);
	WeaponMesh->SetAngularDamping(0.35f);

	MainGripConstraint = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("MainGrip"));
	MainGripConstraint->SetupAttachment(WeaponMesh);

	OffhandGripConstraint = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("OffhandGrip"));
	OffhandGripConstraint->SetupAttachment(WeaponMesh);
}

void AWeaponBase::BeginPlay()
{
	Super::BeginPlay();

	if (!WeaponData)
	{
		InitializeWeapon(nullptr, PresetId);
	}
	else
	{
		UpdateMassProperties();
	}

	if (WeaponMesh)
	{
		WeaponMesh->OnComponentHit.AddDynamic(this, &AWeaponBase::HandleWeaponHit);
	}
}

void AWeaponBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (RecentHitTimes.Num() > 0)
	{
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		for (auto It = RecentHitTimes.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid() || Now - It.Value() > 5.f)
			{
				It.RemoveCurrent();
			}
		}
	}
}

void AWeaponBase::InitializeWeapon(UWeaponDataAsset* InData, EWeaponId InPresetId)
{
	WeaponData = InData ? InData : (WeaponData ? WeaponData : UIronvowPresetLibrary::MakeWeaponData(InPresetId, this));
	if (!WeaponData)
	{
		return;
	}

	PresetId = WeaponData->PresetId;

	UStaticMesh* LoadedMesh = WeaponData->Mesh.IsNull() ? nullptr : WeaponData->Mesh.LoadSynchronous();
	bool bPlaceholder = false;
	if (!LoadedMesh)
	{
		// Fall back to an engine cube so a weapon is always visible and hittable.
		LoadedMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		bPlaceholder = true;
	}

	if (LoadedMesh && WeaponMesh)
	{
		WeaponMesh->SetStaticMesh(LoadedMesh);

		if (bPlaceholder)
		{
			const float Length = FMath::Max(10.f, WeaponData->LengthCm);
			WeaponMesh->SetRelativeScale3D(FVector(Length / 100.f, 0.05f, 0.09f));
			WeaponMesh->SetRelativeLocation(FVector(Length * 0.5f - 8.f, 0.f, 0.f));
		}
	}

	UpdateMassProperties();
}

void AWeaponBase::UpdateMassProperties()
{
	if (!WeaponMesh || !WeaponData)
	{
		return;
	}

	WeaponMesh->SetMassOverrideInKg(NAME_None, FMath::Max(0.1f, WeaponData->MassKg), true);
	WeaponMesh->SetCenterOfMass(FVector(WeaponData->BalanceOffsetCm, 0.f, 0.f), NAME_None);
	WeaponMesh->SetLinearDamping(0.04f);
	WeaponMesh->SetAngularDamping(0.55f);
}

void AWeaponBase::ConfigureConstraint(UPhysicsConstraintComponent* Constraint, FName WeaponSocket, FName HandBone, float AngularStiffness)
{
	if (!Constraint || !WeaponMesh || !Wielder)
	{
		return;
	}

	USkeletalMeshComponent* WielderMesh = Wielder->GetMesh();
	if (!WielderMesh)
	{
		return;
	}

	const bool bHasSocket = WeaponSocket != NAME_None && WeaponMesh->DoesSocketExist(WeaponSocket);
	const FVector FrameLocation = bHasSocket ? WeaponMesh->GetSocketLocation(WeaponSocket) : WeaponMesh->GetComponentLocation();

	Constraint->SetWorldLocationAndRotation(FrameLocation, WeaponMesh->GetComponentQuat());
	Constraint->SetDisableCollision(true);
	Constraint->SetConstrainedComponents(WeaponMesh, NAME_None, WielderMesh, HandBone);

	Constraint->SetLinearXLimit(ELinearConstraintMotion::LCM_Locked, 0.f);
	Constraint->SetLinearYLimit(ELinearConstraintMotion::LCM_Locked, 0.f);
	Constraint->SetLinearZLimit(ELinearConstraintMotion::LCM_Locked, 0.f);

	Constraint->SetAngularSwing1Limit(ELinearConstraintMotion::ACM_Limited, 42.f);
	Constraint->SetAngularSwing2Limit(ELinearConstraintMotion::ACM_Limited, 42.f);
	Constraint->SetAngularTwistLimit(ELinearConstraintMotion::ACM_Limited, 55.f);

	Constraint->SetAngularDriveMode(EAngularDriveMode::SLERP);
	Constraint->SetAngularDriveParams(AngularStiffness, AngularStiffness * 0.06f, 0.f);
	Constraint->SetAngularOrientationTarget(FRotator::ZeroRotator);
	Constraint->SetAngularVelocityTarget(FVector::ZeroVector);
	Constraint->SetLinearPositionTarget(FVector::ZeroVector);
	Constraint->SetLinearVelocityTarget(FVector::ZeroVector);
}

void AWeaponBase::AttachToWielder(AIronvowCharacterBase* InWielder, EWeaponGripMode Mode)
{
	if (!InWielder || !WeaponMesh)
	{
		return;
	}

	if (!WeaponData)
	{
		InitializeWeapon(nullptr, PresetId);
	}

	Wielder = InWielder;
	bHeld = true;
	GripMode = Mode;
	CachedWielderDamage = Wielder->FindComponentByClass<UDamageComponent>();

	USkeletalMeshComponent* WielderMesh = Wielder->GetMesh();
	if (!WielderMesh)
	{
		return;
	}

	WeaponMesh->SetSimulatePhysics(false);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	WeaponMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	WeaponMesh->SetNotifyRigidBodyCollision(true);

	WeaponMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
	WeaponMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

	const FName HandBone = WeaponData ? WeaponData->MainHandBone : TEXT("hand_r");
	const FVector HandLocation = WielderMesh->GetSocketLocation(HandBone);
	WeaponMesh->SetWorldLocation(HandLocation);

	WeaponMesh->SetSimulatePhysics(true);
	UpdateMassProperties();

	ConfigureConstraint(MainGripConstraint, WeaponData->MainGripSocket, HandBone, 14000.f);

	if (Mode == EWeaponGripMode::OneHanded)
	{
		if (OffhandGripConstraint)
		{
			OffhandGripConstraint->BreakConstraint();
		}
	}
	else
	{
		const FName Socket = (Mode == EWeaponGripMode::HalfSword) ? WeaponData->HalfSwordSocket : WeaponData->OffhandGripSocket;
		ConfigureConstraint(OffhandGripConstraint, Socket, WeaponData->OffHandBone, 10000.f);
	}
}

void AWeaponBase::SetGripMode(EWeaponGripMode NewMode)
{
	if (!WeaponData || !bHeld)
	{
		return;
	}

	if (NewMode == EWeaponGripMode::HalfSword && !WeaponData->bSupportsHalfSword)
	{
		NewMode = EWeaponGripMode::TwoHanded;
	}

	if (GripMode == NewMode)
	{
		return;
	}

	GripMode = NewMode;

	ConfigureConstraint(MainGripConstraint, WeaponData->MainGripSocket, WeaponData->MainHandBone,
		GripMode == EWeaponGripMode::HalfSword ? 18000.f : 14000.f);

	if (GripMode == EWeaponGripMode::OneHanded)
	{
		if (OffhandGripConstraint)
		{
			OffhandGripConstraint->BreakConstraint();
		}
		return;
	}

	const FName Socket = (GripMode == EWeaponGripMode::HalfSword) ? WeaponData->HalfSwordSocket : WeaponData->OffhandGripSocket;
	ConfigureConstraint(OffhandGripConstraint, Socket, WeaponData->OffHandBone, 11000.f);
}

void AWeaponBase::DetachFromWielder()
{
	if (MainGripConstraint)
	{
		MainGripConstraint->BreakConstraint();
	}
	if (OffhandGripConstraint)
	{
		OffhandGripConstraint->BreakConstraint();
	}

	Wielder = nullptr;
	bHeld = false;
	CachedWielderDamage = nullptr;

	if (WeaponMesh)
	{
		WeaponMesh->SetSimulatePhysics(true);
	}
}

void AWeaponBase::DropWeapon(const FVector& Impulse)
{
	DetachFromWielder();

	if (WeaponMesh)
	{
		WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		if (!Impulse.IsNearlyZero())
		{
			WeaponMesh->AddImpulse(Impulse, NAME_None, true);
		}
	}

	OnWeaponDropped.Broadcast();
}

void AWeaponBase::AddTipForce(const FVector& Force)
{
	if (!WeaponMesh || !bHeld)
	{
		return;
	}
	WeaponMesh->AddForceAtLocation(Force, GetTipLocation(), NAME_None);
}

void AWeaponBase::AddGripForce(const FVector& Force)
{
	if (!WeaponMesh || !bHeld)
	{
		return;
	}
	WeaponMesh->AddForceAtLocation(Force, GetGripLocation(), NAME_None);
}

UWeaponDataAsset* AWeaponBase::GetWeaponData() const
{
	return WeaponData;
}

FVector AWeaponBase::GetTipLocation() const
{
	if (!WeaponMesh)
	{
		return FVector::ZeroVector;
	}

	if (WeaponMesh->DoesSocketExist(TEXT("Tip")))
	{
		return WeaponMesh->GetSocketLocation(TEXT("Tip"));
	}

	const FVector Offset = WeaponData ? WeaponData->TipOffset : FVector(90.f, 0.f, 0.f);
	return WeaponMesh->GetComponentTransform().TransformPosition(Offset);
}

FVector AWeaponBase::GetTipVelocity() const
{
	if (!WeaponMesh)
	{
		return FVector::ZeroVector;
	}
	return WeaponMesh->GetPhysicsLinearVelocityAtPoint(GetTipLocation(), NAME_None);
}

FVector AWeaponBase::GetBladeAxis() const
{
	return WeaponMesh ? WeaponMesh->GetComponentTransform().GetUnitAxis(EAxis::X) : FVector::ForwardVector;
}

FVector AWeaponBase::GetGripLocation() const
{
	if (!WeaponMesh)
	{
		return FVector::ZeroVector;
	}

	if (WeaponData && WeaponData->MainGripSocket != NAME_None && WeaponMesh->DoesSocketExist(WeaponData->MainGripSocket))
	{
		return WeaponMesh->GetSocketLocation(WeaponData->MainGripSocket);
	}

	return WeaponMesh->GetComponentLocation();
}

float AWeaponBase::GetImpactSpeed() const
{
	if (!WeaponMesh)
	{
		return 0.f;
	}

	const float TipSpeed = GetTipVelocity().Size();
	const float AngularSpeed = WeaponMesh->GetPhysicsAngularVelocityInRadians().Size();
	const float Reach = GetEffectiveReach() * 0.01f;
	return FMath::Max(TipSpeed, AngularSpeed * Reach * 100.f);
}

float AWeaponBase::GetEffectiveReach() const
{
	return FVector::Dist(GetGripLocation(), GetTipLocation());
}

void AWeaponBase::HandleWeaponHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!OtherActor || OtherActor == this || OtherActor == Wielder || !bHeld)
	{
		return;
	}

	const float Speed = GetImpactSpeed();
	if (Speed < MinImpactSpeed)
	{
		return;
	}

	if (AWeaponBase* OtherWeapon = Cast<AWeaponBase>(OtherActor))
	{
		ResolveWeaponCollision(OtherWeapon, Hit, Speed);
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (const float* LastHit = RecentHitTimes.Find(OtherActor))
	{
		if (Now - *LastHit < HitCooldown)
		{
			return;
		}
	}
	RecentHitTimes.Add(OtherActor, Now);

	UDamageComponent* TargetDamage = OtherActor->FindComponentByClass<UDamageComponent>();
	if (!WeaponData)
	{
		return;
	}

	if (!TargetDamage)
	{
		// Arena props, palisades and crates: audible, but nothing to wound.
		FWeaponImpactData PropImpact;
		PropImpact.SpeedCmPerSec = Speed;
		PropImpact.Location = Hit.ImpactPoint;
		SpawnImpactFeedback(PropImpact, Hit, EWeaponImpactSurface::Wood);
		return;
	}

	// Travel direction: how the contact point is actually moving.
	const FVector ContactVelocity = WeaponMesh->GetPhysicsLinearVelocityAtPoint(Hit.ImpactPoint, NAME_None);
	FVector Direction = ContactVelocity.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = GetBladeAxis();
	}

	// Flatness: 1 when the blade's flat faces the surface, 0 when edge-on.
	const FVector BladeNormal = WeaponMesh->GetComponentTransform().GetUnitAxis(EAxis::Z);
	const float Flatness = FMath::Clamp(FMath::Abs(FVector::DotProduct(BladeNormal, Hit.ImpactNormal)), 0.f, 1.f);

	FWeaponImpactData Impact;
	Impact.SpeedCmPerSec = Speed;
	Impact.MassKg = WeaponData->MassKg;
	Impact.BaseDamage = WeaponData->BaseDamage;
	Impact.Edge = WeaponData->Edge;
	Impact.Location = Hit.ImpactPoint;
	Impact.Direction = Direction;
	Impact.Flatness = Flatness * (WeaponData->Edge == EWeaponEdge::Blunt ? 0.25f : 1.f);
	Impact.bHalfSword = GripMode == EWeaponGripMode::HalfSword;

	if (const UStaminaComponent* WielderStamina = Wielder ? Wielder->FindComponentByClass<UStaminaComponent>() : nullptr)
	{
		Impact.StaminaScale = WielderStamina->GetPowerScale();
	}

	const FDamageResult Result = TargetDamage->ApplyWeaponImpact(this, Impact, Hit.BoneName);

	// Physical reaction: the body is pushed by the actual momentum.
	if (UActiveRagdollComponent* Ragdoll = OtherActor->FindComponentByClass<UActiveRagdollComponent>())
	{
		const FVector Impulse = Impact.Direction * Impact.MassKg * Impact.SpeedCmPerSec * 0.25f;
		const float Severity = (Impact.SpeedCmPerSec / 1100.f) * FMath::Sqrt(Impact.MassKg);
		Ragdoll->ApplyImpactReaction(Impulse, Hit.BoneName, Severity);
	}

	if (Wielder)
	{
		if (UStaminaComponent* WielderStamina = Wielder->FindComponentByClass<UStaminaComponent>())
		{
			WielderStamina->TryConsume(WeaponData->SwingStaminaCost * 0.25f);
		}
	}

	const bool bMetal = Result.StoppedBy != EArmorLayer::None || Result.Result == EHitResult::Deflected;
	SpawnImpactFeedback(Impact, Hit, bMetal ? EWeaponImpactSurface::Metal : EWeaponImpactSurface::Flesh);

	OnWeaponImpact.Broadcast(OtherActor, Result, Hit.ImpactPoint);
}

void AWeaponBase::ResolveWeaponCollision(AWeaponBase* OtherWeapon, const FHitResult& Hit, float RelativeSpeed)
{
	if (!OtherWeapon || !WeaponMesh)
	{
		return;
	}

	const FVector Normal = Hit.ImpactNormal.GetSafeNormal();

	// Both blades are pushed apart by the exchange; the weaker grip loses more.
	const float PushImpulse = FMath::Clamp(RelativeSpeed * 0.02f, 20.f, 600.f);
	WeaponMesh->AddImpulseAtLocation(Normal * PushImpulse, Hit.ImpactPoint, NAME_None);

	if (OtherWeapon->WeaponMesh)
	{
		float OtherStaminaScale = 0.6f;
		if (const UStaminaComponent* DefenderStamina = OtherWeapon->Wielder ? OtherWeapon->Wielder->FindComponentByClass<UStaminaComponent>() : nullptr)
		{
			OtherStaminaScale = DefenderStamina->GetPowerScale();
		}

		OtherWeapon->WeaponMesh->AddImpulseAtLocation(
			-Normal * PushImpulse * (1.35f - OtherStaminaScale * 0.5f), Hit.ImpactPoint, NAME_None);
	}

	// Blocking costs the defender stamina; a spent guard breaks.
	if (OtherWeapon->Wielder)
	{
		if (UStaminaComponent* DefenderStamina = OtherWeapon->Wielder->FindComponentByClass<UStaminaComponent>())
		{
			DefenderStamina->ConsumeUpTo(RelativeSpeed * 0.006f);
		}
	}

	if (Wielder)
	{
		if (UStaminaComponent* AttackerStamina = Wielder->FindComponentByClass<UStaminaComponent>())
		{
			AttackerStamina->TryConsume(RelativeSpeed * 0.002f);
		}
	}

	const FWeaponImpactData MetalImpact = [RelativeSpeed]()
	{
		FWeaponImpactData Data;
		Data.SpeedCmPerSec = RelativeSpeed;
		Data.Edge = EWeaponEdge::Blunt;
		return Data;
	}();

	SpawnImpactFeedback(MetalImpact, Hit, EWeaponImpactSurface::Metal);

	OnWeaponParried.Broadcast(OtherWeapon->Wielder, RelativeSpeed);
}

void AWeaponBase::SpawnImpactFeedback(const FWeaponImpactData& Impact, const FHitResult& Hit, EWeaponImpactSurface Surface)
{
	if (!WeaponData || GetWorld()->GetTimeSeconds() < NextFeedbackTime)
	{
		return;
	}
	NextFeedbackTime = GetWorld()->GetTimeSeconds() + 0.05f;

	const FImpactFeedback& Feedback = WeaponData->Feedback;

	TSoftObjectPtr<USoundBase> SoundPtr;
	switch (Surface)
	{
	case EWeaponImpactSurface::Metal: SoundPtr = Feedback.MetalImpactSound; break;
	case EWeaponImpactSurface::Wood: SoundPtr = Feedback.WoodImpactSound; break;
	case EWeaponImpactSurface::Flesh:
	default: SoundPtr = Feedback.FleshImpactSound; break;
	}

	USoundBase* Sound = SoundPtr.IsNull() ? nullptr : SoundPtr.LoadSynchronous();

	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Hit.ImpactPoint, 1.f, FMath::FRandRange(0.92f, 1.08f));
	}

	if (Surface == EWeaponImpactSurface::Flesh && !Feedback.BloodDecalMaterial.IsNull())
	{
		if (UMaterialInterface* DecalMaterial = Feedback.BloodDecalMaterial.LoadSynchronous())
		{
			UGameplayStatics::SpawnDecalAtLocation(
				this,
				DecalMaterial,
				FVector(Feedback.DecalSizeCm),
				Hit.ImpactPoint,
				(-Hit.ImpactNormal).Rotation(),
				Feedback.DecalLifeSpan);
		}
	}

	if (Surface == EWeaponImpactSurface::Wood && !Feedback.WoodChipDecalMaterial.IsNull())
	{
		if (UMaterialInterface* DecalMaterial = Feedback.WoodChipDecalMaterial.LoadSynchronous())
		{
			UGameplayStatics::SpawnDecalAtLocation(
				this,
				DecalMaterial,
				FVector(Feedback.DecalSizeCm * 0.6f),
				Hit.ImpactPoint,
				(-Hit.ImpactNormal).Rotation(),
				Feedback.DecalLifeSpan * 0.6f);
		}
	}
}
