// Arena bounds and spawn points.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IronvowArena.generated.h"

class UBoxComponent;

/**
 * Place one of these in a level per arena. Two shipped arenas are documented in
 * Docs/CONTENT_LORE.md (Harrowmere ring, Vellmoor salt yard); duplicate the
 * level and swap the name / spawn points for the second one.
 */
UCLASS()
class IRONVOW_API AIronvowArena : public AActor
{
	GENERATED_BODY()

public:
	AIronvowArena();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena")
	TObjectPtr<UBoxComponent> Bounds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	FText ArenaName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena", meta = (MultiLine = "true"))
	FText ArenaLore;

	/** Spawn points in actor space. Leave empty to use the bounds' long axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	TArray<FVector> SpawnPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena", meta = (ClampMin = "2"))
	int32 MaxFighters = 2;

	UFUNCTION(BlueprintPure, Category = "Arena")
	bool HasSpawnPoints() const { return SpawnPoints.Num() > 0; }

	UFUNCTION(BlueprintPure, Category = "Arena")
	FTransform GetSpawnTransform(int32 Index) const;

	UFUNCTION(BlueprintPure, Category = "Arena")
	FVector GetRandomPointInArena() const;

	UFUNCTION(BlueprintPure, Category = "Arena")
	bool IsInsideArena(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "Arena")
	FVector GetArenaCenter() const;
};
