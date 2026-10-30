#include "Characters/IronvowEnemyCharacter.h"

#include "AI/IronvowAIController.h"
#include "Components/DamageComponent.h"

AIronvowEnemyCharacter::AIronvowEnemyCharacter()
{
	AIControllerClass = AIronvowAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AIronvowEnemyCharacter::BeginPlay()
{
	if (Damage)
	{
		Damage->HealthScale = HealthScale;
	}

	Super::BeginPlay();

	ApplyDifficulty(Difficulty);
}

void AIronvowEnemyCharacter::ApplyHealthScale(float NewHealthScale)
{
	HealthScale = FMath::Clamp(NewHealthScale, 0.25f, 4.f);

	if (Damage)
	{
		Damage->HealthScale = HealthScale;
		Damage->ResetCombatant();
	}
}

void AIronvowEnemyCharacter::ApplyDifficulty(EEnemyDifficulty NewDifficulty)
{
	Difficulty = NewDifficulty;

	if (AIronvowAIController* AIController = Cast<AIronvowAIController>(GetController()))
	{
		AIController->SetDifficulty(NewDifficulty);
	}
}
