/*
 * CoA Universal Content Scaling
 * ItemBudgetScaler: Implementation of item stat, rating and level budget scaling.
 */

#include "ItemBudgetScaler.h"
#include "ContentPackRegistry.h"
#include "GeneratedContentCensus.h"
#include "InstanceProfile.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectMgr.h"
#include <algorithm>
#include <cmath>

namespace
{
    struct PowerBandMapping
    {
        uint32 authMin;
        uint32 authMax;
        uint32 effMin;
        uint32 effMax;
    };

    uint32 MapThroughBand(uint32 authoredIlvl, PowerBandMapping const& band)
    {
        if (authoredIlvl <= band.authMin)
            return band.effMin;
        if (authoredIlvl >= band.authMax)
            return band.effMax;

        float const t = float(authoredIlvl - band.authMin) / float(std::max<uint32>(1, band.authMax - band.authMin));
        return band.effMin + static_cast<uint32>(std::round(t * float(band.effMax - band.effMin)));
    }
}

ItemScalingContext ItemScalingContext::Resolve(ItemTemplate const* proto)
{
    ItemScalingContext ctx;
    if (!proto)
        return ctx;

    // 1. Check generated census profile (Authoritative Source of Truth)
    if (auto const* prof = FindGeneratedItemProfile(proto->ItemId))
    {
        ctx.hasGeneratedProfile = true;
        ctx.era = prof->era;
        ctx.tier = prof->tier;
        ctx.sourceMap = prof->sourceMap;
        ctx.specialFlags = prof->specialFlags;
        ctx.policy = static_cast<ItemScalingPolicy>(prof->policy);

        return ctx;
    }

    // 2. Custom Item Safety Policy Fallback (for unprofiled custom items)
    // custom_content.json is the single authoritative source of truth for custom classification.
    // Unprofiled custom items receive safe fallback: CUSTOM_UNCLASSIFIED -> PRESERVE + warning.
    if (proto->ItemId >= 100000)
    {
        ctx.specialFlags |= (ITEM_SPECIAL_CUSTOM | ITEM_SPECIAL_PRESERVE);
        ctx.sourceMap = 0;
        ctx.era = ContentEra::Custom;
        ctx.tier = ContentTier::WORLD;
        ctx.policy = ItemScalingPolicy::PRESERVE;

        LOG_WARN("server.loading", "UniversalContentScaling: Unprofiled custom item {} encountered; applying safe PRESERVE fallback.",
            proto->ItemId);
        return ctx;
    }

    // 3. Fallback: Era resolution through registry
    EraResolutionResult const eraRes = sContentPackRegistry->ResolveEraDetailsForItem(
        proto->ItemId, proto->ItemLevel, proto->RequiredLevel);
    ctx.era = eraRes.era;

    // Detect triggers/special features from ItemTemplate for fallback
    for (uint32 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        if (proto->Spells[i].SpellId > 0)
        {
            uint32 trig = proto->Spells[i].SpellTrigger;
            if (trig == 1 || trig == 2)
                ctx.specialFlags |= ITEM_SPECIAL_PROC;
            else if (trig == 0)
                ctx.specialFlags |= ITEM_SPECIAL_USE;
        }
    }
    if (proto->ItemSet > 0)
        ctx.specialFlags |= ITEM_SPECIAL_SET;
    for (uint32 i = 0; i < MAX_ITEM_PROTO_SOCKETS; ++i)
    {
        if (proto->Socket[i].Color != 0)
        {
            ctx.specialFlags |= ITEM_SPECIAL_SOCKET;
            break;
        }
    }

    // Fallback tier inference from authored ItemLevel
    ctx.fallbackTierInference = true;
    if (ctx.era == ContentEra::Classic)
    {
        if (proto->ItemLevel < 66)
            ctx.tier = ContentTier::DUNGEON_NORMAL;
        else if (proto->ItemLevel <= 68)
            ctx.tier = ContentTier::RAID_ENTRY;
        else if (proto->ItemLevel <= 75)
            ctx.tier = ContentTier::RAID_MID;
        else if (proto->ItemLevel <= 83)
            ctx.tier = ContentTier::RAID_END;
        else
            ctx.tier = ContentTier::RAID_PINNACLE;
    }
    else if (ctx.era == ContentEra::TBC)
    {
        if (proto->ItemLevel <= 115)
            ctx.tier = ContentTier::DUNGEON_NORMAL;
        else if (proto->ItemLevel <= 128)
            ctx.tier = ContentTier::RAID_ENTRY;
        else if (proto->ItemLevel <= 138)
            ctx.tier = ContentTier::RAID_MID;
        else if (proto->ItemLevel <= 151)
            ctx.tier = ContentTier::RAID_END;
        else
            ctx.tier = ContentTier::RAID_PINNACLE;
    }
    else if (ctx.era == ContentEra::WotLK)
    {
        if (proto->ItemLevel <= 187)
            ctx.tier = ContentTier::DUNGEON_NORMAL;
        else if (proto->ItemLevel <= 213)
            ctx.tier = ContentTier::RAID_ENTRY;
        else if (proto->ItemLevel <= 226)
            ctx.tier = ContentTier::RAID_MID;
        else if (proto->ItemLevel <= 245)
            ctx.tier = ContentTier::RAID_END;
        else
            ctx.tier = ContentTier::RAID_PINNACLE;
    }
    else
    {
        ctx.tier = ContentTier::WORLD;
    }

    if (ctx.specialFlags & (ITEM_SPECIAL_PROC | ITEM_SPECIAL_USE | ITEM_SPECIAL_SET | ITEM_SPECIAL_SOCKET))
        ctx.policy = ItemScalingPolicy::REVIEW_SPECIAL;
    else
        ctx.policy = ItemScalingPolicy::STANDARD;

    return ctx;
}

