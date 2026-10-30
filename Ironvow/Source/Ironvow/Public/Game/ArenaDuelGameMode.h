// 1v1 arena duel with rounds.
#pragma once

#include "CoreMinimal.h"
#include "Game/IronvowGameModeBase.h"
#include "ArenaDuelGameMode.generated.h"

UCLASS()
class IRONVOW_API AArenaDuelGameMode : public AIronvowGameModeBase
{
	GENERATED_BODY()

public:
	AArenaDuelGameMode();

	virtual void StartMatch() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duel", meta = (ClampMin = "1"))
	int32 RoundsToWin = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Duel", meta = (ClampMin = "0.1"))
	float RoundIntermission = 2.2f;

	UPROPERTY(BlueprintReadOnly, Category = "Duel")
	int32 PlayerScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Duel")
	int32 EnemyScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Duel")
	int32 RoundNumber = 0;

	UFUNCTION(BlueprintCallable, Category = "Duel")
	void StartRound();

	UFUNCTION(BlueprintCallable, Category = "Duel")
	void RespawnOpponent();

	UFUNCTION(BlueprintPure, Category = "Duel")
	bool IsMatchOver() const;

protected:
	virtual void HandleEnemyKilled(AIronvowCharacterBase* Enemy, AActor* Killer) override;
	virtual void HandlePlayerKilled(AIronvowCharacterBase* Player, AActor* Killer) override;

	void ScheduleNextRound();

	FTimerHandle RoundTimer;
};
