# mod-coa-content-scaling

**Universal Content Scaling & Progression Framework for AzerothCore / Conquest of Azeroth (CoA)**

`mod-coa-content-scaling` is a production-ready, modular, data-driven content scaling engine designed for custom level-cap realms (e.g. level 60, 70, 80) and dynamic group sizing (1..N players, including solo play).

Unlike destructive SQL migration patches that rewrite creature/item/quest database rows, this module normalizes power budgets in memory and dynamically maps authored content levels to effective combat levels at runtime.

---

## Architecture Overview

```text
mod-coa-content-scaling (Core Engine)
    ├── ProgressionLayout (Anchor interpolation for any MaxPlayerLevel)
    ├── ContentEra & ContentTier (Separates character level from content power)
    ├── CombatBudgetProfile (Runtime HP / Damage / Mana / Armor normalization)
    ├── InstanceScaleContext (1..N group scaling & Anti-Exploit snapshot lock)
    ├── Spell & Aura Scaling (Dynamic flat spell damage / tick / absorb scaling)
    ├── ItemBudgetScaler (ItemTemplate stats/damage/armor in-memory normalization)
    ├── AdaptiveEncounterAPI & SoloAssistPolicy (Solo / small-group mechanics assist)
    └── Diagnostic Commands (.coascale)

Content Packs (Optional & Independent):
    ├── content-packs/mod-coa-tbc-content    (Outland, TBC dungeons & raids)
    └── content-packs/mod-coa-wotlk-content  (Northrend, WotLK dungeons & raids)
```

Classic is the base world. Expansion content packs are strictly independent:
- **Classic only**: Classic spans `1 -> MaxPlayerLevel`.
- **Classic + TBC**: Classic compresses, TBC finishes at `MaxPlayerLevel`.
- **Classic + WotLK**: Classic compresses, WotLK finishes at `MaxPlayerLevel` (TBC is NOT required).
- **Classic + TBC + WotLK**: Full progression layout `1 -> ClassicEnd -> TbcEnd -> MaxPlayerLevel`.

---

## Key Features

### 1. Dynamic `MaxPlayerLevel` as Single Source of Truth
The module never hardcodes max level to 60 or 80. It reads `CONFIG_MAX_PLAYER_LEVEL` from the world configuration.
- **Level 60 layout**: Classic `1-45`, TBC `45-55`, WotLK `55-60`.
- **Level 70 layout**: Classic `1-53`, TBC `53-63`, WotLK `63-70`.
- **Level 80 layout**: Classic `1-60`, TBC `60-70`, WotLK `70-80`.
- **Interpolated layouts**: Any cap between 60 and 80 interpolates monotonically using reference anchors.
- **Fail-safe validation**: Unsafe caps (>80) fail-fast with clean logging and safe Classic fallback.

### 2. Combat Budget Normalization (Creature Stats)
In AzerothCore, `CreatureBaseStats` indexes health and damage by `expansion`. Simply lowering a creature's level in memory still uses the WotLK expansion multiplier table.
`mod-coa-content-scaling` addresses this by decoupling **Character Level** from **Content Power**:
- `effectiveLevel` dictates combat-table semantics (hit/miss, glancing, defense).
- `ContentTier` (`WORLD`, `DUNGEON_NORMAL`, `DUNGEON_HEROIC`, `RAID_ENTRY`..`RAID_PINNACLE`) dictates actual stat budgets (HP, base damage, armor, mana).
- Replaces authored expansion multipliers with normalized budgets on the fly without database mutations.

### 3. Instance & Group Scaling (1..N Players)
- PvE dungeons and raids scale dynamically for groups from 1 player up to baseline size (5, 10, 25, 40).
- Uses diminishing-returns power curves for health: `(players / baseline)^0.65` and safe linear steps for damage.
- **Anti-Exploit Snapshot Lock**: When a boss encounter begins (`OnUnitEnterCombat`), the scale context locks. Players zoning in or leaving mid-combat cannot cheese encounter scaling. Context unlocks upon wipe, evade, or victory.
- **Calibrated Boss Budgets (`coa_boss_flex`)**: Integrates seamlessly with hand-tuned per-player budgets when present.
- **Double-Scaling Prevention**: Automatically disables legacy `FlexHealth` scripts to prevent quadratic stat multiplication.

### 4. Spells & Auras Scaling
- Boss spells with flat damage, periodic aura ticks, healing, and absorption shields (`SPELL_AURA_SCHOOL_ABSORB`, `SPELL_AURA_MANA_SHIELD`) scale according to the active instance context.

### 5. In-Memory Item Budget Scaling
- `ItemTemplate` records (RequiredLevel, ItemLevel, stats, armor, weapon damage) are normalized at server startup based on the active progression layout.
- In WoW 3.3.5, client tooltips are queried dynamically via `CMSG_ITEM_QUERY_SINGLE`, allowing players to see correct scaled stats immediately without requiring client DBC modifications.

### 6. Adaptive Encounter API & Solo Assist
- Supports role-deficit mitigation for solo and small-group dungeon runs (`CoAContentScaling.SoloAssist.Mode = 1` or `2`).
- Scales dynamic loot quantities according to group size to maintain economy balance.

---

