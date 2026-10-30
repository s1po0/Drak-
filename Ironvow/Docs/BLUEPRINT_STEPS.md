# Manual Blueprint / editor checklist

Everything below is asset work that cannot be generated from a text repository. Do it in order; each
line matches the step document of the same number.

## Step 1 — setup and movement

- [ ] Create `Content/Ironvow/{Maps,Data,Blueprints,UI,AI}`.
- [ ] `BP_IronvowPlayer` (parent `IronvowPlayerCharacter`), assign Mannequin mesh + empty AnimBP,
      mesh offset `(0, 0, -92)`, rotation `(0, -90, 0)`.
- [ ] `BP_IronvowPlayerController` (parent `IronvowPlayerController`) and assign it in World Settings
      or the game mode.
- [ ] Physics asset: bodies simulate, constraint profile **Default** with motor drives enabled.
- [ ] Create the two maps (`Arena_Harrowmere`, `Arena_Vellmoor`) with a floor, light, `PlayerStart`,
      `NavMeshBoundsVolume` and an `IronvowArena` actor.

## Step 2 — active ragdoll

- [ ] Confirm bone names `pelvis`, `foot_l`, `foot_r`, `head`, `spine_02`, `hand_l`, `hand_r`, or
      update the component fields.
- [ ] Physics asset motor drives tuned (position ~1000, velocity ~100, max force ~500).
- [ ] Optional debug key calling `ApplyImpactReaction`.

## Step 3 — weapons

- [ ] `BP_ArmingSword`, `BP_Longsword`, `BP_Axe`, `BP_Mace`, `BP_Spear`, `BP_Dagger`
      (parent `WeaponBase`), each with `Preset Id` and/or `Weapon Data`.
- [ ] Add sockets `Grip_Main`, `Grip_Offhand`, `Grip_HalfSword`, `Tip` to each weapon mesh (optional
      if you accept the default offsets).
- [ ] Assign `Weapon Class` on `BP_IronvowPlayer` and on `BP_IronvowEnemy`.

## Step 4 — damage, armour, stamina

- [ ] Six `WeaponDataAsset` assets in `Content/Ironvow/Data/Weapons` (values in `CONTENT_LORE.md`).
- [ ] Three `ArmorDataAsset` assets in `Content/Ironvow/Data/Armour`.
- [ ] Per-weapon `Feedback` sounds and the blood decal material.
- [ ] Optional: bind `OnZoneDamaged` for hit markers.

## Step 5 — AI

- [ ] `BP_IronvowEnemy` (parent `IronvowEnemyCharacter`) with mesh, AnimBP and weapon class.
- [ ] Nav mesh built in the arena.
- [ ] Optional Behaviour Tree: `BB_Ironvow` (`Target`, `Distance`, `Stamina`), `BT_IronvowDuel`
      using the four C++ nodes, assigned to `BP_IronvowEnemy`.

## Step 6 — arena, UI, modes

- [ ] `WBP_HUD` (parent `IronvowHUDWidget`), implement `OnHUDUpdated`, `OnMessageReceived`,
      `OnBleedingChanged`.
- [ ] `WBP_PauseMenu` (parent `IronvowMenuWidget`), three buttons.
- [ ] `WBP_MainMenu` (parent **`IronvowMainMenuWidget`**) with Duel / Survival / Sandbox buttons and
      the three map names set.
- [ ] `BP_ArenaDuelGameMode`, `BP_SurvivalGameMode`, `BP_SandboxGameMode` if you want designer defaults
      (weapon, armour, difficulty, waves); set `HUD Widget Class` / `Pause Menu Class` on the
      player controller Blueprint instead of per mode.
- [ ] Two arenas: duplicate the Harrowmere map, rename to Vellmoor, adjust bounds and lighting.

## Step 7 — polish

- [ ] Metal / flesh / wood impact sounds.
- [ ] Blood decal material.
- [ ] Optional Niagara spark on `On Weapon Parried`.
- [ ] Tune `Hit Stop Dilation`, `Hit Stop Duration`, `Heavy Hit Threshold` on `BP_IronvowPlayer`.

## Verified automatically

| Check | Status |
| --- | --- |
| C++ compiles against UE 5.4 | **not verified** (no engine in the authoring environment) |
| Balanced braces / includes / generated.h | checked by script |
| Class ↔ method declaration consistency | checked by script |
| Gameplay feel | requires PIE in the editor |
