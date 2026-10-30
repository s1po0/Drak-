#include "Game/ArenaDuelGameMode.h"

#include "Characters/IronvowCharacterBase.h"
#include "Characters/IronvowEnemyCharacter.h"
#include "Characters/IronvowPlayerCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AArenaDuelGameMode::AArenaDuelGameMode()
{
	DefaultEnemyWeapon = EWeaponId::Longsword;
	DefaultEnemyArmor = EArmorSetId::Byrnie;
}

void AArenaDuelGameMode::StartMatch()
{
	Super::StartMatch();

	PlayerScore = 0;
	EnemyScore = 0;
	RoundNumber = 0;
	StartRound();
}

void AArenaDuelGameMode::StartRound()
{
	if (IsMatchOver())
	{
		OnMatchMessage.Broadcast(FText::FromString(TEXT("The duel is settled. Press restart to fight again.")));
		return;
	}

	++RoundNumber;
	OnMatchMessage.Broadcast(FText::FromString(FString::Printf(TEXT("Round %d"), RoundNumber)));

	RespawnOpponent();
}

void AArenaDuelGameMode::RespawnOpponent()
{
	if (GetAliveEnemyCount() > 0)
	{
		return;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	const FTransform SpawnTransform = FindSpawnTransform(1, PlayerPawn);

	SpawnDuelist(DefaultEnemyWeapon, DefaultEnemyArmor, DefaultDifficulty, SpawnTransform);
}

void AArenaDuelGameMode::HandleEnemyKilled(AIronvowCharacterBase* Enemy, AActor* Killer)
{
	++PlayerScore;
	OnMatchMessage.Broadcast(FText::FromString(FString::Printf(TEXT("Opponent down. %d - %d"), PlayerScore, EnemyScore)));

	if (IsMatchOver())
	{
		OnMatchMessage.Broadcast(FText::FromString(TEXT("You hold the ring.")));
		return;
	}

	ScheduleNextRound();
}

void AArenaDuelGameMode::HandlePlayerKilled(AIronvowCharacterBase* Player, AActor* Killer)
{
	++EnemyScore;
	OnMatchMessage.Broadcast(FText::FromString(FString::Printf(TEXT("You are down. %d - %d"), PlayerScore, EnemyScore)));

	if (IsMatchOver())
	{
		return;
	}

	// The player pawn is respawned by the game mode restart for now.
	RestartMatchAfterDelay(RoundIntermission + 1.2f);
}

void AArenaDuelGameMode::ScheduleNextRound()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(RoundTimer);

	FTimerDelegate NextRoundDelegate;
	NextRoundDelegate.BindWeakLambda(this, [this]()
	{
		if (AIronvowPlayerCharacter* Player = Cast<AIronvowPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
		{
			if (!Player->IsAlive())
			{
				Player->ResetCombatantState();
			}
		}
		StartRound();
	});

	World->GetTimerManager().SetTimer(RoundTimer, NextRoundDelegate, FMath::Max(0.2f, RoundIntermission), false);
}

bool AArenaDuelGameMode::IsMatchOver() const
{
	return PlayerScore >= RoundsToWin || EnemyScore >= RoundsToWin;
}