### 7. Core LFG Architecture Integration (Solo / Matchmaking / Bot Fill)
`mod-coa-content-scaling` hooks into AzerothCore's Dungeon Finding engine (`LFGMgr`, `LFGQueue`) to support flexible group compositions without breaking standard matchmaking:
- **`LfgCompositionMode`**:
  - `MATCHMAKING`: Standard automated group assembly with real players.
  - `BOT_FILL`: Immediately fills missing party roles with autonomous `mod-coa-playerbots` bots.
  - `CURRENT_PARTY`: Enters the dungeon with the current partial group (or solo) without waiting for 5 players, relying on dynamic instance scaling.
- **Queue Policy Isolation**: Non-standard queue policies bypass role check and standard composition requirements (`LfgQueuePolicy.bypassMatchmaking`), ensuring solo and partial groups can instantly enter dungeons.
- **Persistence**: Player LFG preferences (`lfg_mode`, `challenge_size`) are stored in `character_coa_lfg_settings`.

---

## In-Game Diagnostics & Commands

### Administration (`.coascale`)
| Command | Description |
|---|---|
| `.coascale status` | Displays current module state, MaxPlayerLevel, active eras, and group scaling flags. |
| `.coascale layout` | Prints the complete active `ProgressionLayout` with level ranges and boundaries. |
| `.coascale creature [entry]` | Inspects creature: authored vs effective level, tier, HP, damage, and era resolution source/confidence. |
| `.coascale instance` | Shows instance scale context for current map (player count, HP scale, damage scale, lock status). |
| `.coascale quest <questId>` | Inspects quest authored vs effective level, and era resolution source. |
| `.coascale item <itemId>` | Inspects item authored vs scaled budget, power band, tier, and era resolution source. |
| `.coascale lfg` | Displays current player's CoA LFG settings (composition mode, challenge size). |
| `.coascale validate` | Performs runtime consistency check on progression boundaries and active configuration. |

### Player LFG Commands
| Command | Description |
|---|---|
| `.lfgmode [matchmaking\|bots\|party]` | Sets personal dungeon finder composition mode (standard queue, bot fill, or partial party). |
| `.lfgchallenge [adaptive\|1..40]` | Sets instance scaling challenge target (adaptive to group size, or fixed simulated player count). |

Without an argument both commands show the current settings. The settings are kept per character. In a group the leader's setting decides. Random bots never set one and keep
`LFG.DefaultMode`.

### CoALFGMode Addon
`addon/CoALFGMode` puts the same two settings in a row above the Dungeon Finder's queue button: Group (Matchmaking,
Fill with bots, Start now) and Bosses (the challenge size). Its tooltip explains the choices and why one is unavailable. Copy the folder into the client's `Interface\AddOns`. `/coalfg` sets them from the
chat line as well (`/coalfg party`, `/coalfg challenge 5`, `/coalfg` alone shows the current state).

The addon whispers itself in the addon language with the prefix `CoALFG`; the server answers every request with the
character's state:

| Request | Answer |
|---|---|
| `GET`, `MODE matchmaking\|bots\|party`, `CHALLENGE 0..40` | `STATE <mode> <challenge> <scaling 0\|1> <botfill 0\|1>` |

Bot fill is offered only when a bot provider (mod-playerbots) is registered, and every choice is disabled while
content scaling is off.

---

## Installation

1. Copy or clone this repository into your AzerothCore `modules/` directory:
   ```bash
   git clone https://github.com/Corfirean/mod-coa-content-scaling.git modules/mod-coa-content-scaling
   ```
2. (Optional) If you want TBC and/or WotLK content packs active, link or copy them into `modules/`:
   ```bash
   cp -r modules/mod-coa-content-scaling/content-packs/mod-coa-tbc-content modules/
   cp -r modules/mod-coa-content-scaling/content-packs/mod-coa-wotlk-content modules/
   ```
3. Apply the minor core hook patch if not already present in your fork:
   ```bash
   git apply modules/mod-coa-content-scaling/patches/core-hooks.patch
   ```
4. Re-run CMake and compile:
   ```bash
   cmake -B build
   cmake --build build --config Release
   ```
5. Copy configuration file:
   ```bash
   cp modules/mod-coa-content-scaling/conf/mod-coa-content-scaling.conf.dist env/dist/etc/mod-coa-content-scaling.conf
   ```

---

## Configuration (`mod-coa-content-scaling.conf`)

```ini
[worldserver]
# Master switch
CoAContentScaling.Enable = 1

# Progression Mode: Auto or Custom
CoAContentScaling.Progression.Mode = "Auto"
CoAContentScaling.Progression.ClassicEnd = 0
CoAContentScaling.Progression.TbcEnd = 0

# Group & Instance Scaling
CoAContentScaling.GroupScaling.Enable = 1
CoAContentScaling.GroupScaling.LockOnEncounterStart = 1
CoAContentScaling.AllowSoloRaids = 1

# Solo Assist (0 = None, 1 = Light 15% reduction, 2 = Full 30% reduction)
CoAContentScaling.SoloAssist.Mode = 0

# Adaptive Mechanics & Rewards
CoAContentScaling.AdaptiveMechanics.Enable = 1
CoAContentScaling.Rewards.ScaleLootCount = 1

# LFG Integration Default Mode (0 = Matchmaking, 1 = Bot Fill, 2 = Current Party)
CoAContentScaling.LFG.DefaultMode = 0

# Debug Logging
CoAContentScaling.Debug = 0
```

---

## License

This module is released under the GNU General Public License v2 (GPL-2.0), in line with the AzerothCore Project.
