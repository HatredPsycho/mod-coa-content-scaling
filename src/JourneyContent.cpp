/*
 * CoA Universal Content Scaling
 * JourneyContent: Implementation of the journey layout on the loaded world.
 *
 * What it does follows mod-world-journey by aspollon (JourneyDb.cpp and JourneyServer.cpp, faed6d6, GNU AGPL v3),
 * on the loaded templates instead of the world database.
 */

#include "JourneyContent.h"
#include "BattlegroundMgr.h"
#include "Creature.h"
#include "CreatureData.h"
#include "DBCStores.h"
#include "ItemBudgetScaler.h"
#include "ItemTemplate.h"
#include "JourneyAddon.h"
#include "JourneyData.h"
#include "LFGMgr.h"
#include "Map.h"
#include "MapMgr.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuestDef.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Trainer.h"
#include "World.h"
#include <algorithm>
#include <map>

namespace
{
    uint64 InstanceKey(uint32 mapId, uint8 difficulty)
    {
        return (uint64(mapId) << 8) | difficulty;
    }

    // The level a dungeon is meant for, a few levels above its entry as it was before the journey (the journey is
    // about two thirds as long as the original road).
    uint8 Recommended(LFGDungeonEntry const* dbc, uint8 minLevel, uint8 maxLevel)
    {
        int const above = std::max<int>(0, int(dbc->TargetLevel) - int(dbc->MinLevel));
        return uint8(std::clamp<int>(minLevel + int(std::lround(above * 0.65)), minLevel, maxLevel));
    }

    int EntryOf(journey::Rules const& rules, journey::InstanceData const& instance, bool manual = true)
    {
        return rules.InstanceEntry(instance.map, journey::InstanceKind(instance.kind), instance.originalEntry,
            instance.contentLevel, manual);
    }
}

JourneyContent* JourneyContent::Instance()
{
    static JourneyContent instance;
    return &instance;
}

JourneyBuildReport JourneyContent::Build(journey::Rules const& rules, JourneySettings const& settings,
    uint8 maxPlayerLevel, std::function<bool(uint32, uint8)> const& keepMap)
{
    JourneyBuildReport report;
    if (IsBuilt())
        return report;

    _settings = settings;
    _maxPlayerLevel = maxPlayerLevel;
    _rules = rules;

    BuildPlaces(rules);
    BuildCreatures(rules, settings, keepMap, report);
    RelevelLoadedCreatures(report);
    BuildQuests(rules, report);
    BuildDungeonFinder(report);
    if (settings.trainers)
        BuildTrainers(rules, report);
    if (settings.battlegrounds)
        BuildBattlegrounds(rules, report);
    if (settings.enchantments)
        CollectEnchantmentSources();

    _built.store(true, std::memory_order_release);
    return report;
}

void JourneyContent::BuildPlaces(journey::Rules const& rules)
{
    for (journey::ZoneData const& zone : journey::Zones)
    {
        _zones[zone.zone] = { zone.originalEntry, zone.part };
        if (int const delta = rules.ZoneEntry(zone.zone, zone.part, zone.originalEntry, true) -
            rules.ZoneEntry(zone.zone, zone.part, zone.originalEntry, false))
            _zoneShifts[zone.zone] = delta;
    }

    for (journey::BotZoneData const& zone : journey::BotZones)
        _zones.try_emplace(zone.zone, ZonePlace{ zone.originalMin, zone.part });

    for (journey::InstanceData const& instance : journey::Instances)
    {
        _instanceEntries[InstanceKey(instance.map, instance.difficulty)] = uint8(EntryOf(rules, instance));
        if (instance.difficulty == 0)
            if (int const delta = EntryOf(rules, instance, true) - EntryOf(rules, instance, false))
                _mapShifts[instance.map] = delta;
    }
}

