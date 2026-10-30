#include "AI/IronvowBTTasks.h"

#include "AIController.h"
#include "GameFramework/Actor.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Characters/IronvowCharacterBase.h"
#include "Components/StaminaComponent.h"

// ---------------------------------------------------------------- attack ----

UBTTask_IronvowAttack::UBTTask_IronvowAttack()
{
	NodeName = TEXT("Ironvow Attack");
	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_IronvowAttack, TargetKey), AActor::StaticClass());
}

EBTNodeResult::Type UBTTask_IronvowAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	AIronvowCharacterBase* Combatant = AIController ? Cast<AIronvowCharacterBase>(AIController->GetPawn()) : nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();

	if (!Combatant || !Blackboard)
	{
		return EBTNodeResult::Failed;
	}

	AActor* Target = Cast<AActor>(Blackboard->GetValueAsObject(TargetKey.SelectedKeyName));
	if (!Target)
	{
		return EBTNodeResult::Failed;
	}

	Combatant->AIStartAttack(Target);
	return EBTNodeResult::Succeeded;
}

FString UBTTask_IronvowAttack::GetStaticDescription() const
{
	return FString::Printf(TEXT("Swing the held weapon at %s"), *TargetKey.SelectedKeyName.ToString());
}

// ----------------------------------------------------------------- guard ----

UBTTask_IronvowGuard::UBTTask_IronvowGuard()
{
	NodeName = TEXT("Ironvow Guard");
}

EBTNodeResult::Type UBTTask_IronvowGuard::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	AIronvowCharacterBase* Combatant = AIController ? Cast<AIronvowCharacterBase>(AIController->GetPawn()) : nullptr;
	if (!Combatant)
	{
		return EBTNodeResult::Failed;
	}

	Combatant->SetGuardRaised(bGuardUp);
	return EBTNodeResult::Succeeded;
}

FString UBTTask_IronvowGuard::GetStaticDescription() const
{
	return bGuardUp ? TEXT("Raise guard") : TEXT("Lower guard");
}

// --------------------------------------------------------------- retreat ----

UBTTask_IronvowRetreat::UBTTask_IronvowRetreat()
{
	NodeName = TEXT("Ironvow Retreat");
	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_IronvowRetreat, TargetKey), AActor::StaticClass());
}

EBTNodeResult::Type UBTTask_IronvowRetreat::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AIronvowCharacterBase* Combatant = Cast<AIronvowCharacterBase>(Pawn);

	if (!Pawn || !Blackboard)
	{
		return EBTNodeResult::Failed;
	}

	AActor* Target = Cast<AActor>(Blackboard->GetValueAsObject(TargetKey.SelectedKeyName));
	if (!Target)
	{
		return EBTNodeResult::Failed;
	}

	if (Combatant)
	{
		Combatant->SetGuardRaised(true);
		if (bSprint)
		{
			Combatant->StartSprint();
		}
	}

	const FVector Away = (Pawn->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
	const FVector Destination = Pawn->GetActorLocation() + Away * RetreatDistance;
	AIController->MoveToLocation(Destination, 60.f);

	return EBTNodeResult::Succeeded;
}

EBTNodeResult::Type UBTTask_IronvowRetreat::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (AAIController* AIController = OwnerComp.GetAIOwner())
	{
		if (AIronvowCharacterBase* Combatant = Cast<AIronvowCharacterBase>(AIController->GetPawn()))
		{
			Combatant->StopSprint();
		}
	}
	return EBTNodeResult::Aborted;
}

FString UBTTask_IronvowRetreat::GetStaticDescription() const
{
	return FString::Printf(TEXT("Retreat %.0f cm from %s"), RetreatDistance, *TargetKey.SelectedKeyName.ToString());
}

// --------------------------------------------------------------- service ----

UBTService_IronvowCombatContext::UBTService_IronvowCombatContext()
{
	NodeName = TEXT("Ironvow Combat Context");
	Interval = 0.2f;
	RandomDeviation = 0.05f;

	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_IronvowCombatContext, TargetKey), AActor::StaticClass());
	DistanceKey.AddFloatFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_IronvowCombatContext, DistanceKey));
	StaminaKey.AddFloatFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_IronvowCombatContext, StaminaKey));
}

void UBTService_IronvowCombatContext::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!Blackboard || !AIController)
	{
		return;
	}

	AIronvowCharacterBase* Combatant = Cast<AIronvowCharacterBase>(AIController->GetPawn());
	AActor* Target = Cast<AActor>(Blackboard->GetValueAsObject(TargetKey.SelectedKeyName));

	if (!Target && AIController->GetWorld())
	{
		Target = AIController->GetWorld()->GetFirstPlayerController()
			? AIController->GetWorld()->GetFirstPlayerController()->GetPawn()
			: nullptr;

		if (Target)
		{
			Blackboard->SetValueAsObject(TargetKey.SelectedKeyName, Target);
		}
	}

	if (Combatant && Target)
	{
		Blackboard->SetValueAsFloat(DistanceKey.SelectedKeyName, Combatant->GetDistanceToTarget(Target));
	}

	if (Combatant && Combatant->GetStaminaComponent())
	{
		Blackboard->SetValueAsFloat(StaminaKey.SelectedKeyName, Combatant->GetStaminaComponent()->GetStaminaRatio());
	}
}

FString UBTService_IronvowCombatContext::GetStaticDescription() const
{
	return TEXT("Publishes distance and stamina to the blackboard");
}
