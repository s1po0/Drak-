#include "AI/IronvowAIController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "Characters/IronvowCharacterBase.h"
#include "Combat/WeaponBase.h"
#include "Components/StaminaComponent.h"
#include "Components/WeaponControlComponent.h"
#include "Data/IronvowPresets.h"
#include "Data/WeaponDataAsset.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"

AIronvowAIController::AIronvowAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	bAttachToPawn = true;
}

void AIronvowAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	SetDifficulty(DifficultyLevel);

	if (BehaviorTreeAsset)
	{
		bUsingBehaviorTree = RunBehaviorTree(BehaviorTreeAsset);
	}
}

void AIronvowAIController::OnUnPossess()
{
	bUsingBehaviorTree = false;
	Super::OnUnPossess();
}

void AIronvowAIController::SetDifficulty(EEnemyDifficulty NewDifficulty)
{
	DifficultyLevel = NewDifficulty;
	Difficulty = UIronvowPresetLibrary::GetDifficultyPreset(NewDifficulty);

	if (AIronvowCharacterBase* Combatant = GetCombatant())
	{
		Combatant->SetSpeedScale(Difficulty.SpeedScale);
	}
}

void AIronvowAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bUsingBehaviorTree)
	{
		return;
	}

	UpdateNativeCombat(DeltaSeconds);
}

AActor* AIronvowAIController::GetCombatTarget() const
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	return PlayerPawn;
}

AIronvowCharacterBase* AIronvowAIController::GetCombatant() const
{
	return Cast<AIronvowCharacterBase>(GetPawn());
}

void AIronvowAIController::UpdateNativeCombat(float DeltaSeconds)
{
	AIronvowCharacterBase* Me = GetCombatant();
	if (!Me || !Me->IsAlive())
	{
		return;
	}

	if (Me->IsRagdolled())
	{
		bAttackQueued = false;
		EnterState(EAICombatState::Downed);
		return;
	}

	AActor* Target = GetCombatTarget();
	if (!Target)
	{
		EnterState(EAICombatState::Circling);
		return;
	}

	StateElapsed += DeltaSeconds;
	AttackCooldown = FMath::Max(0.f, AttackCooldown - DeltaSeconds);
	ReactionCooldown = FMath::Max(0.f, ReactionCooldown - DeltaSeconds);
	DecisionCooldown = FMath::Max(0.f, DecisionCooldown - DeltaSeconds);

	const UWeaponDataAsset* WeaponData = Me->GetWeapon() ? Me->GetWeapon()->GetWeaponData() : nullptr;
	const float Reach = WeaponData ? WeaponData->LengthCm : 120.f;
	const float DesiredRange = FMath::Clamp(Reach * 0.8f, 90.f, 260.f);
	const float Distance = Me->GetDistanceToTarget(Target);
	const float StaminaRatio = Me->GetStaminaComponent() ? Me->GetStaminaComponent()->GetStaminaRatio() : 1.f;

	// 1. Too tired to fight: break off and recover.
	if (StaminaRatio < Difficulty.RetreatStaminaRatio && State != EAICombatState::Attacking)
	{
		EnterState(EAICombatState::Retreating);
	}

	// 2. Legs broken or been knocked down recently: guard up and buy time.
	if (Me->GetHealthRatio() < 0.25f && StaminaRatio < 0.4f)
	{
		Me->SetGuardRaised(true);
	}

	switch (State)
	{
	case EAICombatState::Retreating:
	{
		Me->SetGuardRaised(true);
		MoveAwayFrom(Target, DesiredRange * 2.f);

		if (StaminaRatio > Difficulty.RetreatStaminaRatio + 0.25f)
		{
			EnterState(EAICombatState::Circling);
		}
		break;
	}

	case EAICombatState::Closing:
	{
		Me->SetGuardRaised(IsThreatenedBy(Target));
		MoveToActor(Target, DesiredRange);

		if (Distance <= DesiredRange * 1.1f)
		{
			EnterState(EAICombatState::Circling);
		}
		break;
	}

	case EAICombatState::Attacking:
	{
		Me->SetGuardRaised(false);

		if (StateElapsed > 1.2f)
		{
			AttackCooldown = FMath::Lerp(2.6f, 0.9f, Difficulty.Aggression);
			EnterState(EAICombatState::Circling);
		}
		break;
	}

	case EAICombatState::Guarding:
	{
		Me->SetGuardRaised(true);

		if (StateElapsed > 0.6f && FMath::FRand() < Difficulty.Aggression)
		{
			EnterState(EAICombatState::Circling);
		}
		break;
	}

	case EAICombatState::Downed:
		Me->SetGuardRaised(false);
		EnterState(EAICombatState::Circling);
		break;

	case EAICombatState::Circling:
	default:
	{
		if (IsThreatenedBy(Target) && FMath::FRand() < Difficulty.BlockChance)
		{
			Me->SetGuardRaised(true);
		}
		else
		{
			Me->SetGuardRaised(StateElapsed < 0.4f);
		}

		if (Distance > Reach * 0.92f)
		{
			if (DecisionCooldown <= 0.f)
			{
				DecisionCooldown = FMath::Lerp(1.1f, 0.35f, Difficulty.Aggression);
				if (Distance > DesiredRange * 1.8f)
				{
					EnterState(EAICombatState::Closing);
				}
				else
				{
					StrafeAround(Target, Distance, DeltaSeconds);
				}
			}
			else
			{
				StrafeAround(Target, Distance, DeltaSeconds);
			}
		}

		const bool bInRange = Distance <= Reach * 0.95f;
		if (bInRange && AttackCooldown <= 0.f && !bAttackQueued
			&& FMath::FRand() < FMath::Clamp(Difficulty.Aggression, 0.1f, 0.95f))
		{
			// Reaction time is the delay between deciding and committing.
			bAttackQueued = true;
			ReactionCooldown = Difficulty.ReactionTime;
			Me->SetGuardRaised(true);
		}

		if (bAttackQueued)
		{
			if (!bInRange)
			{
				bAttackQueued = false;
			}
			else if (ReactionCooldown <= 0.f)
			{
				bAttackQueued = false;
				Me->AIStartAttack(Target);
				EnterState(EAICombatState::Attacking);
			}
		}
		break;
	}
	}

	UpdateAimPoint(DeltaSeconds);
}

