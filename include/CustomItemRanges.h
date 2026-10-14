/*
 * CoA Universal Content Scaling
 * CustomItemRanges: which custom item ranges carry a stat budget.
 */

#ifndef COA_CUSTOM_ITEM_RANGES_H
#define COA_CUSTOM_ITEM_RANGES_H

#include "Define.h"
#include <array>

struct CustomItemRange
{
    uint32 first;
    uint32 last;
    uint8 policy;   // 0 STANDARD, 1 TIER_ALIGNED, 2 PRESERVE, 3 REVIEW_SPECIAL
};

inline constexpr std::array<CustomItemRange, 3> CustomItemRangeTable =
{{
    CustomItemRange{ 100000u, 200000u, 2 },   // CUSTOM_COSMETIC
    CustomItemRange{ 200001u, 350000u, 2 },   // CUSTOM_CLASS_ITEM
    CustomItemRange{ 350001u, 4294967295u, 1 },   // CUSTOM_GAMEPLAY
}};

inline CustomItemRange const* FindCustomItemRange(uint32 itemId)
{
    for (CustomItemRange const& range : CustomItemRangeTable)
        if (itemId >= range.first && itemId <= range.last)
            return &range;

    return nullptr;
}

#endif // COA_CUSTOM_ITEM_RANGES_H
