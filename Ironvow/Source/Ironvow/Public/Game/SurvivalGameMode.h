// Endless waves of duelists.
#pragma once

#include "CoreMinimal.h"
#include "Game/IronvowGameModeBase.h"
#include "SurvivalGameMode.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaveChanged, int32, WaveNumber);

USTRUCT(BlueprintType)
struct FWaveDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "1"))
	int32 Count = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	EWeaponId Weapon = EWeaponId::ArmingSword;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	EArmorSetId Armor = EArmorSetId::Homespun;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	EEnemyDifficulty Difficulty = EEnemyDifficulty::Duelist;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0.25", ClampMax = "4.0"))
	float HealthScale = 1.f;
};

UCLASS()
class IRONVOW_API ASurvivalGameMode : public AIronvowGameModeBase
{
	GENERATED_BODY()

public:
	ASurvivalGameMode();

	virtual void StartMatch() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Survival")
	TArray<FWaveDefinition> Waves;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Survival", meta = (ClampMin = "0.5"))
	float IntermissionSeconds = 4.f;

	UPROPERTY(BlueprintReadOnly, Category = "Survival")
	int32 CurrentWave = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Survival")
	int32 Kills = 0;

	UPROPERTY(BlueprintAssignable, Category = "Survival")
	FOnWaveChanged OnWaveStarted;

	UPROPERTY(BlueprintAssignable, Category = "Survival")
	FOnWaveChanged OnWaveCleared;

	UFUNCTION(BlueprintCallable, Category = "Survival")
	void StartNextWave();

	UFUNCTION(BlueprintPure, Category = "Survival")
	FWaveDefinition GetWaveDefinition(int32 WaveIndex) const;

protected:
	virtual void HandleEnemyKilled(AIronvowCharacterBase* Enemy, AActor* Killer) override;

	void SpawnWave(const FWaveDefinition& Wave);
	void CheckWaveCleared();

	FTimerHandle WaveTimer;
	int32 PendingSpawns = 0;
};