void JourneyContent::BuildCreatures(journey::Rules const& rules, JourneySettings const& settings,
    std::function<bool(uint32, uint8)> const& keepMap, JourneyBuildReport& report)
{
    // The map most spawns of each creature are on.
    std::unordered_map<uint32, std::unordered_map<uint32, uint32>> spawnCounts;
    for (auto const& [spawnId, data] : sObjectMgr->GetAllCreatureData())
        for (uint32 entry : { data.id, data.id2, data.id3 })
            if (entry)
                ++spawnCounts[entry][data.mapid];

    for (auto const& [entry, counts] : spawnCounts)
    {
        auto const most = std::max_element(counts.begin(), counts.end(),
            [](auto const& a, auto const& b) { return a.second < b.second; });
        _spawnMaps[entry] = most->first;
    }

    CreatureTemplateContainer const* templates = sObjectMgr->GetCreatureTemplates();
    if (!templates)
        return;

    // A template of a heroic or raid difficulty -> its normal one.
    std::unordered_map<uint32, uint32> heroicOf;
    for (auto const& [entry, cinfo] : *templates)
        for (uint32 other : cinfo.DifficultyEntry)
            if (other)
                heroicOf[other] = entry;

    for (auto const& [entry, constInfo] : *templates)
    {
        ++report.creatureTemplates;
        auto* cinfo = const_cast<CreatureTemplate*>(&constInfo);

        auto const heroic = heroicOf.find(entry);
        uint32 const spawnedAs = heroic != heroicOf.end() ? heroic->second : entry;
        auto const map = _spawnMaps.find(spawnedAs);
        bool const spawned = map != _spawnMaps.end();
        if (spawned && keepMap(map->second, heroic != heroicOf.end() ? 1 : 0))
            continue;

        int const originalMin = cinfo->minlevel;
        int const originalMax = cinfo->maxlevel;
        uint8 const originalExpansion = uint8(cinfo->expansion);
        journey::Era const era = journey::CreatureEra(spawned, spawned ? map->second : 0, originalMax,
            heroic != heroicOf.end(), cinfo->rank == CREATURE_ELITE_WORLDBOSS);
        journey::CreaturePlan const plan = rules.PlanCreature(originalMin, originalMax, era);
        if (plan.minLevel == originalMin && plan.maxLevel == originalMax)
            continue;

        cinfo->minlevel = uint8(plan.minLevel);
        cinfo->maxlevel = uint8(plan.maxLevel);
        cinfo->mingold = journey::ScaleMoney(cinfo->mingold, originalMin, plan.minLevel);
        cinfo->maxgold = journey::ScaleMoney(cinfo->maxgold, originalMax, plan.maxLevel);

        // On the way, a creature of Outland or Northrend is a creature of the journey: the base stats of the old
        // world, which go up to 60 without the jumps of the later tables. The raids and heroics keep theirs.
        bool const onTheWay =
            era == journey::ERA_CLASSIC || era == journey::ERA_OUTLAND || era == journey::ERA_NORTHREND;
        if (settings.journeyBaseStats && onTheWay)
            cinfo->expansion = 0;

        _shifts[entry] = { uint8(originalMin), uint8(originalMax), uint8(plan.minLevel), uint8(plan.maxLevel),
            originalExpansion, uint8(cinfo->expansion) };
        ++report.creaturesMoved;
    }
}

void JourneyContent::RelevelLoadedCreatures(JourneyBuildReport& report)
{
    // The transports, the battlefields and outdoor PvP load their grids before the world is initialized, so some
    // creatures were made from the authored templates; they are made again from the journey's.
    sMapMgr->DoForAllMaps([this, &report](Map* map)
    {
        for (auto const& [spawnId, creature] : map->GetCreatureBySpawnIdStore())
        {
            if (!creature || !_shifts.count(creature->GetEntry()))
                continue;

            creature->UpdateEntry(creature->GetEntry(), creature->GetCreatureData(), true);
            if (creature->IsAlive())
                creature->SetFullHealth();
            ++report.creaturesRelevelled;
        }
    });
}

