# Step 2 — Active ragdoll that stays balanced and falls when hit

## What exists in code

`UActiveRagdollComponent` (`Source/Ironvow/Public/Components/ActiveRagdollComponent.h`) keeps the
skeletal mesh standing with physical animation motors and turns it into a Chaos ragdoll when balance
runs out.

Key behaviour:

- **Motors.** `ApplyPhysicalAnimationProfileBelow(pelvis, "Default")` + `SetStrengthMultiplyer()`.
  Strength blends between `StumbleMotorStrength` (0.22) and `BaseMotorStrength` (1.0) with balance,
  and is further scaled by leg integrity and by `SetMotorStrengthScale` (used by armour / fatigue).
- **Balance scalar.** Each tick the support centre (average of `foot_l`/`foot_r`) is compared with
  the pelvis. Lean beyond `LeanToleranceCm` (14 cm) drains balance at
  `BalanceDrainPerMeter` (0.55/s per metre of excess lean, up to `LeanMaxDistanceCm` / 55 cm), with
  a 2.2× penalty when the legs are damaged. Standing still recovers `0.30/s`.
- **Upright force.** A horizontal force at the pelvis (up to `UprightForce` = 4200 N) pulls the body
  back over its feet. This is the visible "fighting to stay up" feel.
- **Impacts.** `ApplyImpactReaction(Impulse, Bone, Severity)` subtracts
  `ImpactBalanceScale × Severity × legPenalty` from balance, starts a stumble (motor scale ×0.35),
  and applies a real impulse to the hit bone. At zero balance the body drops to full ragdoll and the
  full impulse is applied.
- **Ragdoll on.** Movement disabled, capsule collision off, mesh profile `Ragdoll`, all bodies
  simulate, blend weight 1.
- **Ragdoll off.** Blend weight returns to 0, bodies go kinematic, mesh transform resets to identity,
  capsule snaps back under the pelvis (`SnapCapsuleToBody`), movement resumes.
- **Recovery.** `TryRecover()` costs 28 stamina, is refused below 12 stamina or below 20 % leg
  integrity, and blends physics weight out over 1.7 s (`GetRecoveryAlpha()` shows the progress).

`AIronvowCharacterBase` feeds leg integrity from the damage component every tick and forces a fall
when a leg zone is destroyed. It also retries `TryRecover()` every 1.4 s while down.

## Test

1. PIE in your test map with `BP_IronvowPlayer`.
2. Console: `IronvowSlowMo 0.3` to watch the physics.
3. **Standing:** walk into a wall and keep pushing — the mesh should lean and recover, not slide.
4. **Fall:** console `ce IronvowImpact` is not available yet, so use the debug key added below, or
   call `ApplyImpactReaction` from a Level Blueprint on BeginPlay with
   `Impulse = (1200, 0, 0)`, `Severity = 3`. The body should stumble and go limp.
5. **Recover:** press `R` while down. The body should pull itself up over ~1.7 s and the capsule
   should end where the pelvis is.
6. Verify in `showdebug skeleton` that the pelvis body is still simulating during the fall and that
   motors resume after standing.

## Manual Blueprint steps

1. Open your physics asset → constraint profile **Default**:
   - `Angular Swing 1/2` and `Twist`: `Limited`, 30–45°.
   - `Position Strength` 1000–1500, `Velocity Strength` 100–200, `Max Force` 500–1000 on each axis.
   - Without motor drives, `SetStrengthMultiplyer` has nothing to drive and the body will flop.
2. In `BP_IronvowPlayer` → `ActiveRagdoll` component:
   - Confirm `Root Bone = pelvis`, `Left Foot Bone = foot_l`, `Right Foot Bone = foot_r`
     (rename these if your skeleton uses different names).
3. Create an Anim Blueprint (`ABP_Ironvow`) that just plays `Idle`/`Walk` from the Mannequin, and
   assign it. During ragdoll the mesh ignores the AnimBP, so no ragdoll node is required.
4. Optional debug: add a `Debug Key` (e.g. `K`) in `BP_IronvowPlayer` that calls
   `ActiveRagdoll → Apply Impact Reaction` with a fixed impulse to test falls without a weapon.
5. If the character sinks through the floor on stand-up, raise `RecoverDuration` and check that the
   capsule's collision profile is `Pawn` after `SetRagdollActive(false)`.

## Tuning notes

| Feel | Change |
| --- | --- |
| Too easy to knock down | lower `BalanceDrainPerMeter`, raise `ImpactBalanceScale` threshold by lowering severity |
| Body feels weightless | raise `UprightForce`, raise `BaseMotorStrength` |
| Ragdoll jitters | raise `MaxSubsteps` in `DefaultEngine.ini` or lower `StumbleMotorStrength` |
