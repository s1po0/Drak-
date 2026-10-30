# Step 4 — Damage, armour and stamina

## What exists in code

### Impact resolution

`AWeaponBase::HandleWeaponHit` triggers on real contact, ignores the wielder, filters slow taps
(`MinImpactSpeed = 220 cm/s`), rate-limits repeats per victim (`HitCooldown = 0.22 s`), and builds a
`FWeaponImpactData` with:

- measured contact speed at the impact point,
- the weapon's real mass,
- edge type from the data asset,
- **flatness** — dot(blade normal, surface normal): a flat hit loses up to 70 % of its power,
- the wielder's stamina power scale.

### Damage maths (`UCombatLibrary`)

```
E        = 0.5 * mass * (speed_cm_s / 100)^2          joules
scale    = clamp(sqrt(E / 108), 0.05, 3)              108 J = committed longsword cut
raw      = BaseDamage * scale * edgeFactor * flatnessLoss * staminaScale * zoneMultiplier
armour   = Π over layers [1 - coverage*integrity*thicknessScale*(1 - edgeEffectiveness)]
final    = raw * armour
```

Edge effectiveness:

| Edge | Cloth | Mail | Plate |
| --- | --- | --- | --- |
| Slash | 0.95 | 0.35 | 0.10 |
| Pierce | 0.90 | 0.55 | 0.30 |
| Blunt | 0.95 | 0.85 | 0.60 |

Blunt damage is floored at 35 % pass-through: a mace hurts through plate. Pierce hits on plate roll
for a gap (22 %, 45 % when half-swording). `EHitResult` reports Glance / Solid / Penetrated /
Deflected from the same numbers.

### `UDamageComponent`

- Six zones (head 60, torso 130, arms 65, legs 85 HP × `HealthScale`).
- Bleeding accumulates per zone, ticks every frame, and clots slowly.
- Armour wear: `UCombatLibrary::ApplyArmorWear` reduces layer integrity (blunt wears armour 2.2×
  faster than pierce). The armour asset is duplicated per combatant at BeginPlay, so two duelists in
  Vow Plate do not share wear.
- Head zone destroyed = instant death; a destroyed leg forces the ragdoll down (`GetLegIntegrity`).
- Delegates: `OnZoneDamaged`, `OnBleedingTick`, `OnArmorLayerBroken`, `OnCombatantDied`.

### `UStaminaComponent`

- 100 max, 11/s regeneration after a 0.9 s delay, 45 % regeneration while exhausted.
- `TryConsume` applies the armour's `StaminaCostMultiplier`.
- `GetPowerScale()` = `0.35 + 0.65 × ratio`; it multiplies swing force and appears in the damage
  formula as `StaminaScale`, so a gassed fighter both swings slower and hits softer.
- Consumed by: swings (tip speed), guard hold, thrusts, sprint, dodge, standing up.

## Test (no weapon mesh needed — the cube fallback works)

1. PIE. Spawn a second character in the level (`BP_IronvowEnemy` once step 5 is done, or drag a
   second `BP_IronvowPlayer` into the map with *Auto Possess Player = Disabled*).
2. Swing into it. Console `showdebug` is off; instead add `Display → Debug → Collision` if you want
   to see contacts, and read the results in `OnZoneDamaged` breakpoints or the log:
   `UE_LOG` is not present by default — use a breakpoint on
   `UDamageComponent::ApplyWeaponImpact`.
3. Verify the armour curve: duplicate `UArmorDataAsset` presets in the content browser, set the same
   weapon against Homespun, Byrnie and Vow Plate. Vow Plate should shrug off slashes and take real
   damage from the mace.
4. Hold guard and get hit by another weapon: `AWeaponBase::ResolveWeaponCollision` drains the
   defender's stamina and pushes both blades apart.
5. Exhaust yourself (swing repeatedly + sprint). Damage and swing speed both fall.

## Manual Blueprint steps

1. Create data assets (right-click → Miscellaneous → Data Asset):
   - `DA_ArmingSword`, `DA_Longsword`, `DA_Axe`, `DA_Mace`, `DA_Spear`, `DA_Dagger`
     (class `WeaponDataAsset`) in `Content/Ironvow/Data/Weapons`.
   - `DA_Homespun`, `DA_Byrnie`, `DA_VowPlate` (class `ArmorDataAsset`) in `Content/Ironvow/Data/Armour`.
   Copy the tables in `Docs/CONTENT_LORE.md` into them, or leave them empty and use the presets.
2. For each armour asset, add six `FHitZoneArmor` entries (head, torso, both arms, both legs) and the
   layer specs listed in the lore document.
3. Bind `OnZoneDamaged` on the character's `Damage` component in `BP_IronvowPlayer` if you want a
   hit-marker or floating damage number; the component already broadcasts the zone, amount and result.
4. Optional: set `Health Scale` per Blueprint for difficulty (the survival mode uses this).

## Known tuning risks

- `GetLegIntegrity() == 0` forces a fall every frame until recovery, so a character with both legs
  destroyed cannot stand. That is intentional, but if you want a crawling state, gate
  `UpdateRagdollIntegrity` on `IsRagdolled()` only.
- Bleeding is capped at 6 HP/s per zone; heavy weapons can still bleed a target out after the fight
  ends — clamp `BleedScale` to 0 for "no bleed-out" duels.