void JourneyContent::BuildQuests(journey::Rules const& rules, JourneyBuildReport& report)
{
    // The map of whoever or whatever starts a quest.
    std::unordered_map<uint32, uint32> starterMaps;
    for (auto const& [creatureEntry, questId] : *sObjectMgr->GetCreatureQuestRelationMap())
    {
        auto const map = _spawnMaps.find(creatureEntry);
        if (map != _spawnMaps.end())
            starterMaps.emplace(questId, map->second);
    }

    std::unordered_map<uint32, uint32> objectMaps;
    for (auto const& [spawnId, data] : sObjectMgr->GetAllGOData())
    {
        uint32& map = objectMaps[data.id];
        map = std::max<uint32>(map, data.mapid);
    }
    for (auto const& [objectEntry, questId] : *sObjectMgr->GetGOQuestRelationMap())
    {
        auto const map = objectMaps.find(objectEntry);
        if (map != objectMaps.end())
            starterMaps.emplace(questId, map->second);
    }

    for (auto const& [questId, quest] : sObjectMgr->GetQuestTemplates())
    {
        if (!quest)
            continue;

        // The reward is read as the journey will have it: its items are laid over the journey before anyone can
        // take the quest, so an Outland green made for 60 asks for Outland's 33, not for 60.
        int rewardLevel = 0;
        auto considerReward = [&rewardLevel, &rules](uint32 itemId)
        {
            if (!itemId)
                return;
            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
            bool const equipment = proto && (proto->Class == ITEM_CLASS_WEAPON || proto->Class == ITEM_CLASS_ARMOR);
            if (!equipment || proto->RequiredLevel <= 1)
                return;

            int required = int(proto->RequiredLevel);
            if (ItemScalingContext::Resolve(proto).policy != ItemScalingPolicy::PRESERVE)
            {
                journey::ItemPlan const plan = rules.PlanItem(int(proto->Quality), required, int(proto->ItemLevel));
                if (plan.era != journey::ITEM_KEEP)
                    required = plan.requiredLevel;
            }
            rewardLevel = std::max(rewardLevel, required);
        };
        for (uint32 itemId : quest->RewardItemId)
            considerReward(itemId);
        for (uint32 itemId : quest->RewardChoiceItemId)
            considerReward(itemId);

        int const sort = quest->GetZoneOrSort();
        auto const shift = sort > 0 ? _zoneShifts.find(uint32(sort)) : _zoneShifts.end();
        auto const starter = starterMaps.find(questId);

        journey::QuestPlan const plan = rules.PlanQuest(quest->GetQuestLevel(), int(quest->GetMinLevel()),
            starter != starterMaps.end(), starter != starterMaps.end() ? starter->second : 0, rewardLevel,
            shift != _zoneShifts.end() ? shift->second : 0);
        _quests[questId] = plan;
        if (plan.level != quest->GetQuestLevel() || plan.minLevel != int(quest->GetMinLevel()))
            ++report.questsMoved;
    }
}

void JourneyContent::BuildDungeonFinder(JourneyBuildReport& report)
{
    // The Dungeon Finder offers every dungeon from its new entry level to the cap; a random dungeon from the lowest
    // entry among the dungeons it draws from.
    std::map<std::pair<uint8, uint8>, uint8> lowestOfGroup;
    std::vector<lfg::LFGDungeonData*> randoms;
    for (LFGDungeonEntry const* dbc : sLFGDungeonStore)
    {
        if (!dbc)
            continue;

        auto* dungeon = const_cast<lfg::LFGDungeonData*>(sLFGMgr->GetLFGDungeon(dbc->ID));
        if (!dungeon)
            continue;

        if (dungeon->type == lfg::LFG_TYPE_RANDOM)
        {
            randoms.push_back(dungeon);
            continue;
        }

        std::optional<uint8> const entry = GetInstanceEntry(dungeon->map, uint8(dungeon->difficulty));
        if (!entry)
            continue;

        dungeon->minlevel = *entry;
        dungeon->maxlevel = std::max<uint8>(dungeon->minlevel, _maxPlayerLevel);
        _dungeonLevels.push_back({ dbc->ID, dungeon->minlevel, dungeon->maxlevel,
            Recommended(dbc, dungeon->minlevel, dungeon->maxlevel) });
        uint8& lowest = lowestOfGroup.try_emplace({ dungeon->group, uint8(dungeon->difficulty) }, dungeon->minlevel)
            .first->second;
        lowest = std::min(lowest, dungeon->minlevel);
        ++report.dungeonFinderEntries;
    }

    for (lfg::LFGDungeonData* random : randoms)
    {
        auto const lowest = lowestOfGroup.find({ random->group, uint8(random->difficulty) });
        if (lowest == lowestOfGroup.end())
            continue;

        random->minlevel = lowest->second;
        random->maxlevel = std::max<uint8>(random->minlevel, _maxPlayerLevel);
        if (LFGDungeonEntry const* dbc = sLFGDungeonStore.LookupEntry(random->id))
            _dungeonLevels.push_back({ dbc->ID, random->minlevel, random->maxlevel,
                Recommended(dbc, random->minlevel, random->maxlevel) });
        ++report.dungeonFinderEntries;
    }
}

