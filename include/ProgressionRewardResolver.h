/*
 * CoA Universal Content Scaling
 * ProgressionRewardResolver: Authority layer for XP, quest rewards, money conversion and level gating.
 */

#ifndef COA_PROGRESSION_REWARD_RESOLVER_H
#define COA_PROGRESSION_REWARD_RESOLVER_H

#include "ContentEra.h"
#include "ContentTier.h"
#include "Define.h"
#include "ProgressionContext.h"
#include "ProgressionLayout.h"
#include <algorithm>
#include <cmath>

class ProgressionRewardResolver
{
public:
    static ProgressionRewardResolver* Instance()
    {
        static ProgressionRewardResolver instance;
        return &instance;
    }

    /// Resolves calibrated quest XP based on effective quest level and progression position.
    /// Preserves stock baseline XP when unscaled / cap 80 with all eras enabled.
    /// When compressed (e.g. cap 60), calibrates XP smoothly between eras to prevent starvation or instant level-ups.
    [[nodiscard]] uint32 ResolveQuestXP(uint32 authoredXP, int32 authoredLevel, int32 effectiveLevel,
                                        ProgressionLayout const& layout, ContentEra era) const
    {
        if (authoredXP == 0)
            return 0;

        // If identity cap 80 with all expansions active, preserve authored XP 1:1
        if (layout.maxLevel == 80 && layout.tbcEnabled && layout.wotlkEnabled)
            return authoredXP;

        if (authoredLevel <= 0 || effectiveLevel <= 0)
            return authoredXP;

        // Base ratio reflects the level compression scaling curve
        // Using logarithmic damping to avoid penalizing or overbuffing compressed bands
        float const levelRatio = float(effectiveLevel) / float(authoredLevel);

        // Calculate progression position factor: quests later in an era provide higher relative reward
        LevelRange const eraRange = layout.GetEraRange(era);
        float eraSpanFactor = 1.0f;
        if (eraRange.maxLevel > eraRange.minLevel)
        {
            float const pos = float(effectiveLevel - eraRange.minLevel) / float(eraRange.maxLevel - eraRange.minLevel);
            // Reward identity: late era quests give up to 15% more budget than early era quests
            eraSpanFactor = 0.90f + 0.25f * std::clamp(pos, 0.0f, 1.0f);
        }

        float const calibrated = float(authoredXP) * levelRatio * eraSpanFactor;
        return std::max<uint32>(10, static_cast<uint32>(std::round(calibrated)));
    }

    /// Evaluates safety policy for max-level quest XP -> Gold conversion.
    /// Formula is standard 6c per XP, with rate check and maximum gold guard to prevent economy inflation.
    [[nodiscard]] int32 ResolveMoneyAtCap(uint32 effectiveXP, float bonusMoneyRate) const
    {
        if (effectiveXP == 0)
            return 0;

        // 6 copper per XP point (standard Blizzard formula: 10,000 XP -> 6 gold)
        double copper = double(effectiveXP) * 6.0 * double(bonusMoneyRate);

        // Safety upper bound per quest: 50 Gold (500,000 copper) to prevent abuse from uncalibrated high-tier quests
        constexpr double MAX_QUEST_CONVERSION_COPPER = 500000.0;
        if (copper > MAX_QUEST_CONVERSION_COPPER)
            copper = MAX_QUEST_CONVERSION_COPPER;

        return static_cast<int32>(std::round(copper));
    }

    /// Resolves required item level to guarantee it does not exceed the acquisition content level.
    [[nodiscard]] uint32 ResolveItemRequiredLevel(uint32 authoredRequiredLevel, ContentEra era,
                                                  uint32 effectiveContentLevel, ProgressionLayout const& layout) const
    {
        if (authoredRequiredLevel == 0)
            return 0;

        // Identity layout check
        if (layout.maxLevel == 80 && layout.tbcEnabled && layout.wotlkEnabled)
            return authoredRequiredLevel;

        uint32 mappedLevel = layout.MapAuthoredToEffective(era, static_cast<uint8>(authoredRequiredLevel));

        // Invariant: requiredLevel <= expected acquisition level + margin (1)
        if (effectiveContentLevel > 0 && mappedLevel > (effectiveContentLevel + 1))
            mappedLevel = effectiveContentLevel;

        return std::min<uint32>(mappedLevel, layout.maxLevel);
    }

