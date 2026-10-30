# Ironvow

A physics-driven medieval duelling game for **Unreal Engine 5.4** (C++ core, Blueprint-facing).

Nothing in this project is a canned animation. A fighter's body is a Chaos ragdoll kept upright by
physical animation motors; the weapon is a rigid body constrained to the hand and moved by forces.
A hit only happens when two colliders actually meet, and its damage comes from the mass and the
closing speed of the blade.

> Setting: **the Gray March**, a border duchy where feuds are settled in the ring instead of court.
> All names, places and lore in this project are original.

---

## Requirements

| Item | Value |
| --- | --- |
| Engine | UE 5.4 (5.3 works, `BuildSettingsVersion.V4`) |
| Physics | Chaos (default), substepping enabled in `Config/DefaultEngine.ini` |
| Modules | `Engine`, `PhysicsCore`, `AIModule`, `UMG`, `GameplayTasks`, `NavigationSystem` |
| Assets | Engine basics only; assign any free Mannequin / marketplace meshes you own |
| Target | 60 FPS on a mid-range GPU/CPU (see *Performance* below) |

## Build

```bash
# 1. Open Ironvow.uproject (right-click > Generate Visual Studio project files on Windows)
# 2. Build the "IronvowEditor" target, Development Editor, Win64
# 3. Open the project and create the maps / Blueprints listed in Docs/BLUEPRINT_STEPS.md
```

The C++ side compiles as a single runtime module, `Source/Ironvow`. There are no plugins and no
external dependencies.

## Controls

| Input | Action |
| --- | --- |
| `W A S D` | Walk |
| `Q / E` | Lean (shoulder shift, also shifts the weapon line) |
| `Left Shift` (hold) | Sprint (drains stamina) |
| `Mouse` | Camera; moving the mouse also pushes the weapon tip (aim offset) |
| `Right Mouse` (hold) | Raise guard / brace the weapon |
| `Left Mouse` | Thrust (costs stamina) |
| `H` | Cycle grip: one hand → two hands → half-sword |
| `Space` | Dodge step in the movement direction |
| `Middle Mouse` | Soft lock-on toggle |
| `R` | Try to stand up after a fall |
| `Esc` / `P` | Pause menu |

Sandbox console commands (development builds, `~`): `IronvowSpawnEnemy <0-5>`, `IronvowGiveWeapon <0-5>`,
`IronvowArmor <0-2>`, `IronvowKillAll`, `IronvowSlowMo <scale>`.

## Project layout

```
Ironvow/
├─ Config/                        Input, physics substepping, default game mode
├─ Source/Ironvow/
│  ├─ Public|Private/
│  │  ├─ IronvowTypes.h           Enums + plain data (edges, zones, armour, impact, difficulty)
│  │  ├─ Data/                    WeaponDataAsset, ArmorDataAsset, code-side presets
│  │  ├─ Combat/                  CombatLibrary (damage model), WeaponBase (physics weapon)
│  │  ├─ Components/              ActiveRagdoll, Damage, Stamina, WeaponControl
│  │  ├─ Characters/              CharacterBase, PlayerCharacter, EnemyCharacter
│  │  ├─ AI/                      AIController + Behaviour Tree tasks
│  │  ├─ Game/                    GameModes (duel / survival / sandbox), Arena, PlayerController
│  │  └─ UI/                      HUD and pause-menu C++ bases
└─ Docs/                          Step-by-step build, test and Blueprint instructions
```

## The five C++ pillars

| Class | Responsibility |
| --- | --- |
| `UActiveRagdollComponent` | Physical animation motors, balance scalar, stumbles, full ragdoll, stand-up |
| `AWeaponBase` | Mass/length/balance, physics grip constraints, contact resolution, block/parry |
| `UDamageComponent` | Per-zone health, armour layers, bleeding, death, leg integrity |
| `UStaminaComponent` | Spend/regenerate, exhaustion, power curve that scales every swing |
| `UWeaponControlComponent` | Aim point → tip force PD controller, grips, half-sword, thrust |

Supporting: `UCombatLibrary` (pure damage math), `UIronvowPresetLibrary` (tuning tables so the game
runs before any DataAsset exists), `AIronvowAIController` (native state machine + BT tasks).

## Damage model in one paragraph

`E = ½ m v²` from the weapon's mass and contact speed, normalised against a 108 J reference swing.
Flat contacts lose up to 70 % of their power. Each armour layer multiplies the remaining damage by
`1 − coverage × integrity × (1 − edge effectiveness)`; plate is 10 % effective against a slash but
60 % against blunt, blunt force always leaks 35 % through, and thrusts roll for a gap at the seams.
Zone multipliers are head 2.2, torso 1.0, arms 0.55, legs 0.75. Bleeding scales with damage and edge
type and is applied per second until it clots.

## Performance

- Physics substepping at 8.33 ms with 6 substeps keeps fast blade contacts from tunnelling.
- Ragdolls are only simulated while down; standing bodies are kinematic with motors.
- Only the pelvis, chest and head carry motor constraints; fingers and twist bones are skipped.
- Impact feedback is rate limited to 20 spawns per second per weapon (`NextFeedbackTime`).
- Decals use a fixed pool size and 18 s lifespan.

## Validation status

The source in this repository has **not been compiled**: the machine that produced it has no Unreal
Engine installation. Treat step 1 of `Docs/01_ProjectSetup.md` as the compile gate and fix anything
the compiler reports before moving on. All gameplay tuning numbers are first-pass and are meant to
be edited in the DataAssets.

## Original content

- Weapons: Vale arming sword, Harrowmere longsword, Kellwood bearded axe, Marrow mace, Reedmark
  boar spear, Whisper dagger.
- Harnesses: Homespun of the March, Riveted Byrnie of Harrowmere, Vow Plate.
- Arenas: the Ring of Harrowmere, the Salt Yard of Vellmoor.
- Factions: the Gray March, the Vow, the Ash-Bound.
See `Docs/CONTENT_LORE.md`.