void JourneyContent::BuildTrainers(journey::Rules const& rules, JourneyBuildReport& report)
{
    // Riding and the professions; the class trainers teach the classes as CoA made them.
    for (auto const& [trainerId, trainer] : sObjectMgr->GetTrainers())
    {
        if (trainer.GetTrainerType() != Trainer::Type::Mount && trainer.GetTrainerType() != Trainer::Type::Tradeskill)
            continue;

        for (Trainer::Spell const& constSpell : trainer.GetSpells())
        {
            auto& spell = const_cast<Trainer::Spell&>(constSpell);
            int const level = rules.TrainerLevel(spell.SpellId, spell.ReqLevel);
            if (level != spell.ReqLevel)
            {
                spell.ReqLevel = uint8(std::clamp(level, 1, 255));
                ++report.trainerSpells;
            }
        }
    }
}

void JourneyContent::BuildBattlegrounds(journey::Rules const& rules, JourneyBuildReport& report)
{
    // The Eye of the Storm, the Strand of the Ancients, the Isle of Conquest: open at the endgame.
    for (BattlegroundTypeId const id : { BATTLEGROUND_EY, BATTLEGROUND_SA, BATTLEGROUND_IC })
    {
        Battleground* bg = sBattlegroundMgr->GetBattlegroundTemplate(id);
        if (!bg)
            continue;

        uint32 const level = uint32(rules.BattlegroundMinLevel(int(bg->GetMinLevel())));
        if (level < bg->GetMinLevel())
        {
            bg->SetLevelRange(level, bg->GetMaxLevel());
            ++report.battlegrounds;
        }
    }

    // Each battleground's first bracket takes a character of the endgame.
    uint32 const endgame = uint32(rules.GetShape().endgame);
    for (uint32 const mapId : { 30u, 489u, 529u, 566u, 607u, 628u })
    {
        PvPDifficultyEntry* first = nullptr;
        for (uint32 bracket = 0; bracket < MAX_BATTLEGROUND_BRACKETS; ++bracket)
            if (PvPDifficultyEntry const* entry = GetBattlegroundBracketById(mapId, BattlegroundBracketId(bracket)))
                if (!first || entry->minLevel < first->minLevel)
                    first = const_cast<PvPDifficultyEntry*>(entry);
        if (first && first->minLevel > endgame)
            first->minLevel = endgame;
    }
}

void JourneyContent::CollectEnchantmentSources()
{
    // Read from the authored templates, before the items are cut.
    for (auto const& [entry, proto] : *sObjectMgr->GetItemTemplateStore())
    {
        if (proto.Quality >= ITEM_QUALITY_ARTIFACT)
            continue;

        if (proto.GemProperties && proto.RequiredLevel > 1)
            if (GemPropertiesEntry const* gem = sGemPropertiesStore.LookupEntry(proto.GemProperties))
                _enchantmentEras[gem->spellitemenchantement] = journey::GemEra(entry);

        if (proto.socketBonus)
            _socketBonusItems[proto.socketBonus].push_back(entry);

        if (proto.Class == ITEM_CLASS_RECIPE && proto.RequiredSkillRank > 300)
            for (_Spell const& spell : proto.Spells)
                if (spell.SpellId > 0 && spell.SpellTrigger == ITEM_SPELLTRIGGER_LEARN_SPELL_ID)
                    if (SpellInfo const* info = sSpellMgr->GetSpellInfo(uint32(spell.SpellId)))
                        for (SpellEffectInfo const& effect : info->GetEffects())
                            if (journey::IsEnchantEffect(effect.Effect) && effect.MiscValue > 0)
                                _enchantmentEras[uint32(effect.MiscValue)] =
                                    journey::CraftEra(int(proto.RequiredSkillRank));
    }

    for (auto const& [trainerId, trainer] : sObjectMgr->GetTrainers())
        for (Trainer::Spell const& spell : trainer.GetSpells())
            if (spell.ReqSkillLine && spell.ReqSkillRank > 300)
                if (SpellInfo const* info = sSpellMgr->GetSpellInfo(spell.SpellId))
                    for (SpellEffectInfo const& effect : info->GetEffects())
                        if (journey::IsEnchantEffect(effect.Effect) && effect.MiscValue > 0)
                        {
                            journey::ItemEra const era = journey::CraftEra(int(spell.ReqSkillRank));
                            auto [known, inserted] = _enchantmentEras.try_emplace(uint32(effect.MiscValue), era);
                            if (!inserted && era > known->second)
                                known->second = era;
                        }
}

