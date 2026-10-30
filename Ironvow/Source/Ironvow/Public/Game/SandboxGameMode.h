// Free sandbox: spawn duelists and weapons at will.
#pragma once

#include "CoreMinimal.h"
#include "Game/IronvowGameModeBase.h"
#include "SandboxGameMode.generated.h"

class AIronvowEnemyCharacter;

UCLASS()
class IRONVOW_API ASandboxGameMode : public AIronvowGameModeBase
{
	GENERATED_BODY()

public:
	ASandboxGameMode();

	virtual void StartMatch() override;

	/** Puts the next duelist two steps in front of the player. */
	UFUNCTION(BlueprintCallable, Category = "Sandbox")
	AIronvowEnemyCharacter* SpawnEnemyInFront(EWeaponId Weapon, EArmorSetId Armor, EEnemyDifficulty Difficulty, float Distance = 320.f);

	UFUNCTION(BlueprintCallable, Category = "Sandbox")
	void GivePlayerWeapon(EWeaponId Weapon);

	UFUNCTION(BlueprintCallable, Category = "Sandbox")
	void SetPlayerArmor(EArmorSetId Armor);

	UFUNCTION(BlueprintCallable, Category = "Sandbox")
	void ClearEnemies();

	UFUNCTION(BlueprintCallable, Category = "Sandbox")
	void KillAllEnemies();

protected:
	virtual void HandleEnemyKilled(AIronvowCharacterBase* Enemy, AActor* Killer) override;
	virtual void HandlePlayerKilled(AIronvowCharacterBase* Player, AActor* Killer) override;
};
