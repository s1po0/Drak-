# Step 6 — Arena, UI and game modes

## What exists in code

### Arena — `AIronvowArena`

- Box bounds (`1400 × 1000 × 300 cm` by default), name, lore text, `MaxFighters`, spawn points.
- `GetSpawnTransform(i)` alternates sides and faces the arena centre.
- `GetRandomPointInArena()`, `IsInsideArena()`.
- Game modes find the first arena in the level automatically.

### Game modes

| Class | Rules |
| --- | --- |
| `AIronvowGameModeBase` | Spawn helper, kill routing, restart timer, player loadout override |
| `AArenaDuelGameMode` | 3 rounds to win, 2.2 s intermission, score tracking, respawns the opponent |
| `ASurvivalGameMode` | Waves (`FWaveDefinition` array or procedural fallback), intermission, kill count |
| `ASandboxGameMode` | Spawn in front of the player, give weapon/armour, kill all, respawn the player |

All three share `SpawnDuelist()`, which spawns an enemy, applies weapon, armour, difficulty and
health scale.

### UI

- `UIronvowHUDWidget` (C++ base, UMG friendly): `NativeTick` polls the player and calls
  `OnHUDUpdated(Health, Stamina, Balance, bRagdolled, bGuardRaised, bLockedOn)`, plus
  `OnBleedingChanged` and `OnMessageReceived` (with `PushMessage`).
- `UIronvowMenuWidget`: `ResumeGame`, `RestartDuel`, `QuitGame`, `OnMenuOpened`.
- `UIronvowMainMenuWidget`: `StartDuel` / `StartSurvival` / `StartSandbox` / `ReturnToMainMenu` /
  `QuitGame`, each opening an editable level name (`DuelMapName`, `SurvivalMapName`,
  `SandboxMapName`, `MainMenuMapName`), and switching to UI input on construct.
- `AIronvowPlayerController`: creates the HUD, handles `Esc`/`P`, pauses with `SetPause`,
  swaps input modes, exposes sandbox Exec commands.

## Test

1. **Arena duel**
   - Create a level with a floor, lighting, a `NavMeshBoundsVolume`, and one `AIronvowArena` actor
     (drag `IronvowArena` into the level). Save as `Content/Ironvow/Maps/Arena_Harrowmere`.
   - World Settings → GameMode = `ArenaDuelGameMode`. PlayerStart inside the arena.
   - PIE: an opponent spawns 3.2 m away; killing it advances the score, the next round starts after
     2.2 s, and reaching 3 wins stops the loop.
2. **Survival**
   - New level → GameMode = `SurvivalGameMode`. PIE: wave 1 spawns duelists, clearing them starts a
     4 s intermission, waves grow in count and health.
3. **Sandbox**
   - New level → GameMode = `SandboxGameMode`. PIE, press `~`:
     `IronvowSpawnEnemy 3`, `IronvowGiveWeapon 4`, `IronvowArmor 2`, `IronvowKillAll`,
     `IronvowSlowMo 0.3`.
   - Dying respawns the player after 2.2 s and never reloads the level.
4. **Pause** — `Esc` opens the menu, pauses the world, frees the cursor; `Resume` returns to game.
5. **HUD** — `OnHUDUpdated` fires every frame (add a print in the Blueprint to confirm).

## Manual Blueprint steps

1. **HUD widget**
   - Create `WBP_HUD` (parent `IronvowHUDWidget`).
   - Add a Canvas with: health bar (bind to the `HealthRatio` event argument), stamina bar, balance
     indicator (changes colour when below 0.4), a "DOWN" prompt when `bRagdolled`, a guard icon, and
     a lock-on reticle.
   - Implement `OnHUDUpdated` and set the values on those widgets.
   - Implement `OnMessageReceived` to show a text block for `MessageDuration` seconds.
2. **Pause menu**
   - Create `WBP_PauseMenu` (parent `IronvowMenuWidget`) with three buttons wired to
     `ResumeGame`, `RestartDuel`, `QuitGame`.
3. **Player controller**
   - Create `BP_IronvowPlayerController` (parent `IronvowPlayerController`) and set
     `HUD Widget Class = WBP_HUD`, `Pause Menu Class = WBP_PauseMenu`.
   - Set it as `Player Controller Class` in both BP game modes or in World Settings.
4. **Main menu**
   - Create `WBP_MainMenu` with parent **`IronvowMainMenuWidget`** (not plain UserWidget).
   - Three buttons call `StartDuel`, `StartSurvival`, `StartSandbox`; a fourth calls `QuitGame`.
   - Set the level names on the widget (`DuelMapName = Arena_Harrowmere`, etc.).
   - Put it in a UI-only `MainMenu` level and set that as the project's default map.
5. **Two arenas**
   - Duplicate `Arena_Harrowmere` as `Arena_Vellmoor`, move the `IronvowArena` bounds, change
     `Arena Name` and spawn points, and change the lighting/atmosphere for a coastal palisade.
   - Point the main menu buttons at each map.
