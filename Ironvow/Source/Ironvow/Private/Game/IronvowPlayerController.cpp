#include "Game/IronvowPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Characters/IronvowPlayerCharacter.h"
#include "Game/IronvowGameModeBase.h"
#include "Game/SandboxGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UI/IronvowHUDWidget.h"
#include "UI/IronvowMenuWidget.h"

AIronvowPlayerController::AIronvowPlayerController()
{
	bShowMouseCursor = false;
}

void AIronvowPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	if (HUDWidgetClass)
	{
		HUDWidget = CreateWidget<UIronvowHUDWidget>(this, HUDWidgetClass);
		if (HUDWidget)
		{
			HUDWidget->AddToViewport(0);
		}
	}

	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
}

void AIronvowPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent)
	{
		InputComponent->BindAction(TEXT("Pause"), IE_Pressed, this, &AIronvowPlayerController::TogglePauseMenu);
	}
}

void AIronvowPlayerController::TogglePauseMenu()
{
	if (UGameplayStatics::IsGamePaused(this))
	{
		ResumeGame();
	}
	else
	{
		ShowPauseMenu(true);
	}
}

void AIronvowPlayerController::ShowPauseMenu(bool bShow)
{
	if (bShow)
	{
		if (!PauseMenuWidget && PauseMenuClass)
		{
			PauseMenuWidget = CreateWidget<UIronvowMenuWidget>(this, PauseMenuClass);
		}

		if (PauseMenuWidget && !PauseMenuWidget->IsInViewport())
		{
			PauseMenuWidget->AddToViewport(10);
		}

		SetPause(true);
		SetInputMode(FInputModeGameAndUI().SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
		bShowMouseCursor = true;
		return;
	}

	if (PauseMenuWidget)
	{
		PauseMenuWidget->RemoveFromParent();
	}

	SetPause(false);
	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
}

void AIronvowPlayerController::ResumeGame()
{
	ShowPauseMenu(false);
}

void AIronvowPlayerController::RestartDuel()
{
	ShowPauseMenu(false);

	if (AIronvowGameModeBase* GameMode = Cast<AIronvowGameModeBase>(GetWorld()->GetAuthGameMode()))
	{
		GameMode->RestartMatchAfterDelay(0.15f);
	}
}

void AIronvowPlayerController::QuitToDesktop()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void AIronvowPlayerController::IronvowSpawnEnemy(int32 WeaponIndex)
{
	ASandboxGameMode* Sandbox = Cast<ASandboxGameMode>(GetWorld()->GetAuthGameMode());
	if (!Sandbox)
	{
		ClientMessage(TEXT("IronvowSpawnEnemy only works in the sandbox game mode."));
		return;
	}

	const int32 WeaponCount = static_cast<int32>(EWeaponId::Count);
	const EWeaponId Weapon = static_cast<EWeaponId>(FMath::Clamp(WeaponIndex, 0, WeaponCount - 1));
	const EArmorSetId Armor = static_cast<EArmorSetId>(FMath::Clamp(WeaponIndex, 0, static_cast<int32>(EArmorSetId::Count) - 1));

	Sandbox->SpawnEnemyInFront(Weapon, Armor, EEnemyDifficulty::Duelist);
	ClientMessage(FString::Printf(TEXT("Spawned duelist with weapon %d."), WeaponIndex));
}

void AIronvowPlayerController::IronvowGiveWeapon(int32 WeaponIndex)
{
	ASandboxGameMode* Sandbox = Cast<ASandboxGameMode>(GetWorld()->GetAuthGameMode());
	if (!Sandbox)
	{
		ClientMessage(TEXT("IronvowGiveWeapon only works in the sandbox game mode."));
		return;
	}

	const int32 WeaponCount = static_cast<int32>(EWeaponId::Count);
	Sandbox->GivePlayerWeapon(static_cast<EWeaponId>(FMath::Clamp(WeaponIndex, 0, WeaponCount - 1)));
}

void AIronvowPlayerController::IronvowArmor(int32 ArmorIndex)
{
	ASandboxGameMode* Sandbox = Cast<ASandboxGameMode>(GetWorld()->GetAuthGameMode());
	if (!Sandbox)
	{
		ClientMessage(TEXT("IronvowArmor only works in the sandbox game mode."));
		return;
	}

	const int32 ArmorCount = static_cast<int32>(EArmorSetId::Count);
	Sandbox->SetPlayerArmor(static_cast<EArmorSetId>(FMath::Clamp(ArmorIndex, 0, ArmorCount - 1)));
}

void AIronvowPlayerController::IronvowKillAll()
{
	ASandboxGameMode* Sandbox = Cast<ASandboxGameMode>(GetWorld()->GetAuthGameMode());
	if (Sandbox)
	{
		Sandbox->KillAllEnemies();
	}
}

void AIronvowPlayerController::IronvowSlowMo(float Scale)
{
	if (AIronvowPlayerCharacter* Player = Cast<AIronvowPlayerCharacter>(GetPawn()))
	{
		Player->RequestSlowMotion(FMath::Clamp(Scale, 0.05f, 1.f), 50.f);
		ClientMessage(FString::Printf(TEXT("Time dilation %.2f"), Scale));
	}
}
