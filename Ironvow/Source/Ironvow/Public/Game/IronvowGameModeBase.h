// Shared rules for every Ironvow game mode.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "IronvowTypes.h"
#include "IronvowGameModeBase.generated.h"

class APawn;
class AIronvowArena;
class AIronvowCharacterBase;
class AIronvowEnemyCharacter;
class AIronvowPlayerCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDuelistKilled, AIronvowCharacterBase*, Victim, AActor*, Killer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchMessage, FText, Message);

UCLASS(Abstract)
class IRONVOW_API AIronvowGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	AIronvowGameModeBase();

	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ironvow|Spawn")
	TSubclassOf<AIronvowEnemyCharacter> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ironvow|Spawn")
	EEnemyDifficulty DefaultDifficulty = EEnemyDifficulty::Duelist;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ironvow|Spawn")
	EWeaponId DefaultEnemyWeapon = EWeaponId::Longsword;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ironvow|Spawn")
	EArmorSetId DefaultEnemyArmor = EArmorSetId::Byrnie;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ironvow|Spawn")
	bool bOverridePlayerLoadout = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ironvow|Spawn")
	EWeaponId PlayerWeapon = EWeaponId::Longsword;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ironvow|Spawn")
	EArmorSetId PlayerArmor = EArmorSetId::Byrnie;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ironvow|Spawn", meta = (ClampMin = "1.0", ClampMax = "20.0"))
	float SpawnSpacingCm = 380.f;

	UPROPERTY(BlueprintAssignable, Category = "Ironvow")
	FOnDuelistKilled OnDuelistKilled;

	UPROPERTY(BlueprintAssignable, Category = "Ironvow")
	FOnMatchMessage OnMatchMessage;

	/** Called by every combatant when it dies. */
	UFUNCTION(BlueprintCallable, Category = "Ironvow")
	virtual void HandleCombatantDeath(AIronvowCharacterBase* Combatant, AActor* Killer);

	UFUNCTION(BlueprintCallable, Category = "Ironvow")
	AIronvowEnemyCharacter* SpawnDuelist(EWeaponId WeaponId, EArmorSetId ArmorId, EEnemyDifficulty Difficulty, const FTransform& SpawnTransform, float HealthScale = 1.f);

	UFUNCTION(BlueprintCallable, Category = "Ironvow")
	void RestartMatchAfterDelay(float Delay);

	UFUNCTION(BlueprintPure, Category = "Ironvow")
	int32 GetAliveEnemyCount() const;

	UFUNCTION(BlueprintPure, Category = "Ironvow")
	AIronvowArena* GetArena() const;

	UFUNCTION(BlueprintPure, Category = "Ironvow")
	FTransform FindSpawnTransform(int32 Index, APawn* ReferencePawn = nullptr) const;

	/** Starts the mode. Overridden by duel / survival / sandbox. */
	UFUNCTION(BlueprintCallable, Category = "Ironvow")
	virtual void StartMatch();

protected:
	UFUNCTION()
	void HandlePlayerDeath(AActor* Killer);

	virtual void HandleEnemyKilled(AIronvowCharacterBase* Enemy, AActor* Killer);
	virtual void HandlePlayerKilled(AIronvowCharacterBase* Player, AActor* Killer);

	AIronvowPlayerCharacter* GetPlayerDuelist() const;
	void ApplyPlayerLoadout();

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AIronvowCharacterBase>> Combatants;

	FTimerHandle RestartTimer;
};
