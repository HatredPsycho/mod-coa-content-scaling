/*
 * CoA Universal Content Scaling
 * CombatBudgetProfile: Implementation of budget normalization and creature scaling.
 */

#include "CombatBudgetProfile.h"
#include "ContentEraResolver.h"
#include "Creature.h"
#include "CreatureData.h"
#include "Map.h"
#include "ObjectMgr.h"
#include <algorithm>
#include <cmath>

CombatBudgetProfile* CombatBudgetProfile::Instance()
{
    static CombatBudgetProfile instance;
    return &instance;
}

float CombatBudgetProfile::GetTierHealthMultiplier(ContentTier tier)
{
    switch (tier)
    {
        case ContentTier::WORLD:          return 1.0f;
        case ContentTier::DUNGEON_NORMAL: return 1.35f;
        case ContentTier::DUNGEON_HEROIC: return 2.20f;
        case ContentTier::RAID_ENTRY:     return 3.50f;
        case ContentTier::RAID_MID:       return 4.50f;
        case ContentTier::RAID_END:       return 6.00f;
        case ContentTier::RAID_PINNACLE:  return 8.00f;
        default:                          return 1.0f;
    }
}

float CombatBudgetProfile::GetTierDamageMultiplier(ContentTier tier)
{
    switch (tier)
    {
        case ContentTier::WORLD:          return 1.0f;
        case ContentTier::DUNGEON_NORMAL: return 1.15f;
        case ContentTier::DUNGEON_HEROIC: return 1.45f;
        case ContentTier::RAID_ENTRY:     return 1.80f;
        case ContentTier::RAID_MID:       return 2.10f;
        case ContentTier::RAID_END:       return 2.50f;
        case ContentTier::RAID_PINNACLE:  return 3.00f;
        default:                          return 1.0f;
    }
}

float CombatBudgetProfile::GetTierArmorMultiplier(ContentTier tier)
{
    switch (tier)
    {
        case ContentTier::WORLD:          return 1.0f;
        case ContentTier::DUNGEON_NORMAL: return 1.05f;
        case ContentTier::DUNGEON_HEROIC: return 1.15f;
        case ContentTier::RAID_ENTRY:     return 1.25f;
        case ContentTier::RAID_MID:       return 1.30f;
        case ContentTier::RAID_END:       return 1.35f;
        case ContentTier::RAID_PINNACLE:  return 1.40f;
        default:                          return 1.0f;
    }
}

CreatureScaleContext CombatBudgetProfile::BuildContext(CreatureTemplate const* cinfo,
                                                      Creature const* creature,
                                                      uint8 effectiveLevel) const
{
    CreatureScaleContext context;
    if (!cinfo)
        return context;

    context.authoredLevel = cinfo->maxlevel;
    context.effectiveLevel = effectiveLevel;

    Map const* map = creature ? creature->GetMap() : nullptr;
    uint32 const mapId = map ? map->GetId() : 0;
    uint32 const areaId = creature ? creature->GetAreaId() : 0;

    // Resolve era
    context.era = sContentEraResolver->ResolveEraForCreature(
        cinfo->Entry, mapId, areaId, cinfo->expansion, cinfo->maxlevel);

    if (map)
    {
        context.dungeon = map->IsDungeon();
        context.raid    = map->IsRaid();
        context.heroic  = map->IsHeroic();
    }

    context.boss = (cinfo->rank == CREATURE_ELITE_WORLDBOSS) ||
                   (context.raid && cinfo->rank == CREATURE_ELITE_RAREELITE) ||
                   (context.dungeon && cinfo->rank == CREATURE_ELITE_ELITE && cinfo->maxlevel >= context.authoredLevel);

    context.worldBoss = (cinfo->rank == CREATURE_ELITE_WORLDBOSS && !context.raid);

    // Determine tier
    if (context.raid)
    {
        if (context.boss)
            context.tier = context.heroic ? ContentTier::RAID_END : ContentTier::RAID_MID;
        else
            context.tier = ContentTier::RAID_ENTRY;
    }
    else if (context.dungeon)
    {
        context.tier = context.heroic ? ContentTier::DUNGEON_HEROIC : ContentTier::DUNGEON_NORMAL;
    }
    else
    {
        context.tier = ContentTier::WORLD;
    }

    return context;
}

CalculatedCombatBudget CombatBudgetProfile::CalculateBudget(CreatureTemplate const* cinfo,
                                                           CreatureScaleContext const& context) const
{
    CalculatedCombatBudget budget;
    if (!cinfo)
        return budget;

    // Retrieve target base stats at the effective level
    CreatureBaseStats const* targetStats = sObjectMgr->GetCreatureBaseStats(context.effectiveLevel, cinfo->unit_class);
    if (!targetStats)
        return budget;

    // Base stats in creature_classlevelstats have columns for expansions:
    // We normalize to expansion 0 (CoA standard budget) to avoid WotLK column inflation at compressed levels:
    uint32 const baseHealth = targetStats->BaseHealth[0];
    float const baseDamage  = targetStats->BaseDamage[0];
    float const baseArmor   = targetStats->BaseArmor;
    uint32 const baseMana   = targetStats->BaseMana;

    float const tierHealthMod = GetTierHealthMultiplier(context.tier);
    float const tierDmgMod    = GetTierDamageMultiplier(context.tier);
    float const tierArmorMod  = GetTierArmorMultiplier(context.tier);

    // Health: base * authored multiplier * tier modifier * group scale
    float finalHealth = float(baseHealth) * cinfo->ModHealth * tierHealthMod * context.groupHealthScale;
    if (cinfo->rank == CREATURE_ELITE_ELITE)
        finalHealth *= 2.0f;
    else if (cinfo->rank == CREATURE_ELITE_WORLDBOSS)
        finalHealth *= 4.0f;

    budget.health = std::max<uint32>(1, static_cast<uint32>(std::ceil(finalHealth)));

    // Mana: base * authored multiplier
    if (baseMana > 0)
    {
        float const finalMana = float(baseMana) * cinfo->ModMana;
        budget.mana = static_cast<uint32>(std::ceil(finalMana));
    }
    else
    {
        budget.mana = 0;
    }

    // Armor: base * authored multiplier * tier modifier
    budget.armor = std::ceil(baseArmor * cinfo->ModArmor * tierArmorMod);

    // Damage: Base weapon damage is calculated strictly from base stats, authored modifier and tier.
    // Group scaling is applied at runtime in damage modification hooks to eliminate double scaling.
    float effectiveDmgMod = cinfo->DamageModifier * tierDmgMod;
    if (effectiveDmgMod <= 0.0f)
        effectiveDmgMod = 1.0f;

    float const baseMinDmg = baseDamage * effectiveDmgMod;
    budget.minDamage = std::max(1.0f, baseMinDmg);
    budget.maxDamage = std::max(budget.minDamage, baseMinDmg * 1.5f);

    budget.attackPower = targetStats->AttackPower;

    return budget;
}
