#include "Game/IronvowGameModeBase.h"

#include "Characters/IronvowCharacterBase.h"
#include "Characters/IronvowEnemyCharacter.h"
#include "Characters/IronvowPlayerCharacter.h"
#include "EngineUtils.h"
#include "Game/IronvowArena.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AIronvowGameModeBase::AIronvowGameModeBase()
{
	DefaultPawnClass = AIronvowPlayerCharacter::StaticClass();
	EnemyClass = AIronvowEnemyCharacter::StaticClass();
}

void AIronvowGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	if (AIronvowPlayerCharacter* Player = GetPlayerDuelist())
	{
		Player->OnDeath.AddDynamic(this, &AIronvowGameModeBase::HandlePlayerDeath);
	}

	ApplyPlayerLoadout();
}

void AIronvowGameModeBase::ApplyPlayerLoadout()
{
	if (!bOverridePlayerLoadout)
	{
		return;
	}

	if (AIronvowPlayerCharacter* Player = GetPlayerDuelist())
	{
		Player->EquipWeapon(PlayerWeapon);
		Player->EquipArmor(PlayerArmor);
	}
}

void AIronvowGameModeBase::StartMatch()
{
	OnMatchMessage.Broadcast(FText::FromString(TEXT("The duel begins.")));
}

void AIronvowGameModeBase::HandleCombatantDeath(AIronvowCharacterBase* Combatant, AActor* Killer)
{
	if (!Combatant)
	{
		return;
	}

	const bool bWasPlayer = Combatant->IsA(AIronvowPlayerCharacter::StaticClass());
	OnDuelistKilled.Broadcast(Combatant, Killer);

	if (bWasPlayer)
	{
		HandlePlayerKilled(Combatant, Killer);
	}
	else
	{
		Combatants.RemoveAll([Combatant](const TWeakObjectPtr<AIronvowCharacterBase>& Entry)
		{
			return Entry.Get() == Combatant;
		});
		HandleEnemyKilled(Combatant, Killer);
	}
}

void AIronvowGameModeBase::HandleEnemyKilled(AIronvowCharacterBase* Enemy, AActor* Killer)
{
	RestartMatchAfterDelay(3.f);
}

void AIronvowGameModeBase::HandlePlayerKilled(AIronvowCharacterBase* Player, AActor* Killer)
{
	RestartMatchAfterDelay(2.5f);
}

void AIronvowGameModeBase::HandlePlayerDeath(AActor* Killer)
{
	OnMatchMessage.Broadcast(FText::FromString(TEXT("You fall. The March remembers.")));
}

void AIronvowGameModeBase::RestartMatchAfterDelay(float Delay)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FTimerDelegate RestartDelegate;
	RestartDelegate.BindWeakLambda(this, [this]()
	{
		UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
	});

	World->GetTimerManager().ClearTimer(RestartTimer);
	World->GetTimerManager().SetTimer(RestartTimer, RestartDelegate, FMath::Max(0.1f, Delay), false);
}

AIronvowEnemyCharacter* AIronvowGameModeBase::SpawnDuelist(EWeaponId WeaponId, EArmorSetId ArmorId, EEnemyDifficulty Difficulty, const FTransform& SpawnTransform, float HealthScale)
{
	UClass* ClassToSpawn = EnemyClass ? EnemyClass.Get() : AIronvowEnemyCharacter::StaticClass();
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AIronvowEnemyCharacter* Enemy = GetWorld()->SpawnActor<AIronvowEnemyCharacter>(
		ClassToSpawn, SpawnTransform.GetLocation(), SpawnTransform.Rotator(), SpawnParams);

	if (!Enemy)
	{
		return nullptr;
	}

	Enemy->StartingWeapon = WeaponId;
	Enemy->StartingArmor = ArmorId;
	Enemy->Difficulty = Difficulty;

	// BeginPlay already ran on spawn, so re-apply the loadout explicitly.
	Enemy->EquipWeapon(WeaponId);
	Enemy->EquipArmor(ArmorId);
	Enemy->ApplyHealthScale(HealthScale);
	Enemy->ApplyDifficulty(Difficulty);

	Combatants.Add(Enemy);
	return Enemy;
}

int32 AIronvowGameModeBase::GetAliveEnemyCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AIronvowCharacterBase>& Entry : Combatants)
	{
		if (Entry.IsValid() && Entry->IsAlive() && !Entry->IsA(AIronvowPlayerCharacter::StaticClass()))
		{
			++Count;
		}
	}
	return Count;
}

AIronvowArena* AIronvowGameModeBase::GetArena() const
{
	for (TActorIterator<AIronvowArena> It(GetWorld()); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

FTransform AIronvowGameModeBase::FindSpawnTransform(int32 Index, APawn* ReferencePawn) const
{
	if (const AIronvowArena* Arena = GetArena())
	{
		if (Arena->HasSpawnPoints())
		{
			return Arena->GetSpawnTransform(Index);
		}
	}

	const FVector Origin = ReferencePawn ? ReferencePawn->GetActorLocation() : FVector::ZeroVector;
	const FVector Offset = FVector(0.f, SpawnSpacingCm * (Index + 1), 0.f);
	FRotator Facing = FRotator::ZeroRotator;

	if (ReferencePawn)
	{
		Facing = (Origin - (Origin + Offset)).Rotation();
	}

	return FTransform(Facing, Origin + Offset + FVector(0.f, 0.f, 100.f));
}

AIronvowPlayerCharacter* AIronvowGameModeBase::GetPlayerDuelist() const
{
	return Cast<AIronvowPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
}
