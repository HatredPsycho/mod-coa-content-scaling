/*
 * CoA Universal Content Scaling
 * ItemBudgetScaler: Implementation of item stat, rating and level budget scaling.
 */

#include "ItemBudgetScaler.h"
#include "LocalLevelScaling.h"
#include "ContentPackRegistry.h"
#include "GeneratedContentCensus.h"
#include "GeneratedCustomRanges.h"
#include "InstanceProfile.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectMgr.h"
#include <algorithm>
#include <cmath>
#include <optional>

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

    // Where each tier's authored item levels land on a compressed realm. Classic only moves when its
    // range is shorter than the 60 levels it was written for.
    std::optional<PowerBandMapping> PowerBand(ContentEra era, ContentTier tier, ProgressionLayout const& layout)
    {
        if (era == ContentEra::Classic)
        {
            LevelRange const& cr = layout.classic;
            if (cr.maxLevel >= 60)
                return std::nullopt;

            switch (tier)
            {
                case ContentTier::WORLD:
                case ContentTier::DUNGEON_NORMAL:
                    return PowerBandMapping{ 1, 60, 1, cr.maxLevel };
                case ContentTier::DUNGEON_HEROIC:
                case ContentTier::RAID_ENTRY: // MC, Onyxia, ZG
                    return PowerBandMapping{ 61, 68, uint32(cr.maxLevel + 1), uint32(cr.maxLevel + 6) };
                case ContentTier::RAID_MID: // BWL
                    return PowerBandMapping{ 69, 75, uint32(cr.maxLevel + 7), uint32(cr.maxLevel + 13) };
                case ContentTier::RAID_END: // AQ40
                    return PowerBandMapping{ 76, 83, uint32(cr.maxLevel + 14), uint32(cr.maxLevel + 19) };
                case ContentTier::RAID_PINNACLE: // Naxx40
                default:
                    return PowerBandMapping{ 84, 92, uint32(cr.maxLevel + 20), uint32(cr.maxLevel + 25) };
            }
        }

        if (era == ContentEra::TBC && layout.tbc.has_value())
        {
            LevelRange const& tr = *layout.tbc;
            switch (tier)
            {
                case ContentTier::WORLD:
                case ContentTier::DUNGEON_NORMAL:
                    return PowerBandMapping{ 85, 115, tr.minLevel, tr.maxLevel };
                case ContentTier::DUNGEON_HEROIC:
                    return PowerBandMapping{ 115, 120, uint32(tr.maxLevel + 1), uint32(tr.maxLevel + 3) };
                case ContentTier::RAID_ENTRY: // Karazhan, Gruul, Magtheridon (T4)
                    return PowerBandMapping{ 121, 128, uint32(tr.maxLevel + 4), uint32(tr.maxLevel + 8) };
                case ContentTier::RAID_MID: // SSC, TK (T5)
                    return PowerBandMapping{ 129, 141, uint32(tr.maxLevel + 9), uint32(tr.maxLevel + 16) };
                case ContentTier::RAID_END: // Hyjal, BT (T6)
                    return PowerBandMapping{ 142, 154, uint32(tr.maxLevel + 17), uint32(tr.maxLevel + 22) };
                case ContentTier::RAID_PINNACLE: // Sunwell Plateau
                default:
                    return PowerBandMapping{ 155, 164, uint32(tr.maxLevel + 23), uint32(tr.maxLevel + 26) };
            }
        }

        if (era == ContentEra::WotLK && layout.wotlk.has_value())
        {
            LevelRange const& wr = *layout.wotlk;
            switch (tier)
            {
                case ContentTier::WORLD:
                case ContentTier::DUNGEON_NORMAL:
                    return PowerBandMapping{ 138, 187, wr.minLevel, wr.maxLevel };
                case ContentTier::DUNGEON_HEROIC:
                    return PowerBandMapping{ 188, 200, uint32(wr.maxLevel + 1), uint32(wr.maxLevel + 4) };
                case ContentTier::RAID_ENTRY: // Naxxramas, OS, EoE, VoA (Tier 7)
                    return PowerBandMapping{ 200, 226, uint32(wr.maxLevel + 5), uint32(wr.maxLevel + 9) };
                case ContentTier::RAID_MID: // Ulduar (Tier 8)
                    return PowerBandMapping{ 227, 252, uint32(wr.maxLevel + 10), uint32(wr.maxLevel + 18) };
                case ContentTier::RAID_END: // Trial of the Crusader / Onyxia (Tier 9)
                    return PowerBandMapping{ 253, 258, uint32(wr.maxLevel + 19), uint32(wr.maxLevel + 26) };
                case ContentTier::RAID_PINNACLE: // Icecrown Citadel & Ruby Sanctum (Tier 10)
                default:
                    return PowerBandMapping{ 259, 284, uint32(wr.maxLevel + 27), uint32(wr.maxLevel + 36) };
            }
        }

        return std::nullopt;
    }
}