ItemBudgetScaler* ItemBudgetScaler::Instance()
{
    static ItemBudgetScaler instance;
    return &instance;
}

ScaledItemBudget ItemBudgetScaler::CalculateItemBudget(ItemTemplate const* proto,
                                                      ProgressionLayout const& layout) const
{
    ItemScalingContext const ctx = ItemScalingContext::Resolve(proto);
    return CalculateItemBudget(proto, layout, ctx);
}

ScaledItemBudget ItemBudgetScaler::CalculateItemBudget(ItemTemplate const* proto,
                                                      ProgressionLayout const& layout,
                                                      ItemScalingContext const& ctx) const
{
    ScaledItemBudget budget;
    if (!proto)
        return budget;

    budget.effectiveRequiredLevel = proto->RequiredLevel;
    budget.effectiveItemLevel = proto->ItemLevel;

    // PRESERVE policy: strictly zero changes to combat fields, levels or budgets
    if (ctx.policy == ItemScalingPolicy::PRESERVE)
        return budget;

    // 1. Required level scaling
    if (proto->RequiredLevel > 0)
    {
        switch (ctx.era)
        {
            case ContentEra::Classic:
                budget.effectiveRequiredLevel = layout.MapAuthoredToEffective(
                    ContentEra::Classic, proto->RequiredLevel, 1, 60);
                break;
            case ContentEra::TBC:
                budget.effectiveRequiredLevel = layout.MapAuthoredToEffective(
                    ContentEra::TBC, proto->RequiredLevel, 58, 70);
                break;
            case ContentEra::WotLK:
                budget.effectiveRequiredLevel = layout.MapAuthoredToEffective(
                    ContentEra::WotLK, proto->RequiredLevel, 68, 80);
                break;
            default:
                budget.effectiveRequiredLevel = std::min<uint32>(proto->RequiredLevel, layout.maxLevel);
                break;
        }
    }

    budget.effectiveRequiredLevel = std::min<uint32>(budget.effectiveRequiredLevel, layout.maxLevel);

    // If realm cap is 80 and all eras active, stock budgets are valid identity
    if (layout.maxLevel == 80 && layout.tbcEnabled && layout.wotlkEnabled)
        return budget;

    // 2. Monotonic Power Bands for compressed realms driven by ContentTier
    uint32 newIlvl = proto->ItemLevel;

    if (ctx.era == ContentEra::Classic)
    {
        LevelRange const& cr = layout.classic;
        if (cr.maxLevel < 60)
        {
            switch (ctx.tier)
            {
                case ContentTier::WORLD:
                case ContentTier::DUNGEON_NORMAL:
                    newIlvl = MapThroughBand(proto->ItemLevel, { 1, 60, 1, cr.maxLevel });
                    break;
                case ContentTier::DUNGEON_HEROIC:
                case ContentTier::RAID_ENTRY: // MC, Onyxia, ZG
                    newIlvl = MapThroughBand(proto->ItemLevel, { 61, 68, uint32(cr.maxLevel + 1), uint32(cr.maxLevel + 6) });
                    break;
                case ContentTier::RAID_MID: // BWL
                    newIlvl = MapThroughBand(proto->ItemLevel, { 69, 75, uint32(cr.maxLevel + 7), uint32(cr.maxLevel + 13) });
                    break;
                case ContentTier::RAID_END: // AQ40
                    newIlvl = MapThroughBand(proto->ItemLevel, { 76, 83, uint32(cr.maxLevel + 14), uint32(cr.maxLevel + 19) });
                    break;
                case ContentTier::RAID_PINNACLE: // Naxx40
                default:
                    newIlvl = MapThroughBand(proto->ItemLevel, { 84, 92, uint32(cr.maxLevel + 20), uint32(cr.maxLevel + 25) });
                    break;
            }
        }
    }
    else if (ctx.era == ContentEra::TBC && layout.tbc.has_value())
    {
        LevelRange const& tr = *layout.tbc;
        switch (ctx.tier)
        {
            case ContentTier::WORLD:
            case ContentTier::DUNGEON_NORMAL:
                newIlvl = MapThroughBand(proto->ItemLevel, { 85, 115, tr.minLevel, tr.maxLevel });
                break;
            case ContentTier::DUNGEON_HEROIC:
                newIlvl = MapThroughBand(proto->ItemLevel, { 115, 120, uint32(tr.maxLevel + 1), uint32(tr.maxLevel + 3) });
                break;
            case ContentTier::RAID_ENTRY: // Karazhan, Gruul, Magtheridon (T4)
                newIlvl = MapThroughBand(proto->ItemLevel, { 121, 128, uint32(tr.maxLevel + 4), uint32(tr.maxLevel + 8) });
                break;
            case ContentTier::RAID_MID: // SSC, TK (T5)
                newIlvl = MapThroughBand(proto->ItemLevel, { 129, 141, uint32(tr.maxLevel + 9), uint32(tr.maxLevel + 16) });
                break;
            case ContentTier::RAID_END: // Hyjal, BT (T6)
                newIlvl = MapThroughBand(proto->ItemLevel, { 142, 154, uint32(tr.maxLevel + 17), uint32(tr.maxLevel + 22) });
                break;
            case ContentTier::RAID_PINNACLE: // Sunwell Plateau
            default:
                newIlvl = MapThroughBand(proto->ItemLevel, { 155, 164, uint32(tr.maxLevel + 23), uint32(tr.maxLevel + 26) });
                break;
        }
    }
    else if (ctx.era == ContentEra::WotLK && layout.wotlk.has_value())
    {
        LevelRange const& wr = *layout.wotlk;
        switch (ctx.tier)
        {
            case ContentTier::WORLD:
            case ContentTier::DUNGEON_NORMAL:
                newIlvl = MapThroughBand(proto->ItemLevel, { 138, 187, wr.minLevel, wr.maxLevel });
                break;
            case ContentTier::DUNGEON_HEROIC:
                newIlvl = MapThroughBand(proto->ItemLevel, { 188, 200, uint32(wr.maxLevel + 1), uint32(wr.maxLevel + 4) });
                break;
            case ContentTier::RAID_ENTRY: // Naxxramas, OS, EoE, VoA (Tier 7)
                newIlvl = MapThroughBand(proto->ItemLevel, { 200, 226, uint32(wr.maxLevel + 5), uint32(wr.maxLevel + 9) });
                break;
            case ContentTier::RAID_MID: // Ulduar (Tier 8)
                newIlvl = MapThroughBand(proto->ItemLevel, { 227, 252, uint32(wr.maxLevel + 10), uint32(wr.maxLevel + 18) });
                break;
            case ContentTier::RAID_END: // Trial of the Crusader / Onyxia (Tier 9)
                newIlvl = MapThroughBand(proto->ItemLevel, { 253, 258, uint32(wr.maxLevel + 19), uint32(wr.maxLevel + 26) });
                break;
            case ContentTier::RAID_PINNACLE: // Icecrown Citadel & Ruby Sanctum (Tier 10)
            default:
                newIlvl = MapThroughBand(proto->ItemLevel, { 259, 284, uint32(wr.maxLevel + 27), uint32(wr.maxLevel + 36) });
                break;
        }
    }

    budget.effectiveItemLevel = newIlvl;

    if (proto->ItemLevel > 0 && budget.effectiveItemLevel < proto->ItemLevel)
    {
        float const ilvlRatio = float(budget.effectiveItemLevel) / float(proto->ItemLevel);
        budget.statMultiplier = std::clamp<float>(ilvlRatio, 0.20f, 1.0f);

        // Combat rating scaling: ratings provide inflated % at lower level without deflation
        float const levelRatio = (proto->RequiredLevel > 0) ?
            (float(budget.effectiveRequiredLevel) / float(proto->RequiredLevel)) : ilvlRatio;
        budget.ratingMultiplier = std::clamp<float>(budget.statMultiplier * (levelRatio * levelRatio), 0.15f, 1.0f);

        budget.armorMultiplier = budget.statMultiplier;
        budget.weaponDpsMultiplier = budget.statMultiplier;
        budget.spellPowerMultiplier = budget.statMultiplier;
    }

    return budget;
}