JourneyEnchantmentReport JourneyContent::BuildEnchantments()
{
    JourneyEnchantmentReport report;
    if (!IsBuilt() || !_settings.enchantments)
        return report;

    // What the top rewards of Outland and of Northrend kept of their statistics.
    std::array<double, 4> sum{};
    std::array<uint32, 4> count{};
    for (auto const& [entry, proto] : *sObjectMgr->GetItemTemplateStore())
    {
        AppliedItemScaling const* applied = sItemBudgetScaler->FindAppliedScaling(entry);
        if (!applied)
            continue;
        journey::ItemPlan const plan = _rules.PlanItem(int(proto.Quality), int(applied->authoredRequiredLevel),
            int(applied->authoredItemLevel));
        if (plan.endgame && plan.era < sum.size())
        {
            sum[plan.era] += applied->statMultiplier;
            ++count[plan.era];
        }
    }
    std::array<double, 4> endgameRatio{ 1.0, 1.0, 1.0, 1.0 };
    for (size_t era = 0; era < sum.size(); ++era)
        if (count[era])
            endgameRatio[era] = sum[era] / double(count[era]);
    report.outlandEndgameRatio = float(endgameRatio[journey::ITEM_TBC]);
    report.northrendEndgameRatio = float(endgameRatio[journey::ITEM_WOTLK]);

    // Shared by things of different levels, the gentler factor wins.
    std::unordered_map<uint32, double> factors;
    auto keepMost = [&factors](uint32 enchantmentId, double factor)
    {
        auto [known, inserted] = factors.try_emplace(enchantmentId, factor);
        if (!inserted && factor > known->second)
            known->second = factor;
    };
    for (auto const& [enchantmentId, era] : _enchantmentEras)
        keepMost(enchantmentId, endgameRatio[era]);
    for (auto const& [bonus, items] : _socketBonusItems)
        for (uint32 item : items)
            if (AppliedItemScaling const* applied = sItemBudgetScaler->FindAppliedScaling(item))
                keepMost(bonus, applied->statMultiplier);

    for (auto const& [enchantmentId, factor] : factors)
    {
        if (factor >= 0.999)
            continue;
        auto* entry = const_cast<SpellItemEnchantmentEntry*>(sSpellItemEnchantmentStore.LookupEntry(enchantmentId));
        if (!entry)
            continue;

        std::vector<std::pair<uint32, uint32>> changes;
        for (uint32 i = 0; i < MAX_SPELL_ITEM_ENCHANTMENT_EFFECTS; ++i)
            if (entry->amount[i] && (entry->type[i] == ITEM_ENCHANTMENT_TYPE_DAMAGE ||
                entry->type[i] == ITEM_ENCHANTMENT_TYPE_RESISTANCE || entry->type[i] == ITEM_ENCHANTMENT_TYPE_STAT))
            {
                uint32 const before = entry->amount[i];
                entry->amount[i] = std::max<uint32>(1, uint32(std::lround(before * factor)));
                changes.emplace_back(before, entry->amount[i]);
            }
        if (changes.empty())
            continue;
        ++report.enchantments;

        // The text the client shows for it, with its numbers as they are now: for the addon's tooltips.
        uint32 const locale = sWorld->GetDefaultDbcLocale();
        char const* description = locale < 16 ? entry->description[locale] : nullptr;
        if (!description || !*description)
            description = entry->description[0];
        if (description && *description)
        {
            std::string const before = description;
            std::string const after = journey::addon::Retext(before, changes);
            if (after != before && before.size() < 100 && after.size() < 100 &&
                journey::addon::CanCarry(before) && journey::addon::CanCarry(after))
                _enchantmentTexts.emplace_back(before, after);
        }
    }

    // The level an enchantment of Outland or Northrend asks for follows the journey like the world it comes from.
    // What an item asks for itself stays the item's.
    int const top = std::min<int>(_maxPlayerLevel, 60);
    for (SpellItemEnchantmentEntry const* constEntry : sSpellItemEnchantmentStore)
    {
        if (!constEntry)
            continue;
        auto const known = _enchantmentEras.find(constEntry->ID);
        bool const northrend = known != _enchantmentEras.end() ? known->second == journey::ITEM_WOTLK :
            constEntry->requiredLevel > 70;
        int const now = _rules.EnchantmentLevel(int(constEntry->requiredLevel), northrend, top);
        if (now < int(constEntry->requiredLevel))
        {
            const_cast<SpellItemEnchantmentEntry*>(constEntry)->requiredLevel = uint32(now);
            ++report.enchantmentLevels;
        }
    }
    return report;
}

