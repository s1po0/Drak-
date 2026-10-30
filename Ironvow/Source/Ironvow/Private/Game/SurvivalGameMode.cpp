#include "Game/SurvivalGameMode.h"

#include "Characters/IronvowCharacterBase.h"
#include "Characters/IronvowEnemyCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ASurvivalGameMode::ASurvivalGameMode()
{
	DefaultEnemyWeapon = EWeaponId::ArmingSword;
	DefaultEnemyArmor = EArmorSetId::Homespun;
}

void ASurvivalGameMode::StartMatch()
{
	Super::StartMatch();

	CurrentWave = 0;
	Kills = 0;
	StartNextWave();
}

FWaveDefinition ASurvivalGameMode::GetWaveDefinition(int32 WaveIndex) const
{
	if (Waves.IsValidIndex(WaveIndex))
	{
		return Waves[WaveIndex];
	}

	// Procedural fallback so survival works before any designer tuning.
	FWaveDefinition Wave;
	Wave.Count = FMath::Clamp(1 + WaveIndex / 2, 1, 4);
	Wave.HealthScale = 1.f + WaveIndex * 0.08f;
	Wave.Difficulty = WaveIndex >= 6
		? EEnemyDifficulty::Vowbreaker
		: (WaveIndex >= 3 ? EEnemyDifficulty::Duelist : EEnemyDifficulty::Novice);

	const int32 WeaponIndex = WaveIndex % static_cast<int32>(EWeaponId::Count);
	Wave.Weapon = static_cast<EWeaponId>(WeaponIndex);
	Wave.Armor = static_cast<EArmorSetId>(FMath::Min(WaveIndex / 3, static_cast<int32>(EArmorSetId::Count) - 1));
	return Wave;
}

void ASurvivalGameMode::StartNextWave()
{
	const FWaveDefinition Wave = GetWaveDefinition(CurrentWave);
	++CurrentWave;

	OnWaveStarted.Broadcast(CurrentWave);
	OnMatchMessage.Broadcast(FText::FromString(FString::Printf(TEXT("Wave %d - %d duelist(s)"), CurrentWave, Wave.Count)));

	SpawnWave(Wave);
}

void ASurvivalGameMode::SpawnWave(const FWaveDefinition& Wave)
{
	PendingSpawns = FMath::Max(0, Wave.Count);

	for (int32 Index = 0; Index < PendingSpawns; ++Index)
	{
		const FTransform SpawnTransform = FindSpawnTransform(Index + 1, UGameplayStatics::GetPlayerPawn(this, 0));
		SpawnDuelist(Wave.Weapon, Wave.Armor, Wave.Difficulty, SpawnTransform, Wave.HealthScale);
	}
}

void ASurvivalGameMode::HandleEnemyKilled(AIronvowCharacterBase* Enemy, AActor* Killer)
{
	++Kills;
	OnMatchMessage.Broadcast(FText::FromString(FString::Printf(TEXT("Duelist down. Kills: %d"), Kills)));

	CheckWaveCleared();
}

void ASurvivalGameMode::CheckWaveCleared()
{
	if (GetAliveEnemyCount() > 0)
	{
		return;
	}

	OnWaveCleared.Broadcast(CurrentWave);
	OnMatchMessage.Broadcast(FText::FromString(TEXT("Wave cleared. Breathe.")));

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(WaveTimer);

	FTimerDelegate NextWaveDelegate;
	NextWaveDelegate.BindWeakLambda(this, [this]()
	{
		StartNextWave();
	});

	World->GetTimerManager().SetTimer(WaveTimer, NextWaveDelegate, FMath::Max(0.5f, IntermissionSeconds), false);
}
