// Native combat AI for a physics-armed duelist.
#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "IronvowTypes.h"
#include "IronvowAIController.generated.h"

class AIronvowCharacterBase;
class UBehaviorTree;

/**
 * Runs a small native state machine that drives the same weapon control
 * component as the player. A Behaviour Tree asset can be assigned to override
 * the decisions; the C++ tasks in IronvowBTTasks.h call the same character API.
 */
UCLASS()
class IRONVOW_API AIronvowAIController : public AAIController
{
	GENERATED_BODY()

public:
	AIronvowAIController();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ironvow|AI")
	EEnemyDifficulty DifficultyLevel = EEnemyDifficulty::Duelist;

	/** Optional. When set, the behaviour tree runs instead of the native loop. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ironvow|AI")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset = nullptr;

	UFUNCTION(BlueprintCallable, Category = "Ironvow|AI")
	void SetDifficulty(EEnemyDifficulty NewDifficulty);

	UFUNCTION(BlueprintPure, Category = "Ironvow|AI")
	FAIDifficulty GetDifficulty() const { return Difficulty; }

	UFUNCTION(BlueprintPure, Category = "Ironvow|AI")
	EAICombatState GetCombatState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Ironvow|AI")
	AActor* GetCombatTarget() const;

	UFUNCTION(BlueprintCallable, Category = "Ironvow|AI")
	void ForceState(EAICombatState NewState);

protected:
	void UpdateNativeCombat(float DeltaSeconds);
	void UpdateAimPoint(float DeltaSeconds);
	void EnterState(EAICombatState NewState);
	bool IsThreatenedBy(const AActor* Target) const;
	void MoveAwayFrom(const AActor* Target, float Distance);
	void StrafeAround(const AActor* Target, float Radius, float DeltaSeconds);

	AIronvowCharacterBase* GetCombatant() const;

	UPROPERTY(Transient)
	EAICombatState State = EAICombatState::Circling;

	UPROPERTY(Transient)
	FAIDifficulty Difficulty;

	float AttackCooldown = 0.f;
	float ReactionCooldown = 0.f;
	float DecisionCooldown = 0.f;
	float AimErrorRefresh = 0.f;
	float StrafeDirection = 1.f;
	float StateElapsed = 0.f;
	FVector CurrentAimError = FVector::ZeroVector;
	bool bUsingBehaviorTree = false;

	/** True while the AI is winding up a committed attack. */
	UPROPERTY(Transient)
	bool bAttackQueued = false;
};