void ItemBudgetScaler::ScaleItemTemplate(ItemTemplate* proto, ProgressionLayout const& layout) const
{
    if (!proto)
        return;

    ItemScalingContext const ctx = ItemScalingContext::Resolve(proto);
    if (ctx.policy == ItemScalingPolicy::PRESERVE)
        return;

    ScaledItemBudget const budget = CalculateItemBudget(proto, layout, ctx);

    proto->RequiredLevel = budget.effectiveRequiredLevel;
    proto->ItemLevel     = budget.effectiveItemLevel;

    if (budget.statMultiplier < 1.0f)
    {
        for (uint32 i = 0; i < proto->StatsCount; ++i)
        {
            if (proto->ItemStat[i].ItemStatValue != 0)
            {
                ItemModCategory const category = GetItemModCategory(proto->ItemStat[i].ItemStatType);
                float mult = budget.statMultiplier;

                if (category == ItemModCategory::SecondaryRating)
                    mult = budget.ratingMultiplier;
                else if (category == ItemModCategory::SpellPower)
                    mult = budget.spellPowerMultiplier;

                proto->ItemStat[i].ItemStatValue = static_cast<int32>(
                    std::round(float(proto->ItemStat[i].ItemStatValue) * mult));
            }
        }

        // Scale weapon damage
        for (uint32 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
        {
            if (proto->Damage[i].DamageMax > 0.0f)
            {
                proto->Damage[i].DamageMin *= budget.weaponDpsMultiplier;
                proto->Damage[i].DamageMax *= budget.weaponDpsMultiplier;
            }
        }

        // Scale armor
        if (proto->Armor > 0)
        {
            proto->Armor = static_cast<uint32>(
                std::round(float(proto->Armor) * budget.armorMultiplier));
        }
    }
}

void ItemBudgetScaler::ScaleAllItems(ProgressionLayout const& layout)
{
    if (_itemsScaled)
    {
        LOG_WARN("module.coa_content_scaling", "ItemBudgetScaler::ScaleAllItems skipped: already scaled (idempotent protection)");
        return;
    }

    ItemTemplateContainer const* itemTemplates = sObjectMgr->GetItemTemplateStore();
    if (!itemTemplates)
        return;

    uint32 totalTemplates = 0;
    uint32 generatedProfilesUsed = 0;
    uint32 fallbackInferred = 0;
    uint32 preservedCustom = 0;
    uint32 specialReview = 0;
    uint32 actuallyMutated = 0;

    for (auto& [itemId, itemTemplate] : *itemTemplates)
    {
        ++totalTemplates;
        ItemTemplate* proto = const_cast<ItemTemplate*>(&itemTemplate);
        ItemScalingContext const ctx = ItemScalingContext::Resolve(proto);

        if (ctx.hasGeneratedProfile)
            ++generatedProfilesUsed;
        if (ctx.fallbackTierInference)
            ++fallbackInferred;
        if (ctx.policy == ItemScalingPolicy::PRESERVE)
            ++preservedCustom;
        if (ctx.policy == ItemScalingPolicy::REVIEW_SPECIAL)
            ++specialReview;

        if (ctx.policy != ItemScalingPolicy::PRESERVE)
        {
            ScaledItemBudget const budget = CalculateItemBudget(proto, layout, ctx);
            if (budget.statMultiplier < 1.0f || budget.effectiveRequiredLevel != proto->RequiredLevel || budget.effectiveItemLevel != proto->ItemLevel)
            {
                ScaleItemTemplate(proto, layout);
                ++actuallyMutated;
            }

            if (budget.statMultiplier < 1.0f)
                _appliedMultipliers[proto->ItemId] = { budget.statMultiplier, budget.ratingMultiplier };
        }
    }

    _itemsScaled = true;

    LOG_INFO("server.loading", ">> UniversalContentScaling: Item scaling summary:");
    LOG_INFO("server.loading", "   Total templates: {}", totalTemplates);
    LOG_INFO("server.loading", "   Generated profiles: {}", generatedProfilesUsed);
    LOG_INFO("server.loading", "   Fallback inferred: {}", fallbackInferred);
    LOG_INFO("server.loading", "   Preserved custom: {}", preservedCustom);
    LOG_INFO("server.loading", "   Special-review: {}", specialReview);
    LOG_INFO("server.loading", "   Actually mutated: {}", actuallyMutated);
}

float ItemBudgetScaler::GetStatMultiplier(uint32 itemEntry) const
{
    auto const itr = _appliedMultipliers.find(itemEntry);
    return itr != _appliedMultipliers.end() ? itr->second.first : 1.0f;
}

float ItemBudgetScaler::GetRatingMultiplier(uint32 itemEntry) const
{
    auto const itr = _appliedMultipliers.find(itemEntry);
    return itr != _appliedMultipliers.end() ? itr->second.second : 1.0f;
}