ItemScalingContext ItemScalingContext::Resolve(ItemTemplate const* proto)
{
    ItemScalingContext ctx;
    if (!proto)
        return ctx;

    // 0. A catalog that ships one item per level has already answered the question this pass asks,
    // and the module that owns those entries says so itself. Rewriting them applies the band twice.
    if (LocalLevelScaling::IsLevelResolvedItem(proto->ItemId))
    {
        ctx.specialFlags |= ITEM_SPECIAL_CUSTOM | ITEM_SPECIAL_PRESERVE;
        ctx.era = ContentEra::Custom;
        ctx.tier = ContentTier::WORLD;
        ctx.policy = ItemScalingPolicy::PRESERVE;
        return ctx;
    }

    // 1. Retain generated source and safety metadata; resolve era through registry priorities.
    if (auto const* prof = FindGeneratedItemProfile(proto->ItemId))
    {
        ctx.hasGeneratedProfile = true;
        ctx.era = sContentPackRegistry->ResolveEraForItem(proto->ItemId, proto->ItemLevel, proto->RequiredLevel);
        ctx.tier = prof->tier;
        ctx.sourceMap = prof->sourceMap;
        ctx.specialFlags = prof->specialFlags;
        ctx.policy = static_cast<ItemScalingPolicy>(prof->policy);

        return ctx;
    }

    // 2. Custom items, classified by the ranges data/content/overrides/custom_content.json names.
    //
    // That file called itself the authoritative source and only the census generator ever read it,
    // which needs a database - so the runtime had nothing and preserved every custom item above
    // 100000. On a realm whose own items outnumber the stock ones four to one that meant the pass
    // skipped most of what players wear, the Bloodforged and Heroic versions included. The ranges
    // are generated into a header now, so they stay in the file and the boundaries stay out of here.
    if (GeneratedCustomRange const* range = FindGeneratedCustomRange(proto->ItemId))
    {
        ctx.specialFlags |= ITEM_SPECIAL_CUSTOM;
        ctx.sourceMap = 0;

        if (static_cast<ItemScalingPolicy>(range->policy) == ItemScalingPolicy::PRESERVE)
        {
            ctx.specialFlags |= ITEM_SPECIAL_PRESERVE;
            ctx.era = ContentEra::Custom;
            ctx.tier = ContentTier::WORLD;
            ctx.policy = ItemScalingPolicy::PRESERVE;
            return ctx;
        }
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
    //
    // What an item asks of its wearer tracks where the item belongs, so it is read the same way a
    // creature's or a quest's level is: an expansion's starting zone rewards gear for the levels it
    // was written for, and reading that as the expansion's own content would hand a level 5 Blood
    // Elf a reward they cannot put on.
    if (proto->RequiredLevel > 0)
    {
        ContentEra const levelEra = layout.ResolveContentEra(ctx.era, static_cast<uint8>(proto->RequiredLevel));

        if (ProgressionLayout::CanonicalAuthoredRange(levelEra).minLevel)
            budget.effectiveRequiredLevel = layout.MapAuthoredToEffective(
                levelEra, static_cast<uint8>(proto->RequiredLevel));
        else
            budget.effectiveRequiredLevel = std::min<uint32>(proto->RequiredLevel, layout.maxLevel);
    }

    budget.effectiveRequiredLevel = std::min<uint32>(budget.effectiveRequiredLevel, layout.maxLevel);

    // If realm cap is 80 and all eras active, stock budgets are valid identity
    if (layout.IsStockIdentity())
        return budget;

    // 2. Monotonic Power Bands for compressed realms driven by ContentTier
    uint32 newIlvl = proto->ItemLevel;
    if (std::optional<PowerBandMapping> const band = PowerBand(ctx.era, ctx.tier, layout))
        newIlvl = MapThroughBand(proto->ItemLevel, *band);

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

uint32 ItemBudgetScaler::ScaleRequiredAverageItemLevel(uint32 authoredItemLevel, ContentEra era,
                                                      ProgressionLayout const& layout) const
{
    if (!authoredItemLevel || layout.IsStockIdentity())
        return authoredItemLevel;

    // A requirement is met with gear, so it is read through the band of the content whose drops
    // reach that level - the lowest tier that does. Reading it through the tier of the instance that
    // asks would demand heroic gear before the heroic could be entered to get it.
    static constexpr ContentTier sources[] =
    {
        ContentTier::DUNGEON_NORMAL, ContentTier::DUNGEON_HEROIC, ContentTier::RAID_ENTRY,
        ContentTier::RAID_MID, ContentTier::RAID_END, ContentTier::RAID_PINNACLE
    };

    std::optional<PowerBandMapping> highest;
    for (ContentTier const tier : sources)
    {
        std::optional<PowerBandMapping> const band = PowerBand(era, tier, layout);
        if (!band)
            continue;

        if (authoredItemLevel <= band->authMax)
            return MapThroughBand(authoredItemLevel, *band);

        highest = band;
    }

    return highest ? MapThroughBand(authoredItemLevel, *highest) : authoredItemLevel;
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
            uint32 const authoredRequiredLevel = proto->RequiredLevel;
            uint32 const authoredItemLevel = proto->ItemLevel;

            if (budget.statMultiplier < 1.0f || budget.effectiveRequiredLevel != authoredRequiredLevel ||
                budget.effectiveItemLevel != authoredItemLevel)
            {
                ScaleItemTemplate(proto, layout);
                ++actuallyMutated;

                _appliedScaling[proto->ItemId] = { authoredRequiredLevel, budget.effectiveRequiredLevel,
                    authoredItemLevel, budget.effectiveItemLevel, budget.statMultiplier, budget.ratingMultiplier };
            }
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

AppliedItemScaling const* ItemBudgetScaler::FindAppliedScaling(uint32 itemEntry) const
{
    auto const itr = _appliedScaling.find(itemEntry);
    return itr != _appliedScaling.end() ? &itr->second : nullptr;
}

float ItemBudgetScaler::GetStatMultiplier(uint32 itemEntry) const
{
    AppliedItemScaling const* applied = FindAppliedScaling(itemEntry);
    return applied ? applied->statMultiplier : 1.0f;
}

float ItemBudgetScaler::GetRatingMultiplier(uint32 itemEntry) const
{
    AppliedItemScaling const* applied = FindAppliedScaling(itemEntry);
    return applied ? applied->ratingMultiplier : 1.0f;
}
