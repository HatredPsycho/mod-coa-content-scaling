# mod-coa-content-scaling

**Universal Content Scaling & Progression Framework for AzerothCore / Conquest of Azeroth (CoA)**

`mod-coa-content-scaling` is a production-ready, modular, data-driven content scaling engine designed for custom level-cap realms (e.g. level 60, 70, 80) and dynamic group sizing (1..N players, including solo play).

Unlike destructive SQL migration patches that rewrite creature/item/quest database rows, this module normalizes power budgets in memory and dynamically maps authored content levels to effective combat levels at runtime.

---

## Architecture Overview

```text
mod-coa-content-scaling (Core Engine)
    ├── ProgressionLayout (Anchor interpolation for any MaxPlayerLevel, or the Journey preset)
    ├── JourneyRules + JourneyData (Overlapping per-expansion ranges, endgame ladder, zone and instance tables)
    ├── ContentEra & ContentTier (Separates character level from content power)
    ├── ContentEraResolver (Era of maps, areas, creatures, quests and items from Map.dbc / AreaTable.dbc)
    ├── CombatBudgetProfile (Runtime HP / Damage / Mana / Armor normalization in instances)
    ├── InstanceScaleContext (1..N group scaling & Anti-Exploit snapshot lock)
    ├── Spell & Aura Scaling (Dynamic flat spell damage / tick / absorb scaling)
    ├── ItemBudgetScaler (ItemTemplate stats/damage/armor in-memory normalization)
    ├── AdaptiveEncounterAPI & SoloAssistPolicy (Solo / small-group mechanics assist)
    └── Diagnostic Commands (.coascale)
```

Which expansion a map belongs to comes from the client data (`Map.dbc`, `AreaTable.dbc`, `MapDifficulty.dbc`);
`CoAContentScaling.TBC.Enable` and `CoAContentScaling.WotLK.Enable` decide which expansions are placed in the
layout. Classic is the base world, and the expansions are strictly independent:
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

### 1a. The Journey Preset
`CoAContentScaling.Progression.Mode = Journey` lays each expansion over its own, overlapping range instead of cutting the
level space into consecutive bands: by default the old world 1-35, Outland 30-43, Northrend 40-55, with the heroics and
raids of Outland and Northrend at the endgame level (60, bosses up to three above). The rules and the zone and instance
tables come from [mod-world-journey](https://github.com/aspollon/mod-world-journey) by aspollon (AGPL v3):
`JourneyRules.cpp` holds the arithmetic, `data/journey/` the original zone and instance levels, and
`tools/make_journey_data.py` builds `include/JourneyData.h` from them. The zone table also names the era of each zone,
ahead of the expansion `Map.dbc` gives its map. The regulators are the `CoAContentScaling.Journey.*` settings.

### 2. Combat Budget Normalization (Creature Stats)
In AzerothCore, `CreatureBaseStats` indexes health and damage by `expansion`. Simply lowering a creature's level in memory still uses the WotLK expansion multiplier table.
`mod-coa-content-scaling` addresses this by decoupling **Character Level** from **Content Power**:
- `effectiveLevel` dictates combat-table semantics (hit/miss, glancing, defense).
- `ContentTier` (`WORLD`, `DUNGEON_NORMAL`, `DUNGEON_HEROIC`, `RAID_ENTRY`..`RAID_PINNACLE`) dictates actual stat budgets (HP, base damage, armor, mana).
- Replaces authored expansion multipliers with normalized budgets on the fly without database mutations.
- Only dungeons and raids carry a budget. In the open world a creature takes the base statistics of the level it
  was moved to, and the realm's own creature scaling lifts it from there.

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
| `.coascale eras` | Shows how many maps, areas and instance profiles the era resolution read from the client data. |
| `.coascale validate` | Performs runtime consistency check on progression boundaries and active configuration. |

---

## Installation

1. Copy or clone this repository into your AzerothCore `modules/` directory:
   ```bash
   git clone https://github.com/Corfirean/mod-coa-content-scaling.git modules/mod-coa-content-scaling
   ```
2. Build against a core that carries the content scaling hooks (`ContentScalingHooks.h`, the encounter mechanic and
   dungeon access hooks): the `coa-scaling` branch of the CoA fork.
3. Re-run CMake and compile:
   ```bash
   cmake -B build
   cmake --build build --config Release
   ```
4. Copy configuration file:
   ```bash
   cp modules/mod-coa-content-scaling/conf/mod-coa-content-scaling.conf.dist env/dist/etc/mod-coa-content-scaling.conf
   ```

---

## Configuration (`mod-coa-content-scaling.conf`)

```ini
[worldserver]
# Master switch
CoAContentScaling.Enable = 1

# Progression Mode: Auto, Custom or Journey
CoAContentScaling.Progression.Mode = "Auto"
CoAContentScaling.Progression.ClassicEnd = 0
CoAContentScaling.Progression.TbcEnd = 0

# Expansions placed in the layout
CoAContentScaling.TBC.Enable = 0
CoAContentScaling.WotLK.Enable = 0

# Group & Instance Scaling
CoAContentScaling.GroupScaling.Enable = 1
CoAContentScaling.GroupScaling.LockOnEncounterStart = 1
CoAContentScaling.AllowSoloRaids = 1

# Solo Assist (0 = None, 1 = Light 15% reduction, 2 = Full 30% reduction)
CoAContentScaling.SoloAssist.Mode = 0

# Adaptive Mechanics & Rewards
CoAContentScaling.AdaptiveMechanics.Enable = 1
CoAContentScaling.Rewards.ScaleLootCount = 1

# Debug Logging
CoAContentScaling.Debug = 0
```

---

## License

This module is released under the GNU General Public License v2 (GPL-2.0), in line with the AzerothCore Project.
