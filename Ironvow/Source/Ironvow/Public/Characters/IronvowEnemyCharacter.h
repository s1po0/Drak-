// AI duelist. Uses the same body and weapon as the player.
#pragma once

#include "CoreMinimal.h"
#include "Characters/IronvowCharacterBase.h"
#include "IronvowEnemyCharacter.generated.h"

UCLASS()
class IRONVOW_API AIronvowEnemyCharacter : public AIronvowCharacterBase
{
	GENERATED_BODY()

public:
	AIronvowEnemyCharacter();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ironvow|AI")
	FText DuelistName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ironvow|AI")
	EEnemyDifficulty Difficulty = EEnemyDifficulty::Duelist;

	/** Scales zone health on spawn, used by later survival waves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ironvow|AI", meta = (ClampMin = "0.25", ClampMax = "4.0"))
	float HealthScale = 1.f;

	UFUNCTION(BlueprintCallable, Category = "Ironvow|AI")
	void ApplyDifficulty(EEnemyDifficulty NewDifficulty);

	/** Rescales the per-zone health pool and refills it. */
	UFUNCTION(BlueprintCallable, Category = "Ironvow|AI")
	void ApplyHealthScale(float NewHealthScale);

protected:
	virtual void BeginPlay() override;
};
