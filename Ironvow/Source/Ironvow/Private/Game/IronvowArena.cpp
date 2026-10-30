#include "Game/IronvowArena.h"

#include "Components/BoxComponent.h"

AIronvowArena::AIronvowArena()
{
	PrimaryActorTick.bCanEverTick = false;

	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	RootComponent = Bounds;
	Bounds->SetBoxExtent(FVector(1400.f, 1000.f, 300.f));
	Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bounds->SetGenerateOverlapEvents(false);

	ArenaName = FText::FromString(TEXT("The Ring of Harrowmere"));
	ArenaLore = FText::FromString(TEXT("A ring of packed earth inside a broken curtain wall. Duels here settle debts the courts will not."));

	SpawnPoints.Add(FVector(-320.f, 0.f, 100.f));
	SpawnPoints.Add(FVector(320.f, 0.f, 100.f));
}

FTransform AIronvowArena::GetSpawnTransform(int32 Index) const
{
	const FVector Center = GetArenaCenter();

	if (SpawnPoints.Num() > 0)
	{
		const FVector Local = SpawnPoints[FMath::Abs(Index) % SpawnPoints.Num()];
		const FVector World = GetActorTransform().TransformPosition(Local);
		const FRotator Facing = (Center - World).GetSafeNormal2D().Rotation();
		return FTransform(FRotator(0.f, Facing.Yaw, 0.f), World);
	}

	const FVector Extent = Bounds ? Bounds->GetScaledBoxExtent() : FVector(1400.f, 1000.f, 300.f);
	const float Side = (Index % 2 == 0) ? -1.f : 1.f;
	const FVector World = GetActorTransform().TransformPosition(FVector(Extent.X * 1.35f * Side, 0.f, 100.f));
	const FRotator Facing = (Center - World).GetSafeNormal2D().Rotation();
	return FTransform(FRotator(0.f, Facing.Yaw, 0.f), World);
}

FVector AIronvowArena::GetRandomPointInArena() const
{
	const FVector Extent = Bounds ? Bounds->GetScaledBoxExtent() : FVector(1400.f, 1000.f, 300.f);
	const FVector Local(
		FMath::FRandRange(-Extent.X * 0.8f, Extent.X * 0.8f),
		FMath::FRandRange(-Extent.Y * 0.8f, Extent.Y * 0.8f),
		100.f);

	return GetActorTransform().TransformPosition(Local);
}

bool AIronvowArena::IsInsideArena(const FVector& WorldLocation) const
{
	if (!Bounds)
	{
		return false;
	}

	const FVector Local = Bounds->GetComponentTransform().InverseTransformPosition(WorldLocation);
	const FVector Extent = Bounds->GetScaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X
		&& FMath::Abs(Local.Y) <= Extent.Y
		&& FMath::Abs(Local.Z) <= Extent.Z;
}

FVector AIronvowArena::GetArenaCenter() const
{
	return GetActorLocation();
}
