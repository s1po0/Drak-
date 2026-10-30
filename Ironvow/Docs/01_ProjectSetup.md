# Step 1 — Project setup and basic movement

## What exists in code

- `Ironvow.uproject` + `Source/Ironvow.Target.cs` / `IronvowEditor.Target.cs` / `Ironvow.Build.cs`.
- `AIronvowCharacterBase` — capsule, Chaos-friendly movement, default component stack
  (`UActiveRagdollComponent`, `UStaminaComponent`, `UDamageComponent`, `UWeaponControlComponent`,
  `UPhysicalAnimationComponent`), walk/sprint speeds, dodge step, loadout functions.
- `AIronvowPlayerCharacter` — spring arm (240 cm, shoulder offset), camera, full legacy input map
  from `Config/DefaultInput.ini`, sprint, guard, thrust, grip cycle, dodge, lock-on, recover.
- `AIronvowPlayerController` — HUD widget hook, pause action, sandbox Exec commands.
- `Config/DefaultEngine.ini` — physics substepping, default game mode.

## Compile gate

1. Install UE 5.4 with C++ support.
2. Right-click `Ironvow.uproject` → *Generate Visual Studio project files*.
3. Open `Ironvow.sln`, target **Development Editor | Win64**, build.
4. Expect the first build to surface any engine-version API drift (see `Docs/07_Polish.md` →
   *API notes*). Fix compile errors before continuing; do not proceed with a broken build.

## Test

1. Open the editor, create a new **Basic** level (or the Third Person template map you will use as a
   sandbox), save it as `/Game/Ironvow/Maps/Arena_Harrowmere`.
2. Project Settings → Maps & Modes → Default GameMode = `ArenaDuelGameMode` (the C++ default already
   points there; the map only needs to exist).
3. Play In Editor (PIE):
   - `WASD` moves, mouse looks.
   - `Left Shift` raises speed to 560 cm/s and drains stamina (watch it in the debugger;
     the HUD widget does not exist until step 6).
   - `Space` launches the capsule forward ~850 cm/s and costs 16 stamina.
   - `Q` / `E` lean the camera and shift the weapon line sideways.
   - No weapon is visible yet if the Mannequin mesh is not assigned — see the Blueprint steps.
4. Console (`~`): `displayall IronvowCharacterBase movement` should list `MaxWalkSpeed`.

## Manual Blueprint / editor steps

1. **Content folders**: create `Content/Ironvow/{Maps,Data,Blueprints,UI,AI}`.
2. **Meshes** (free): import or use the Third Person template's `SKM_Manny`/`SKM_Quinn`, or any
   Epic-provided skeletal mesh. Assing it to the character's `Mesh` component.
3. Create `BP_IronvowPlayer` (parent: `IronvowPlayerCharacter`):
   - Mesh component → Skeletal Mesh = your Mannequin,
     Location `(0, 0, -92)`, Rotation `(0, -90, 0)`.
   - Set `Mesh → Animation Mode = Use Animation Blueprint` (empty AnimBP is fine for now).
   - Set `Weapon Class` = `BP_Longsword` (step 3) or leave empty to get the engine fallback.
4. Create `BP_IronvowPlayerController` (parent: `IronvowPlayerController`) and set it as the game
   mode's `PlayerControllerClass`, or set it per-level in World Settings.
5. **Physics Asset**: open the Mannequin's physics asset, and:
   - Enable `Simulate Physics` on all bodies.
   - Constraint profiles: create a profile named **Default** with all constraints
     `Linear Limit = Locked`, `Angular = Limited`, and enable **Motor drives** on the swing/twist
     axes (`Position Strength ≈ 1000`, `Velocity Strength ≈ 100`, `Max Force ≈ 500`).
   - This profile is what `PhysicalAnimationProfile = "Default"` refers to in the ragdoll component.

## What could break first

- If the Mannequin physics asset has no `Default` profile, step 2 will not hold the body upright.
- Legacy Input mappings require `Config/DefaultInput.ini` to ship with the project (it does).
  If you migrate to Enhanced Input later, replace the `BindAxis`/`BindAction` calls in
  `AIronvowPlayerCharacter::SetupPlayerInputComponent`.
