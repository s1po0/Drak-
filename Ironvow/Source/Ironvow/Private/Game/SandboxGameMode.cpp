#include "Game/SandboxGameMode.h"

#include "Characters/IronvowCharacterBase.h"
#include "Characters/IronvowEnemyCharacter.h"
#include "Characters/IronvowPlayerCharacter.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ASandboxGameMode::ASandboxGameMode()
{
	bOverridePlayerLoadout = true;
	PlayerWeapon = EWeaponId::Longsword;
	PlayerArmor = EArmorSetId::Byrnie;
}

void ASandboxGameMode::StartMatch()
{
	Super::StartMatch();
	OnMatchMessage.Broadcast(FText::FromString(TEXT("Sandbox: F1 spawns a duelist, F2 gives the next weapon, F3 cycles armour.")));
}

AIronvowEnemyCharacter* ASandboxGameMode::SpawnEnemyInFront(EWeaponId Weapon, EArmorSetId Armor, EEnemyDifficulty Difficulty, float Distance)
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn)
	{
		return nullptr;
	}

	const FVector Forward = PlayerPawn->GetActorForwardVector();
	const FVector Location = PlayerPawn->GetActorLocation() + Forward * Distance + FVector(0.f, 0.f, 20.f);
	const FRotator Rotation = (-Forward).Rotation();

	return SpawnDuelist(Weapon, Armor, Difficulty, FTransform(Rotation, Location));
}

void ASandboxGameMode::GivePlayerWeapon(EWeaponId Weapon)
{
	if (AIronvowPlayerCharacter* Player = Cast<AIronvowPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Player->EquipWeapon(Weapon);
		OnMatchMessage.Broadcast(FText::FromString(TEXT("Weapon changed.")));
	}
}

void ASandboxGameMode::SetPlayerArmor(EArmorSetId Armor)
{
	if (AIronvowPlayerCharacter* Player = Cast<AIronvowPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Player->EquipArmor(Armor);
		OnMatchMessage.Broadcast(FText::FromString(TEXT("Harness changed.")));
	}
}

void ASandboxGameMode::ClearEnemies()
{
	for (TActorIterator<AIronvowEnemyCharacter> It(GetWorld()); It; ++It)
	{
		It->Destroy();
	}
}

void ASandboxGameMode::KillAllEnemies()
{
	for (TActorIterator<AIronvowEnemyCharacter> It(GetWorld()); It; ++It)
	{
		It->Kill(UGameplayStatics::GetPlayerPawn(this, 0));
	}
}

void ASandboxGameMode::HandleEnemyKilled(AIronvowCharacterBase* Enemy, AActor* Killer)
{
	// Sandbox never restarts the level.
	OnMatchMessage.Broadcast(FText::FromString(TEXT("Duelist down. Spawn another.")));
}

void ASandboxGameMode::HandlePlayerKilled(AIronvowCharacterBase* Player, AActor* Killer)
{
	OnMatchMessage.Broadcast(FText::FromString(TEXT("You died in the sandbox. Respawning.")));

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FTimerDelegate RespawnDelegate;
	RespawnDelegate.BindWeakLambda(this, [this]()
	{
		if (AIronvowPlayerCharacter* PlayerCharacter = Cast<AIronvowPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
		{
			PlayerCharacter->ResetCombatantState();
		}
	});

	FTimerHandle Handled;
	World->GetTimerManager().SetTimer(Handled, RespawnDelegate, 2.2f, false);
}
