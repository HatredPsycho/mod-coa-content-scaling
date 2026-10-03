/*
 * CoA Universal Content Scaling
 * ProgressionLayout: Immutable realm-level progression layout and level compression.
 */

#ifndef COA_PROGRESSION_LAYOUT_H
#define COA_PROGRESSION_LAYOUT_H

#include "ContentEra.h"
#include "Define.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

struct LevelRange
{
    uint8 minLevel{1};
    uint8 maxLevel{60};

    constexpr LevelRange() = default;
    constexpr LevelRange(uint8 minLvl, uint8 maxLvl) : minLevel(minLvl), maxLevel(maxLvl) { }

    [[nodiscard]] constexpr bool Contains(uint8 level) const { return level >= minLevel && level <= maxLevel; }
    [[nodiscard]] constexpr uint8 Length() const { return maxLevel >= minLevel ? (maxLevel - minLevel) : 0; }
    [[nodiscard]] constexpr bool IsValid() const { return minLevel > 0 && maxLevel >= minLevel; }
};

struct ProgressionLayout
{
    uint8 maxLevel{60};

    bool tbcEnabled{false};
    bool wotlkEnabled{false};

    LevelRange classic{1, 60};
    std::optional<LevelRange> tbc;
    std::optional<LevelRange> wotlk;

    // Factory to construct layout based on active packs, level cap and optional custom boundaries
    static ProgressionLayout Create(uint8 maxLevel, bool tbcEnabled, bool wotlkEnabled,
                                   uint8 customClassicEnd = 0, uint8 customTbcEnd = 0);

    [[nodiscard]] bool Validate(std::string& outError) const;

    [[nodiscard]] LevelRange GetEraRange(ContentEra era) const;

    [[nodiscard]] static LevelRange CanonicalAuthoredRange(ContentEra era);
    [[nodiscard]] ContentEra ResolveContentEra(ContentEra taggedEra, uint8 authoredLevel) const;

    [[nodiscard]] bool IsEraEnabled(ContentEra era) const;

    [[nodiscard]] uint8 MapAuthoredToEffective(ContentEra era, uint8 authoredLevel,
                                               uint8 sourceMin = 0, uint8 sourceMax = 0) const;

    [[nodiscard]] ContentEra GetEraForAuthoredLevel(uint8 authoredLevel) const;

    [[nodiscard]] std::string ToString() const;
};

#endif // COA_PROGRESSION_LAYOUT_H
