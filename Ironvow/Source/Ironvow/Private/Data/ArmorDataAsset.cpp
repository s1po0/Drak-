#include "Data/ArmorDataAsset.h"

const FHitZoneArmor* UArmorDataAsset::FindZone(EHitZone Zone) const
{
	return Zones.FindByPredicate([Zone](const FHitZoneArmor& Entry)
	{
		return Entry.Zone == Zone;
	});
}
