# Step 5 — One working AI enemy

## What exists in code

### `AIronvowEnemyCharacter`

A `AIronvowCharacterBase` that auto-possesses `AIronvowAIController`, exposes
`ApplyDifficulty()` and `HealthScale`, and is spawned by the game mode.

### `AIronvowAIController`

A native state machine that controls **the same physics weapon** as the player (it drives
`UWeaponControlComponent::SetAimTarget` and `AIronvowCharacterBase::AIStartAttack`, never an
animation):

| State | Behaviour |
| --- | --- |
| `Closing` | Runs at the target until inside `reach × 0.8` |
| `Circling` | Strafes around the target, guards when the player's tip is fast and close |
| `Attacking` | Sweeps the aim point across the target over `AIAttackDuration` (0.85 s) and thrusts with piercing weapons |
| `Guarding` | Holds guard, breaks off after a short window |
| `Retreating` | Backs off, guard up, when stamina < `RetreatStaminaRatio` |
| `Downed` | Ragdolled; the character's own recovery timer stands it up |

Difficulty (`FAIDifficulty`, from `UIronvowPresetLibrary::GetDifficultyPreset`) sets reaction time,
aim error, block chance, aggression and retreat threshold:

| Tier | Reaction | Aim error | Block | Aggression |
| --- | --- | --- | --- | --- |
| Novice of the March | 0.75 s | 34 cm | 0.30 | 0.35 |
| March Duelist | 0.42 s | 16 cm | 0.60 | 0.60 |
| Vowbreaker | 0.22 s | 6 cm | 0.80 | 0.80 |

Reaction time is real: the AI decides to attack, waits `ReactionTime` with its guard up, then
commits.

### Behaviour Tree support

`IronvowBTTasks.h` provides C++ nodes for designers who prefer trees:

- `BTTask_IronvowAttack` — commits an attack at the blackboard target.
- `BTTask_IronvowGuard` — raises/lowers guard (`bGuardUp`).
- `BTTask_IronvowRetreat` — backs away (`RetreatDistance`, `bSprint`).
- `BTService_IronvowCombatContext` — publishes distance and stamina to the blackboard.

If `BehaviorTreeAsset` is empty, the native machine runs; if a tree is assigned,
`RunBehaviorTree` takes over and the native loop stands down.

## Test

1. Create `BP_IronvowEnemy` (parent `IronvowEnemyCharacter`), set the Mannequin mesh and the same
   AnimBP as the player.
2. Drag one into the level, set **Auto Possess AI = Placed in World or Spawned** (the C++ default),
   and set `Difficulty = March Duelist`.
3. PIE. Expected, in order:
   - it closes the distance, then circles at blade range;
   - it raises guard when you wind up a fast swing;
   - it attacks in real swings that can be parried by your blade (weapon-to-weapon collision);
   - when its stamina is spent it backs off and guards until it recovers.
4. Set `Difficulty = Vowbreaker` and repeat: attacks should land much more often and aim error should
   be nearly invisible.
5. Knock it down (heavy mace hits). It should lie still, then attempt to stand every 1.4 s, and fail
   while stamina is below 12.
6. Debug the state: select the controller during PIE and watch `State` in the details panel, or call
   `GetCombatState()`.

## Manual Blueprint steps (only if you want the Behaviour Tree)

The native machine is enough for step 5; do this later if you want designers to edit behaviour:

1. Create `BB_Ironvow` (Blackboard) with keys: `Target` (Object, base = Actor),
   `Distance` (Float), `Stamina` (Float).
2. Create `BT_IronvowDuel`:
   - `BTService_IronvowCombatContext` (root service), wire the three keys.
   - Sequence: `BTTask_IronvowGuard (true)` → `Wait 0.3` → `BTTask_IronvowAttack`.
   - Add a decorator `Blackboard: Stamina < 0.25` branch to `BTTask_IronvowRetreat`.
3. Assign `BT_IronvowDuel` to `BP_IronvowEnemy → AI Controller Class → Behavior Tree Asset`, and the
   blackboard asset to the tree.
4. In the AI controller Blueprint (optional wrapper), expose difficulty as an editable variable and
   call `SetDifficulty`.

## State Tree

The C++ side is a native state machine plus BT tasks. A State Tree asset needs the
`StateTreeModule` / `GameplayStateTreeModule` plugins, which this project deliberately does not
enable. If you add them, the tree tasks should call the same character API used here
(`SetGuardRaised`, `AIStartAttack`, `SetWeaponAimTarget`, `SetSpeedScale`).

## Known limits

- Navigation needs a NavMeshBoundsVolume in the arena; `MoveToLocation` silently does nothing without
  one. Add a nav mesh and press `P` to confirm the green area.
- The AI has no attack telegraph animation; the physical wind-up (guard → aim travel) is the tell.
- The AI never half-swords yet — `AIStartAttack` uses whatever grip the weapon is in. Add
  `WeaponControl->SetGripMode(HalfSword)` in a custom task if you want braced thrusts.
