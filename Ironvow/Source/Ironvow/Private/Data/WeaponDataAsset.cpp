#include "Data/WeaponDataAsset.h"

float UWeaponDataAsset::GetReferenceEnergyJoules() const
{
	// Energy of a committed one-handed cut: 0.5 * mass * (12 m/s)^2.
	constexpr float ReferenceSpeedMps = 12.f;
	return 0.5f * FMath::Max(MassKg, 0.1f) * ReferenceSpeedMps * ReferenceSpeedMps;
}