std::vector<std::string> JourneyContent::GetAddonMessages() const
{
    std::vector<std::string> messages;
    if (!IsBuilt() || !_settings.addon)
        return messages;

    messages.emplace_back("B");

    std::vector<std::string> zones;
    for (journey::ZoneData const& zone : journey::Zones)
    {
        int const entry = _rules.ZoneEntry(zone.zone, zone.part, zone.originalEntry);
        zones.push_back(std::string(zone.key) + "=" + std::to_string(entry) + "-" +
            std::to_string(std::max(entry, _rules.GetShape().endgame)) + ";");
    }
    // The world map also names the levels of a whole continent when zoomed out: Outland and Northrend get the lowest
    // entry of their zones up to the endgame.
    static constexpr std::array<char const*, 3> Continents = { nullptr, "Expansion01", "Northrend" };
    for (uint8 part = 1; part <= 2; ++part)
    {
        int lowest = _rules.GetShape().endgame;
        for (journey::ZoneData const& zone : journey::Zones)
            if (zone.part == part)
                lowest = std::min(lowest, _rules.ZoneEntry(zone.zone, zone.part, zone.originalEntry));
        zones.push_back(std::string(Continents[part]) + "=" + std::to_string(lowest) + "-" +
            std::to_string(_rules.GetShape().endgame) + ";");
    }
    for (std::string& message : journey::addon::Chunk('Z', zones))
        messages.push_back(std::move(message));

    std::vector<std::string> dungeons;
    for (DungeonLevels const& dungeon : _dungeonLevels)
        dungeons.push_back(std::to_string(dungeon.id) + "=" + std::to_string(dungeon.minLevel) + "-" +
            std::to_string(dungeon.maxLevel) + "-" + std::to_string(dungeon.recommended) + ";");
    for (std::string& message : journey::addon::Chunk('L', dungeons))
        messages.push_back(std::move(message));

    if (_settings.tooltips)
        for (auto const& [before, after] : _enchantmentTexts)
            messages.push_back("E" + before + "~" + after);

    messages.emplace_back("D");
    return messages;
}

float JourneyContent::GetQuestXpRate(uint32 questId, float defaultRate) const
{
    std::optional<journey::QuestPlan> const plan = GetQuestPlan(questId);
    int const part = plan ? journey::PartOf(plan->era) : 0;
    return defaultRate * _settings.questXpRates[std::clamp(part, 0, 3)];
}

int JourneyContent::GetCreatureShift(Creature const* creature) const
{
    if (!IsBuilt() || !creature || (_zoneShifts.empty() && _mapShifts.empty()))
        return 0;

    Map const* map = creature->FindMap();
    if (!map)
        return 0;

    if (map->Instanceable())
    {
        auto const shift = _mapShifts.find(map->GetId());
        return shift != _mapShifts.end() ? shift->second : 0;
    }

    uint32 const zone = map->GetZoneId(creature->GetPhaseMask(), creature->GetPositionX(), creature->GetPositionY(),
        creature->GetPositionZ());
    auto const shift = _zoneShifts.find(zone);
    return shift != _zoneShifts.end() ? shift->second : 0;
}

std::optional<journey::QuestPlan> JourneyContent::GetQuestPlan(uint32 questId) const
{
    if (!IsBuilt())
        return std::nullopt;

    auto const plan = _quests.find(questId);
    if (plan == _quests.end())
        return std::nullopt;
    return plan->second;
}

