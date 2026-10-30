# The Gray March — content and tuning tables

All names, places and lore below are original to this project. Nothing is borrowed from Half Sword
or any other game.

## The world

**The Gray March** is a border duchy between two exhausted kingdoms. The old courts have stopped
hearing cases, so feuds are settled in rings of packed earth by fighters who are paid in grain,
salt and grudges. Three powers matter:

- **The Vow** — the duke's sworn brotherhood, armoured in fitted plate. They believe a duel is a
  legal instrument.
- **The Ash-Bound** — a mercenary company that fights without heraldry and sells its survivors back
  to their families.
- **The March itself** — villages like Vale, Kellwood, Reedmark and Vellmoor, whose smiths name
  weapons after the ground they were made for.

Two arenas:

| Arena | Description | In-game |
| --- | --- | --- |
| **The Ring of Harrowmere** | Packed earth inside a broken curtain wall, one gap where the crowd stands. | `AIronvowArena` default, 1400×1000 cm |
| **The Salt Yard of Vellmoor** | A palisaded yard above the tide line; salt eats armour and the wind carries sound away. | Duplicate the level, halve the bounds, raise wind / fog |

## Weapons

| Preset | Name | Edge | Mass | Length | Balance | Base dmg | Tip force | Half-sword |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| ArmingSword | Vale Arming Sword | Slash | 1.15 kg | 92 cm | −9 cm | 20 | 1300 N | no |
| Longsword | Harrowmere Longsword | Slash | 1.55 kg | 118 cm | −12 cm | 24 | 1550 N | **yes** |
| Axe | Kellwood Bearded Axe | Slash | 1.80 kg | 86 cm | +4 cm | 27 | 1450 N | no |
| Mace | Marrow Mace of Saint Ord | Blunt | 2.30 kg | 74 cm | +8 cm | 30 | 1400 N | no |
| Spear | Reedmark Boar Spear | Pierce | 2.00 kg | 210 cm | +30 cm | 24 | 1600 N | no |
| Dagger | Whisper Dagger | Pierce | 0.40 kg | 42 cm | −2 cm | 12 | 900 N | no |

Flavour:

- *Vale Arming Sword* — a levy blade; quick enough to find a seam in mail.
- *Harrowmere Longsword* — the duelling blade of the March; the point finishes what the edge starts.
- *Kellwood Bearded Axe* — a woodcutter's tool promoted to an argument.
- *Marrow Mace of Saint Ord* — does not need to find a gap; it brings the gap with it.
- *Reedmark Boar Spear* — long, patient, cruel: the March's answer to plate.
- *Whisper Dagger* — the last word in a close duel.

## Armour

| Set | Name | Weight | Move | Stamina | Layers |
| --- | --- | --- | --- | --- | --- |
| Homespun | Homespun of the March | 5 kg | ×0.98 | ×1.02 | cloth 0.6–0.95 coverage |
| Byrnie | Riveted Byrnie of Harrowmere | 13 kg | ×0.90 | ×1.12 | cloth + mail (mail 0.45–0.95) |
| VowPlate | Vow Plate | 24 kg | ×0.80 | ×1.28 | cloth + mail + plate (plate 0.6–0.95) |

Defaults per zone (see `UIronvowPresetLibrary::MakeArmorData` for exact layer values):

- Head: cloth → mail → plate, hardness rising with tier.
- Torso: the most complete coverage (0.95–1.0).
- Arms: plate coverage drops to 0.6 on the Vow Plate (articulation gaps).
- Legs: mail coverage drops to 0.45 on the Byrnie (riding harness).

## Enemy tiers

| Preset | Name in fiction | Reaction | Aim error | Block | Aggression |
| --- | --- | --- | --- | --- | --- |
| Novice | Novice of the March | 0.75 s | 34 cm | 0.30 | 0.35 |
| Duelist | March Duelist | 0.42 s | 16 cm | 0.60 | 0.60 |
| Vowbreaker | Vowbreaker | 0.22 s | 6 cm | 0.80 | 0.80 |

## Suggested survival wave table

Paste into `ASurvivalGameMode::Waves` on `BP_SurvivalGameMode` if you want authored waves; otherwise
the procedural fallback generates them.

| Wave | Count | Weapon | Armour | Difficulty | Health |
| --- | --- | --- | --- | --- | --- |
| 1 | 1 | Arming Sword | Homespun | Novice | 1.0 |
| 2 | 1 | Axe | Homespun | Novice | 1.0 |
| 3 | 2 | Arming Sword | Byrnie | Duelist | 1.0 |
| 4 | 2 | Spear | Byrnie | Duelist | 1.1 |
| 5 | 2 | Mace | Byrnie | Duelist | 1.15 |
| 6 | 2 | Longsword | Vow Plate | Vowbreaker | 1.2 |
| 7 | 3 | Mixed | Vow Plate | Vowbreaker | 1.3 |