    /// Calculates tiered raid unlock level window to preserve progression order:
    /// Normal Dungeon < Heroic Dungeon < Raid Entry < Raid Mid < Raid End < Raid Pinnacle.
    [[nodiscard]] uint8 ResolveTierUnlockLevel(ContentTier tier, ContentEra era,
                                              ProgressionLayout const& layout) const
    {
        LevelRange const range = layout.GetEraRange(era);
        if (!range.IsValid())
            return layout.maxLevel;

        uint8 const minL = range.minLevel;
        uint8 const maxL = range.maxLevel;
        uint8 const span = (maxL > minL) ? (maxL - minL) : 0;

        // If span is tight (< 4 levels), stagger by 1 level where possible
        switch (tier)
        {
            case ContentTier::WORLD:
            case ContentTier::DUNGEON_NORMAL:
                return minL;

            case ContentTier::DUNGEON_HEROIC:
                // Heroics unlock towards the upper part of the era span, before raids
                if (span >= 5)
                    return minL + static_cast<uint8>(std::round(float(span) * 0.70f));
                return (maxL > 1) ? (maxL - 1) : maxL;

            case ContentTier::RAID_ENTRY:
                if (span >= 5)
                    return minL + static_cast<uint8>(std::round(float(span) * 0.80f));
                return (maxL > 1) ? (maxL - 1) : maxL;

            case ContentTier::RAID_MID:
                if (span >= 5)
                    return minL + static_cast<uint8>(std::round(float(span) * 0.88f));
                return maxL;

            case ContentTier::RAID_END:
                if (span >= 5)
                    return minL + static_cast<uint8>(std::round(float(span) * 0.94f));
                return maxL;

            case ContentTier::RAID_PINNACLE:
            default:
                return maxL;
        }
    }

    /// Which reward bracket a finished random dungeon pays from. The brackets in
    /// lfg_dungeon_rewards are keyed by the levels the dungeons were authored at, so a player on a
    /// realm that compressed those levels points at a bracket far below the content they just ran.
    /// This maps their effective level back to the authored one.
    [[nodiscard]] uint8 ResolveLfgRewardLevel(ContentEra era, uint8 playerLevel,
                                              ProgressionLayout const& layout) const
    {
        if (layout.maxLevel == 80 && layout.tbcEnabled && layout.wotlkEnabled)
            return playerLevel;

        LevelRange const eraRange = layout.GetEraRange(era);
        if (playerLevel >= eraRange.maxLevel)
        {
            switch (era)
            {
                case ContentEra::WotLK:
                    return 80;
                case ContentEra::TBC:
                    return 70;
                case ContentEra::Classic:
                default:
                    return 60;
            }
        }

        uint8 authMin = 1;
        uint8 authMax = 60;
        switch (era)
        {
            case ContentEra::WotLK:
                authMin = 68;
                authMax = 80;
                break;
            case ContentEra::TBC:
                authMin = 58;
                authMax = 70;
                break;
            case ContentEra::Classic:
            default:
                authMin = 1;
                authMax = 60;
                break;
        }

        if (eraRange.maxLevel <= eraRange.minLevel)
            return authMax;

        float const progress = float(playerLevel - eraRange.minLevel) / float(eraRange.maxLevel - eraRange.minLevel);
        uint8 const mapped = authMin + static_cast<uint8>(std::round(progress * float(authMax - authMin)));
        return std::clamp<uint8>(mapped, authMin, authMax);
    }

private:
    ProgressionRewardResolver() = default;
};

#define sProgressionRewardResolver ProgressionRewardResolver::Instance()

#endif // COA_PROGRESSION_REWARD_RESOLVER_H