void AIronvowAIController::UpdateAimPoint(float DeltaSeconds)
{
	AIronvowCharacterBase* Me = GetCombatant();
	AActor* Target = GetCombatTarget();
	if (!Me || !Target)
	{
		return;
	}

	AimErrorRefresh -= DeltaSeconds;
	if (AimErrorRefresh <= 0.f)
	{
		AimErrorRefresh = 0.85f;
		CurrentAimError = FMath::VRand() * Difficulty.AimErrorCm;
	}

	if (Me->GetWeaponControl())
	{
		const FVector AimPoint = Target->GetActorLocation()
			+ FVector(0.f, 0.f, 15.f)
			+ CurrentAimError;

		Me->SetWeaponAimTarget(AimPoint);
	}
}

bool AIronvowAIController::IsThreatenedBy(const AActor* Target) const
{
	const AIronvowCharacterBase* Me = GetCombatant();
	const AIronvowCharacterBase* Enemy = Cast<AIronvowCharacterBase>(Target);
	if (!Me || !Enemy)
	{
		return false;
	}

	const AWeaponBase* EnemyWeapon = Enemy->GetWeapon();
	if (!EnemyWeapon || !EnemyWeapon->IsHeld())
	{
		return false;
	}

	const float Distance = Me->GetDistanceToTarget(Target);
	const float Reach = Enemy->GetReach() + 120.f;
	const float TipSpeed = EnemyWeapon->GetTipVelocity().Size();

	return Distance < Reach && TipSpeed > 500.f;
}

void AIronvowAIController::MoveAwayFrom(const AActor* Target, float Distance)
{
	if (!Target)
	{
		return;
	}

	const FVector Away = (GetPawn()->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
	const FVector Destination = GetPawn()->GetActorLocation() + Away * FMath::Max(120.f, Distance);
	MoveToLocation(Destination, 60.f);
}

void AIronvowAIController::StrafeAround(const AActor* Target, float Radius, float DeltaSeconds)
{
	if (!Target)
	{
		return;
	}

	// Change direction occasionally so the AI does not orbit forever.
	if (FMath::FRand() < 0.004f)
	{
		StrafeDirection *= -1.f;
	}

	const FVector ToTarget = (Target->GetActorLocation() - GetPawn()->GetActorLocation()).GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, ToTarget) * StrafeDirection;
	const FVector Destination = Target->GetActorLocation() - ToTarget * FMath::Max(80.f, Radius * 0.85f) + Right * 90.f;

	MoveToLocation(Destination, 50.f);
}

void AIronvowAIController::EnterState(EAICombatState NewState)
{
	if (State == NewState)
	{
		return;
	}

	State = NewState;
	StateElapsed = 0.f;

	AIronvowCharacterBase* Me = GetCombatant();
	if (!Me)
	{
		return;
	}

	if (NewState == EAICombatState::Retreating)
	{
		Me->StopSprint();
	}
	else if (NewState == EAICombatState::Closing)
	{
		Me->StartSprint();
	}
	else
	{
		Me->StopSprint();
	}
}

void AIronvowAIController::ForceState(EAICombatState NewState)
{
	EnterState(NewState);
	StateElapsed = 0.f;
}
