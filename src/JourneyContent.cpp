/*
 * CoA Universal Content Scaling
 * JourneyContent: Implementation of the journey layout on the loaded world.
 *
 * What it does follows mod-world-journey by aspollon (JourneyDb.cpp and JourneyServer.cpp, faed6d6, GNU AGPL v3),
 * on the loaded templates instead of the world database.
 */

#include "JourneyContent.h"
#include "Creature.h"
#include "CreatureData.h"
#include "DBCStores.h"
#include "ItemTemplate.h"
#include "JourneyData.h"
#include "LFGMgr.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuestDef.h"
#include <algorithm>
#include <map>

namespace
{
    uint64 InstanceKey(uint32 mapId, uint8 difficulty)
    {
        return (uint64(mapId) << 8) | difficulty;
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

    BuildPlaces(rules);
    BuildCreatures(rules, settings, keepMap, report);
    BuildQuests(rules, report);
    BuildDungeonFinder(report);

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

        _shifts[entry] = { uint8(originalMin), uint8(originalMax), uint8(plan.minLevel), uint8(plan.maxLevel) };
        ++report.creaturesMoved;
    }
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

        int rewardLevel = 0;
        auto considerReward = [&rewardLevel](uint32 itemId)
        {
            if (!itemId)
                return;
            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
            bool const equipment = proto && (proto->Class == ITEM_CLASS_WEAPON || proto->Class == ITEM_CLASS_ARMOR);
            if (equipment && proto->RequiredLevel > 1)
                rewardLevel = std::max(rewardLevel, int(proto->RequiredLevel));
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
        ++report.dungeonFinderEntries;
    }
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
