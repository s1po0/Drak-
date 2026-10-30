# Step 3 — Weapon physics and mouse control

## What exists in code

### `AWeaponBase`

- The weapon is a `UStaticMeshComponent` root with `SimulatePhysics = true`, mass overridden from
  `UWeaponDataAsset::MassKg`, and `SetCenterOfMass(BalanceOffsetCm)` — a mace really is heavier at
  the head than a dagger is at the point.
- Two `UPhysicsConstraintComponent`s hold it: `MainGripConstraint` (dominant hand) and
  `OffhandGripConstraint` (support hand / half-sword grip). Both lock linear movement, limit angular
  swing to ~42°, and use a SLERP angular drive so the blade can still be pushed by the world.
- `AddTipForce(Force)` applies a force **at the tip**, so torque comes for free from weapon length.
- Grip modes: `OneHanded`, `TwoHanded`, `HalfSword`. Half-sword moves the off hand onto the blade
  socket and raises the main grip stiffness.
- `GetTipVelocity()` uses `GetPhysicsLinearVelocityAtPoint`, so "swing speed" is measured, not
  animated.
- The mesh falls back to `/Engine/BasicShapes/Cube.Cube` scaled to the weapon's length when no mesh
  asset is assigned, so a weapon is always visible and hittable.

### `UWeaponControlComponent`

- A PD controller pulls the tip toward `AimTarget`:
  `F = (Target − Tip) × Stiffness − TipVelocity × Damping`, clamped to
  `WeaponData->TipControlForce × power`.
- Stiffness/damping are per-grip: guard raises stiffness 1.35× and lifts the aim point 34 cm;
  half-sword raises stiffness 1.25× but cuts force to 70 %.
- `CommitThrust()` spends 12 stamina and pushes along the blade axis for 0.22 s.
- Stamina drains continuously from tip speed and from holding guard.
- `SetAimFromScreen()` exists for cursor-based setups; the shipped player uses a camera-relative
  aim point plus a decaying mouse offset (`AIronvowPlayerCharacter::UpdateAimTarget`) because the
  mouse is captured.

## Test

1. PIE with `BP_IronvowPlayer` (weapon class set to `BP_Longsword` or empty for the cube fallback).
2. Move the mouse — the tip should chase the aim point with visible inertia; heavy weapons lag more.
3. Hold `Right Mouse` — the tip rises; the blade should feel braced, not teleported.
4. Press `Left Mouse` — the point drives forward.
5. Press `H` — grip cycles one hand → two hands → half-sword (only for weapons with
   `bSupportsHalfSword`, i.e. the longsword).
6. Console `showdebug` → none; instead open the weapon actor's details during PIE (eject) and watch
   `WeaponMesh → Physics → Linear/Angular velocity` while swinging.
7. `t.MaxFPS 60` and swing continuously: the blade must not tunnel through the dummy (substepping).

## Manual Blueprint steps

1. Create `BP_Longsword` (parent: `AWeaponBase`) → set `Weapon Data` to the longsword DataAsset once
   you create it (step 4 doc), or leave null and set `Preset Id = Longsword`.
2. In the static mesh component:
   - Collision: **Physics Actor** profile (already set in C++), `Simulate Physics = true`.
   - Add sockets if your mesh uses them: `Grip_Main`, `Grip_Offhand`, `Grip_HalfSword`, `Tip`.
   - If you have no sockets, C++ falls back to the component origin and `TipOffset`, which is
     adequate for the cube placeholder but will look wrong on a real mesh.
3. Create `BP_ArmingSword`, `BP_Axe`, `BP_Mace`, `BP_Spear`, `BP_Dagger` the same way with the
   matching `Preset Id`.
4. Assign each blueprint's `Weapon Data` if you want the full authored numbers from
   `Docs/CONTENT_LORE.md` (or rely on `UIronvowPresetLibrary`, which builds the same numbers as
   transient assets at spawn).
5. Ensure the character's mesh has `hand_r` and `hand_l` bones; otherwise change
   `Main Hand Bone` / `Off Hand Bone` in the weapon DataAsset.

## Where the feel comes from

| Feeling | Source |
| --- | --- |
| Heavy weapon lags behind the mouse | `MassKg` + `TipControlForce` in the PD clamp |
| Short weapons swing fast | `LengthCm` reduces tip torque and reach |
| Spear is precise but slow to redirect | long `LengthCm`, `TipControlForce` 1600 |
| Half-sword thrusts land | stiffness ×1.25, force ×0.7, `HalfSwordThrustScale` on damage |
