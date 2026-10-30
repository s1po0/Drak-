# Step 7 — Polish: sound, VFX, slow motion

## What exists in code

### Slow motion and hit-stop

`AIronvowPlayerCharacter`:

- `RequestSlowMotion(Dilation, Duration)` sets global time dilation and restores it with a timer, so
  overlapping requests do not stack.
- On every landed hit (`HandleLocalWeaponImpact`): camera shake scaled by damage
  (`GetMappedRangeValueClamped` 0→40 dmg onto 0.35→2.2), plus a 0.12× hit-stop for 0.05 s when the
  hit exceeds `HeavyHitThreshold` (14).
- Killing blow: 0.28× for 0.85 s.
- `RequestCameraShake` runs a Perlin-noise-driven additive rotation on the spring arm, decaying
  quadratically.

### Impact feedback

`AWeaponBase::SpawnImpactFeedback` takes an `EWeaponImpactSurface` (Flesh / Metal / Wood):

- **Metal** — weapon-on-weapon and armour hits.
- **Flesh** — body hits, plus the blood decal.
- **Wood** — hits on anything without a damage component (palisades, crates, arena props), plus the
  optional chip decal.

Rate limited to one spawn per 50 ms per weapon.

### Content hooks

`FImpactFeedback` on `UWeaponDataAsset`:

| Field | Use |
| --- | --- |
| `MetalImpactSound` | weapon-to-weapon and armour hits |
| `FleshImpactSound` | body hits |
| `WoodImpactSound` | reserved for shields / arena props |
| `BloodDecalMaterial` | deferred decal at the impact point |
| `WoodChipDecalMaterial` | reserved |
| `DecalSizeCm` / `DecalLifeSpan` | 24 cm, 18 s by default |

`UDamageComponent` also has `OnBleedingTick` for a periodic drip sound/effect.

## Test

1. PIE, land a normal hit: camera shakes, no slow motion.
2. Land a heavy mace hit on the torso: brief hit-stop (0.05 s of 0.12× time), then normal speed.
3. Land a killing blow: 0.85 s at 0.28×, then restore. Press pause twice mid-effect to confirm time
   returns to 1.0 and does not stay slowed.
4. Block a swing with your own weapon: metal ping + both weapons bounce apart + stamina drops.
5. Hit flesh: sound + blood decal appears and fades after 18 s.
6. Strike a palisade / crate (anything without a `DamageComponent`): wood impact sound, no blood.
6. `stat unit` should stay near 16.6 ms with two ragdolls active.

## Manual Blueprint / asset steps

1. **Sounds** — import (or use Engine free assets):
   `S_MetalImpact` (weapon on plate), `S_FleshImpact`, `S_Swing` optional.
2. Assign them to each `WeaponDataAsset → Feedback`.
3. **Decals** — create `M_BloodDecal` from any free blood texture, set *Deferred Decal* material domain,
   `Blend Mode = Translucent`. Assign to `Feedback → Blood Decal Material`.
4. **VFX (optional)** — Niagara sparks on parry: spawn in the weapon Blueprint by binding to
   `On Weapon Parried` (available on `AWeaponBase`), or use a `NiagaraComponent` activated by the
   `OnWeaponImpact` event. C++ deliberately spawns no Niagara systems so no plugin dependency is
   forced.
5. **Hit-stop tuning** — set `Hit Stop Dilation` and `Hit Stop Duration` on `BP_IronvowPlayer`.
   For a heavier feel use 0.05× / 0.07 s.
6. **Camera shake tuning** — `RequestCameraShake(Intensity, Duration)` is BlueprintCallable, so
   custom events (explosions, heavy blocks) can call it directly.
7. **HUD slow-mo cue** — in `WBP_HUD`, listen to the game's time dilation in `OnHUDUpdated` or add a
   border overlay when `IsLockedOn`/heavy hit happens (Blueprint only).

## Not implemented in C++ (asset work or future work)

- **Niagara systems** — C++ exposes the events (`OnWeaponImpact`, `OnWeaponParried`); the effects
  themselves are Blueprint/asset work so no plugin dependency is forced.
- **Anim Blueprint** — the project plays Mannequin locomotion; the ragdoll does not need a ragdoll
  node because the mesh is driven by Chaos while limp.
- **State Tree AI** — the shipped AI is a native state machine plus Behaviour Tree tasks. A State
  Tree would add the `StateTreeModule` / `GameplayStateTreeModule` plugin dependencies; the same
  character API (`AIStartAttack`, `SetGuardRaised`, `SetWeaponAimTarget`, `SetDifficulty`) is ready
  for it.

## API notes (engine-version drift)

Because this project was authored without a compiler in the loop, these are the calls most likely to
need a small adjustment on a given 5.x version:

| Call | Note |
| --- | --- |
| `UPhysicalAnimationComponent::SetStrengthMultiplyer` | Misspelling is intentional; it is the engine API |
| `ApplyPhysicalAnimationProfileBelow` | Returns `void` in 5.x (returned `bool` in some 4.x builds) |
| `SetAngularDriveParams` | Three-argument overload used |
| `USkeletalMeshComponent::SetSkeletalMeshAsset` | 5.1+ name; older builds use `SetSkeletalMesh` |
| `Mesh->SetCenterOfMass` | Exposed via `UPrimitiveComponent`; unchanged across 5.x |
| `FMath::PerlinNoise1D` | Engine math helper, unchanged |

## Performance checklist

- `Config/DefaultEngine.ini` → `bSubstepping=True`, `MaxSubstepDeltaTime=0.008333`, `MaxSubsteps=6`.
- Keep ragdolls simulated only while down; standing bodies use motors.
- Cap `WeaponControlComponent::TickComponent` work: it is O(1) per weapon.
- Decals are small and short-lived; avoid stacking more than ~40 in a scene.
- Use `stat physics` to confirm Chaos stays under ~3 ms for a duel.
