// C++ behaviour-tree nodes that drive the same physics combat API as the player.
#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BTServiceNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "IronvowBTTasks.generated.h"

/** Commits one physical attack at the blackboard target. */
UCLASS()
class IRONVOW_API UBTTask_IronvowAttack : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_IronvowAttack();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetKey;
};

/** Raises or lowers the guard. */
UCLASS()
class IRONVOW_API UBTTask_IronvowGuard : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_IronvowGuard();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

	UPROPERTY(EditAnywhere, Category = "Guard")
	bool bGuardUp = true;
};

/** Backs away from the target, optionally sprinting. */
UCLASS()
class IRONVOW_API UBTTask_IronvowRetreat : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_IronvowRetreat();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

	UPROPERTY(EditAnywhere, Category = "Retreat", meta = (ClampMin = "100.0"))
	float RetreatDistance = 420.f;

	UPROPERTY(EditAnywhere, Category = "Retreat")
	bool bSprint = false;
};

/** Publishes distance and stamina to the blackboard for tree decisions. */
UCLASS()
class IRONVOW_API UBTService_IronvowCombatContext : public UBTServiceNode
{
	GENERATED_BODY()

public:
	UBTService_IronvowCombatContext();

	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual FString GetStaticDescription() const override;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector DistanceKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector StaminaKey;
};