std::optional<uint8> JourneyContent::GetInstanceEntry(uint32 mapId, uint8 difficulty) const
{
    auto const entry = _instanceEntries.find(InstanceKey(mapId, difficulty));
    if (entry == _instanceEntries.end())
        return std::nullopt;
    return entry->second;
}

int JourneyContent::OriginalLevel(Creature const* creature) const
{
    // The level a creature stood at before the journey: the same place in its original range.
    CreatureTemplate const* cinfo = creature->GetCreatureTemplate();
    int const now = creature->GetLevel();
    auto const shift = cinfo ? _shifts.find(cinfo->Entry) : _shifts.end();
    if (shift == _shifts.end())
        return now;

    Shift const& s = shift->second;
    double const place = s.newMax > s.newMin ?
        double(std::clamp<int>(now, s.newMin, s.newMax) - s.newMin) / double(s.newMax - s.newMin) : 0.0;
    return int(std::lround(s.originalMin + place * (s.originalMax - s.originalMin)));
}

namespace
{
    // What a blow is worth at a level: the weapon damage and attack power of that level's base stats.
    double HitAt(uint8 level, uint8 unitClass, uint8 expansion)
    {
        CreatureBaseStats const* stats = sObjectMgr->GetCreatureBaseStats(level, unitClass);
        if (!stats)
            return 0.0;
        return stats->BaseDamage[std::min<uint8>(expansion, MAX_EXPANSIONS - 1)] + stats->AttackPower / 14.0 * 2.0;
    }
}

double JourneyContent::GetSpellFactor(Unit const* attacker) const
{
    if (!IsBuilt() || !attacker)
        return 1.0;

    Creature const* creature = attacker->ToCreature();
    if (!creature)
        if (Unit const* owner = attacker->GetCharmerOrOwner())
            creature = owner->ToCreature();
    if (!creature || creature->IsControlledByPlayer())
        return 1.0;

    CreatureTemplate const* cinfo = creature->GetCreatureTemplate();
    auto const shift = cinfo ? _shifts.find(cinfo->Entry) : _shifts.end();
    if (shift == _shifts.end())
        return 1.0;

    uint8 const now = std::clamp<uint8>(creature->GetLevel(), 1, 255);
    uint8 const was = uint8(std::clamp(OriginalLevel(creature), 1, 255));
    return journey::SpellLevelFactor(HitAt(was, cinfo->unit_class, shift->second.originalExpansion),
        HitAt(now, cinfo->unit_class, shift->second.newExpansion));
}

uint8 JourneyContent::GetWindowLevel(Player const* viewer, Creature const* creature, uint8 ownLevel) const
{
    if (!IsBuilt() || !_settings.window || !viewer || !creature)
        return 0;

    auto const zone = _zones.find(creature->GetZoneId());
    if (zone == _zones.end())
        return 0;

    CreatureTemplate const* cinfo = creature->GetCreatureTemplate();
    uint32 const rank = cinfo ? cinfo->rank : 0;
    if (rank == CREATURE_ELITE_WORLDBOSS)
        return 0;

    bool const eliteOrRare = rank == CREATURE_ELITE_ELITE || rank == CREATURE_ELITE_RARE ||
        rank == CREATURE_ELITE_RAREELITE;
    int const offset = journey::WindowOffset(OriginalLevel(creature), zone->second.originalEntry, zone->second.part,
        eliteOrRare, _settings.windowBelow, _settings.windowAbove);
    int const placed = std::clamp<int>(int(viewer->GetLevel()) + offset, 1, 255);
    return uint8(std::max<int>(ownLevel, placed));
}

void JourneyContent::AddScalingScope(std::vector<uint32>& worldMaps,
    std::vector<std::pair<uint32, LocalLevelScaling::LevelBand>>& dungeonBands) const
{
    if (!IsBuilt())
        return;

    worldMaps.push_back(530);
    worldMaps.push_back(571);
    for (journey::InstanceData const& instance : journey::Instances)
    {
        if (instance.kind != journey::INSTANCE_DUNGEON || instance.difficulty != 0)
            continue;

        std::optional<uint8> const entry = GetInstanceEntry(instance.map, instance.difficulty);
        uint8 const low = std::clamp<uint8>(entry.value_or(instance.originalEntry), 1, _maxPlayerLevel);
        dungeonBands.push_back({ instance.map, LocalLevelScaling::LevelBand{ low, _maxPlayerLevel } });
    }
}
