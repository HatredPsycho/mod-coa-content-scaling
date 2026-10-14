/*
 * CoA Universal Content Scaling
 * CoAContentScaling: Implementation of master controller, scripts and hooks.
 */

#include "CoAContentScaling.h"
#include "AdaptiveEncounterAPI.h"
#include "AllMapScript.h"
#include "CoAContentScalingConfig.h"
#include "CombatBudgetProfile.h"
#include "Config.h"
#include "Containers.h"
#include "ContentEraResolver.h"
#include "Creature.h"
#include "CreatureData.h"
#include "DBCEnums.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "Field.h"
#include "GameTime.h"
#include "Group.h"
#include "InstanceProfile.h"
#include "InstanceScaleContext.h"
#include "ItemBudgetScaler.h"
#include "Chat.h"
#include "JourneyAddon.h"
#include "JourneyBotPlan.h"
#include "JourneyContent.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "LFGMgr.h"
#include "ContentScalingHooks.h"
#include "Log.h"
#include "LootMgr.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ProgressionRewardResolver.h"
#include "QueryResult.h"
#include "QuestDef.h"
#include "ScaledAuraFeed.h"
#include "ScriptMgr.h"
#include "SoloAssistPolicy.h"
#include "SpellAuraDefines.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "World.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <map>

void AddCoAContentScalingCommands();

CoAContentScaling* CoAContentScaling::Instance()
{
    static CoAContentScaling instance;
    return &instance;
}

void CoAContentScaling::LoadConfig()
{
    // False on this fork, where upstream has true: a realm that gets the worldserver without the
    // configuration file beside it would otherwise start scaling because the file is missing. Nothing
    // here runs unless mod-coa-content-scaling.conf says so.
    _enabled = sConfigMgr->GetOption<bool>(CoAContentScalingConfigKeys::Enable, false);
    _groupScalingEnabled = sConfigMgr->GetOption<bool>(CoAContentScalingConfigKeys::GroupScalingEnable, true);
    _lockOnEncounterStart = sConfigMgr->GetOption<bool>(CoAContentScalingConfigKeys::GroupScalingLockOnEncounterStart, true);
    _allowSoloRaids = sConfigMgr->GetOption<bool>(CoAContentScalingConfigKeys::GroupScalingAllowSoloRaids, true);
    _adaptiveMechanicsEnabled = sConfigMgr->GetOption<bool>(CoAContentScalingConfigKeys::AdaptiveMechanicsEnable, true);
    _scaleLootCount = sConfigMgr->GetOption<bool>(CoAContentScalingConfigKeys::RewardsScaleLootCount, true);
    _debug = sConfigMgr->GetOption<bool>(CoAContentScalingConfigKeys::Debug, false);

    std::string const rawProgressionMode = sConfigMgr->GetOption<std::string>(CoAContentScalingConfigKeys::ProgressionMode, "Auto");
    _progressionMode = CoAContentScalingConfig::ParseProgressionMode(rawProgressionMode);

    _customClassicEnd = static_cast<uint8>(sConfigMgr->GetOption<uint32>(CoAContentScalingConfigKeys::ProgressionClassicEnd, 0));
    _customTbcEnd = static_cast<uint8>(sConfigMgr->GetOption<uint32>(CoAContentScalingConfigKeys::ProgressionTbcEnd, 0));

    _tbcEnabled = sConfigMgr->GetOption<bool>(CoAContentScalingConfigKeys::TbcEnable, false);
    _journeyShape = LoadJourneyShape();
    _journeySettings.journeyBaseStats =
        sConfigMgr->GetOption<std::string>(CoAContentScalingConfigKeys::JourneyCreatureBaseStats, "journey") != "original";
    _journeySettings.window = sConfigMgr->GetOption<bool>(CoAContentScalingConfigKeys::JourneyWindowEnable, true);
    _journeySettings.windowBelow =
        std::clamp(sConfigMgr->GetOption<int32>(CoAContentScalingConfigKeys::JourneyWindowBelow, 2), 0, 10);
    _journeySettings.windowAbove =
        std::clamp(sConfigMgr->GetOption<int32>(CoAContentScalingConfigKeys::JourneyWindowAbove, 2), 0, 10);
    _journeySettings.trainers = sConfigMgr->GetOption<bool>(CoAContentScalingConfigKeys::JourneyTrainersEnable, true);
    _journeySettings.battlegrounds =
        sConfigMgr->GetOption<bool>(CoAContentScalingConfigKeys::JourneyBattlegroundsEnable, true);
    _journeySettings.enchantments =
        sConfigMgr->GetOption<bool>(CoAContentScalingConfigKeys::JourneyEnchantmentsEnable, true);
    _journeySettings.addon = sConfigMgr->GetOption<bool>(CoAContentScalingConfigKeys::JourneyAddonEnable, true);
    _journeySettings.tooltips = sConfigMgr->GetOption<bool>(CoAContentScalingConfigKeys::JourneyTooltipsEnable, true);
    {
        float const general = sConfigMgr->GetOption<float>(CoAContentScalingConfigKeys::JourneyQuestXpRate, 1.0f);
        static constexpr std::array<char const*, 4> Parts = { "OldWorld", "Outland", "Northrend", "Endgame" };
        for (size_t part = 0; part < Parts.size(); ++part)
        {
            float const own = sConfigMgr->GetOption<float>(
                std::string(CoAContentScalingConfigKeys::JourneyQuestXpRate) + "." + Parts[part], -1.0f, false);
            float const rate = own >= 0.0f ? own : general;
            _journeySettings.questXpRates[part] = std::isfinite(rate) ? std::clamp(rate, 0.0f, 10.0f) : 1.0f;
        }
    }
    _wotlkEnabled = sConfigMgr->GetOption<bool>(CoAContentScalingConfigKeys::WotlkEnable, false);

    std::string const soloMode = sConfigMgr->GetOption<std::string>(CoAContentScalingConfigKeys::SoloAssistMode, "0");
    sSoloAssistPolicy->SetMode(CoAContentScalingConfig::ParseSoloAssistMode(soloMode));

    _authenticMaps = CoAContentScalingConfig::ParseMapList(
        sConfigMgr->GetOption<std::string>(CoAContentScalingConfigKeys::AuthenticMaps, ""));
    _authenticHeroicMaps = CoAContentScalingConfig::ParseMapList(
        sConfigMgr->GetOption<std::string>(CoAContentScalingConfigKeys::AuthenticHeroicMaps, ""));
    _authenticHeroicMinLevel = static_cast<uint8>(std::min<uint32>(
        sConfigMgr->GetOption<uint32>(CoAContentScalingConfigKeys::AuthenticHeroicMinLevel, 60), 255));

    LoadTuning();
}

namespace
{
    std::map<uint32, int> ReadJourneyEntries(std::string const& prefix)
    {
        std::map<uint32, int> entries;
        for (std::string const& key : sConfigMgr->GetKeysByString(prefix))
        {
            uint32 const id = static_cast<uint32>(std::strtoul(key.c_str() + prefix.size(), nullptr, 10));
            int32 const level = sConfigMgr->GetOption<int32>(key, 0);
            if (id && level > 0)
                entries[id] = level;
        }
        return entries;
    }
}

journey::Shape CoAContentScaling::LoadJourneyShape()
{
    using namespace CoAContentScalingConfigKeys;
    auto range = [](char const* key, journey::Range fallback)
    {
        return CoAContentScalingConfig::ParseLevelRange(key, sConfigMgr->GetOption<std::string>(key, "", false), fallback);
    };

    journey::Shape shape;
    shape.classic = range(JourneyClassicRange, shape.classic);
    shape.outlandRange = range(JourneyOutlandRange, shape.outlandRange);
    shape.northrendRange = range(JourneyNorthrendRange, shape.northrendRange);
    shape.endgame = sConfigMgr->GetOption<int32>(JourneyEndgameLevel, shape.endgame);
    shape.bossLevels = sConfigMgr->GetOption<int32>(JourneyEndgameBossLevels, shape.bossLevels);
    shape.outlandItems = range(JourneyOutlandItemLevels, shape.outlandItems);
    shape.northrendItems = range(JourneyNorthrendItemLevels, shape.northrendItems);
    shape.zoneEntries = ReadJourneyEntries(JourneyZonePrefix);
    shape.instanceEntries = ReadJourneyEntries(JourneyInstancePrefix);
    return shape.Normalized();
}

bool CoAContentScaling::IsAuthenticMap(uint32 mapId, uint8 difficulty) const
{
    return _authenticMaps.count(mapId) ||
        (difficulty != REGULAR_DIFFICULTY && _authenticHeroicMaps.count(mapId));
}

bool CoAContentScaling::IsAuthenticMap(Map const* map) const
{
    return map && IsAuthenticMap(map->GetId(), map->GetDifficulty());
}

uint8 CoAContentScaling::GetAuthenticMinLevel(uint32 mapId, uint8 difficulty, uint8 authoredMinLevel) const
{
    if (difficulty == REGULAR_DIFFICULTY || !_authenticHeroicMaps.count(mapId))
        return authoredMinLevel;

    return std::max(authoredMinLevel, _authenticHeroicMinLevel);
}

void CoAContentScaling::LoadTuning()
{
    float const damage = sConfigMgr->GetOption<float>(CoAContentScalingConfigKeys::DamageMultiplier, 1.0f);
    _damageMultiplier.store(std::isfinite(damage) ? std::clamp(damage, 0.25f, 2.0f) : 1.0f, std::memory_order_relaxed);

    _worldLeechEnabled.store(sConfigMgr->GetOption<bool>(CoAContentScalingConfigKeys::WorldLeechEnable, false),
                             std::memory_order_relaxed);
    float const leech = sConfigMgr->GetOption<float>(CoAContentScalingConfigKeys::WorldLeechPercent, 5.0f);
    _worldLeechPercent.store(std::isfinite(leech) ? std::clamp(leech, 0.0f, 100.0f) : 5.0f, std::memory_order_relaxed);
}

void CoAContentScaling::FinalizeAndInitialize()
{
    if (_layoutInitialized)
        return;

    // Switched off means switched off: no layout, no database read, no item templates rewritten. The
    // table this reads belongs to another module, and upstream read it before anything asked whether
    // this one was even enabled.
    if (!_enabled)
        return;

    // 1. Compute immutable progression layout
    _layoutInitialized = true;
    InitializeLayout();

    // The realm's own creature multipliers lift the open world, which content scaling leaves at the base statistics of
    // the moved levels; in instances content scaling sets the budget, and CoA's instance multipliers would double it.
    if (sConfigMgr->GetOption<bool>("CoA.CreatureScaling.Enable", false, false))
    {
        bool instanceMultipliers =
            !sConfigMgr->GetOption<std::string>("CoA.CreatureScaling.MapHealth", "", false).empty() ||
            !sConfigMgr->GetOption<std::string>("CoA.CreatureScaling.MapDamage", "", false).empty();
        for (char const* key : { "CoA.CreatureScaling.Dungeon.Health", "CoA.CreatureScaling.Dungeon.Damage",
                                 "CoA.CreatureScaling.Raid.Health", "CoA.CreatureScaling.Raid.Damage" })
            if (std::fabs(sConfigMgr->GetOption<float>(key, 1.0f, false) - 1.0f) > 0.001f)
                instanceMultipliers = true;
        if (instanceMultipliers)
            LOG_WARN("server.loading", "CoAContentScaling: CoA.CreatureScaling applies dungeon, raid or map "
                     "multipliers; content scaling already sets the instance budget, so set them to 1.0 and "
                     "MapHealth/MapDamage empty.");
    }

    // 2. Load calibrated boss flex profiles
    sInstanceScalingMgr->LoadCalibratedBossFlex();

    // Item templates are not scaled here. This runs from OnLoadCustomDatabaseTable, which the world
    // reaches about a hundred and fifty steps before LoadItemTemplates - the store is still empty,
    // and the pass walked zero templates on every start since the module was written. ScaleItems()
    // does it from OnBeforeWorldInitialized instead, once everything is loaded.

    LOG_INFO("server.loading", "CoAContentScaling: Finalized lifecycle and built immutable ProgressionLayout (Cap {})",
             _layout.maxLevel);
}

void CoAContentScaling::LoadContentEras()
{
    // The client data is read after OnLoadCustomDatabaseTable, so the eras are read here, once the stores
    // are filled and before the item templates are rewritten by them.
    if (!_enabled || sContentEraResolver->IsFinalized())
        return;

    sInstanceProfileRegistry->Initialize();
    sContentEraResolver->LoadClientData();
    sContentEraResolver->Finalize();

    if (!_layout.journey)
        return;

    // The authored levels are read here, before the items are rewritten: a quest stands at the level its
    // equipment reward asks for.
    JourneyBuildReport const report = sJourneyContent->Build(_layout.journeyRules, _journeySettings, _layout.maxLevel,
        [this](uint32 mapId, uint8 difficulty) { return IsAuthenticMap(mapId, difficulty); });
    LOG_INFO("server.loading", "CoAContentScaling: the journey moved {} of {} creature templates ({} already "
             "spawned made again), {} quests, {} Dungeon Finder entries, {} trainer spells and {} battlegrounds",
             report.creaturesMoved, report.creatureTemplates, report.creaturesRelevelled, report.questsMoved,
             report.dungeonFinderEntries, report.trainerSpells, report.battlegrounds);
}

void CoAContentScaling::ScaleEnchantments()
{
    if (!_enabled || !_layout.journey)
        return;

    JourneyEnchantmentReport const report = sJourneyContent->BuildEnchantments();
    LOG_INFO("server.loading", "CoAContentScaling: the top rewards of Outland keep {:.0f}% of their statistics, "
             "those of Northrend {:.0f}%; {} enchantments and gems cut, {} ask for a level of the journey",
             report.outlandEndgameRatio * 100.0f, report.northrendEndgameRatio * 100.0f, report.enchantments,
             report.enchantmentLevels);
}

ContentEra CoAContentScaling::EraOfJourney(journey::Era era)
{
    switch (era)
    {
        case journey::ERA_OUTLAND:
        case journey::ERA_TOP_OUTLAND:
            return ContentEra::TBC;
        case journey::ERA_NORTHREND:
        case journey::ERA_TOP_NORTHREND:
            return ContentEra::WotLK;
        default:
            return ContentEra::Classic;
    }
}

namespace
{
    // What a loot table is dropped as: the items themselves, and the Heroic and Mythic variants
    // coa_dungeon_loot_variant swaps them for on a dungeon's higher difficulties.
    enum LootSource : uint8
    {
        LOOT_SOURCE_AUTHENTIC         = 1,
        LOOT_SOURCE_SCALED            = 2,
        LOOT_SOURCE_VARIANT_AUTHENTIC = 4,
        LOOT_SOURCE_VARIANT_SCALED    = 8,

        LOOT_SOURCE_ITEM_MASK    = LOOT_SOURCE_AUTHENTIC | LOOT_SOURCE_SCALED,
        LOOT_SOURCE_VARIANT_MASK = LOOT_SOURCE_VARIANT_AUTHENTIC | LOOT_SOURCE_VARIANT_SCALED
    };

    using LootSources = std::unordered_map<uint32, uint8>;

    struct ScriptSummonedCreature
    {
        uint32 mapId;
        uint32 entry;
    };

    // Creatures the instance scripts summon instead of spawning, so no spawn ties their loot to a map.
    constexpr std::array<ScriptSummonedCreature, 45> ScriptSummonedCreatures
    {{
        { 409, 11502 }, { 409, 11663 }, { 409, 11664 }, { 409, 12018 }, { 409, 12119 },
        { 469, 11583 },
        { 309, 14515 }, { 309, 15082 }, { 309, 15083 }, { 309, 15084 }, { 309, 15085 }, { 309, 15114 },
        { 531, 15246 }, { 531, 15517 },
        { 229, 10258 }, { 229, 10264 }, { 229, 10268 }, { 229, 10339 }, { 229, 10584 }, { 229, 10601 },
        { 229, 10602 }, { 229, 10683 }, { 229, 10742 },
        { 230, 8925 }, { 230, 8926 }, { 230, 8927 }, { 230, 8928 }, { 230, 8932 }, { 230, 8933 },
        { 230, 9027 }, { 230, 9028 }, { 230, 9029 }, { 230, 9030 }, { 230, 9031 }, { 230, 9032 },
        { 230, 9476 }, { 230, 10076 }, { 230, 16059 },
        { 289, 10506 }, { 289, 11284 }, { 289, 11598 },
        { 329, 10394 }, { 329, 10439 }, { 329, 16101 }, { 329, 16102 }
    }};

    struct LootRow
    {
        uint32 entry;
        uint32 item;
        uint32 reference;
    };

    std::vector<LootRow> LoadLootRows(std::string_view table)
    {
        std::vector<LootRow> rows;
        if (QueryResult result = WorldDatabase.Query("SELECT Entry, Item, Reference FROM {}", table))
        {
            rows.reserve(result->GetRowCount());
            do
            {
                Field* fields = result->Fetch();
                rows.push_back({ fields[0].Get<uint32>(), fields[1].Get<uint32>(), fields[2].Get<uint32>() });
            } while (result->NextRow());
        }

        return rows;
    }

    template <typename SourceOf>
    void MarkCreatureLoot(LootSources& sources, uint32 entry, SourceOf sourceOf)
    {
        CreatureTemplate const* cinfo = sObjectMgr->GetCreatureTemplate(entry);
        if (!cinfo)
            return;

        if (cinfo->lootid)
            sources[cinfo->lootid] |= sourceOf(REGULAR_DIFFICULTY, true);

        for (uint8 index = 0; index < MAX_DIFFICULTY - 1; ++index)
            if (uint32 const difficultyEntry = cinfo->DifficultyEntry[index])
                if (CreatureTemplate const* variant = sObjectMgr->GetCreatureTemplate(difficultyEntry))
                    if (variant->lootid)
                        sources[variant->lootid] |= sourceOf(Difficulty(index + 1), false);
    }

    bool SpreadLoot(std::vector<LootRow> const& rows, LootSources const& containers, LootSources& references,
                    LootSources& items)
    {
        bool changed = false;
        for (LootRow const& row : rows)
        {
            auto const container = containers.find(row.entry);
            if (container == containers.end())
                continue;

            uint8 const source = container->second;
            uint8& target = row.reference ? references[row.reference] : items[row.item];
            if ((target | source) != target)
            {
                target |= source;
                changed = true;
            }
        }

        return changed;
    }
}

std::unordered_set<uint32> CoAContentScaling::CollectAuthenticItems() const
{
    // An item keeps its authored template only when every spawned source of it is on an authentic map:
    // a shared table that also drops in scaled content keeps following the scaled content. A loot table
    // of a five-player dungeon also stands for the variants its items are swapped for on Heroic and
    // Mythic, which follow those difficulties instead.
    auto sourceOn = [this](uint32 mapId)
    {
        MapEntry const* entry = sMapStore.LookupEntry(mapId);
        bool const hasVariants = entry && entry->IsNonRaidDungeon();
        return [this, mapId, hasVariants](Difficulty difficulty, bool swapsToVariants) -> uint8
        {
            uint8 source = IsAuthenticMap(mapId, difficulty) ? LOOT_SOURCE_AUTHENTIC : LOOT_SOURCE_SCALED;
            if (swapsToVariants && hasVariants)
                source |= IsAuthenticMap(mapId, DUNGEON_DIFFICULTY_HEROIC) ?
                    LOOT_SOURCE_VARIANT_AUTHENTIC : LOOT_SOURCE_VARIANT_SCALED;
            return source;
        };
    };

    LootSources creatureLoot;
    for (auto const& [spawnId, data] : sObjectMgr->GetAllCreatureData())
        for (uint32 entry : { data.id, data.id2, data.id3 })
            if (entry)
                MarkCreatureLoot(creatureLoot, entry, sourceOn(data.mapid));

    for (ScriptSummonedCreature const& summoned : ScriptSummonedCreatures)
        MarkCreatureLoot(creatureLoot, summoned.entry, sourceOn(summoned.mapId));

    for (uint32 mapId : _authenticMaps)
        for (uint8 difficulty = 0; difficulty < MAX_DIFFICULTY; ++difficulty)
            if (DungeonEncounterList const* encounters = sObjectMgr->GetDungeonEncounterList(mapId, Difficulty(difficulty)))
                for (DungeonEncounter const* encounter : *encounters)
                    if (encounter->creditType == ENCOUNTER_CREDIT_KILL_CREATURE)
                        MarkCreatureLoot(creatureLoot, encounter->creditEntry, sourceOn(mapId));

    LootSources chestLoot;
    for (auto const& [spawnId, data] : sObjectMgr->GetAllGOData())
        if (GameObjectTemplate const* goInfo = sObjectMgr->GetGameObjectTemplate(data.id))
            if (uint32 const lootId = goInfo->GetLootId())
                chestLoot[lootId] |= sourceOn(data.mapid)(REGULAR_DIFFICULTY, true);

    LootSources references;
    LootSources items;
    SpreadLoot(LoadLootRows("creature_loot_template"), creatureLoot, references, items);
    SpreadLoot(LoadLootRows("gameobject_loot_template"), chestLoot, references, items);

    std::vector<LootRow> const referenceRows = LoadLootRows("reference_loot_template");
    while (SpreadLoot(referenceRows, LootSources(references), references, items))
        ;

    std::unordered_set<uint32> authentic;
    for (auto const& [item, source] : items)
        if ((source & LOOT_SOURCE_ITEM_MASK) == LOOT_SOURCE_AUTHENTIC)
            authentic.insert(item);

    if (QueryResult result = WorldDatabase.Query("SELECT base_item, heroic_item, mythic_item FROM coa_dungeon_loot_variant"))
    {
        do
        {
            Field* fields = result->Fetch();
            auto const base = items.find(fields[0].Get<uint32>());
            if (base == items.end() || (base->second & LOOT_SOURCE_VARIANT_MASK) != LOOT_SOURCE_VARIANT_AUTHENTIC)
                continue;

            for (uint8 tier = 1; tier <= 2; ++tier)
                if (uint32 const variant = fields[tier].Get<uint32>())
                    authentic.insert(variant);
        } while (result->NextRow());
    }

    return authentic;
}

void CoAContentScaling::ScaleItems()
{
    if (!_enabled || !sConfigMgr->GetOption<bool>(CoAContentScalingConfigKeys::ScaleItems, true))
        return;

    if (!_authenticMaps.empty() || !_authenticHeroicMaps.empty())
    {
        std::unordered_set<uint32> items = CollectAuthenticItems();
        LOG_INFO("server.loading",
                 "CoAContentScaling: {} items of {} authentic maps and {} authentic Heroic/Mythic dungeons keep "
                 "their authored levels", items.size(), _authenticMaps.size(), _authenticHeroicMaps.size());
        sItemBudgetScaler->PreserveItems(std::move(items));
    }

    sItemBudgetScaler->ScaleAllItems(_layout);
    ScaleAccessRequirements();
}

void CoAContentScaling::ScaleAccessRequirements()
{
    if (!sItemBudgetScaler->AreItemsScaled() || _layout.IsStockIdentity())
        return;

    uint32 scaled = 0;
    for (MapEntry const* map : sMapStore)
    {
        if (!map || !map->IsDungeon())
            continue;

        for (uint8 difficulty = 0; difficulty < MAX_DIFFICULTY; ++difficulty)
        {
            if (IsAuthenticMap(map->MapID, difficulty))
                continue;

            auto* access = const_cast<DungeonProgressionRequirements*>(
                sObjectMgr->GetAccessRequirement(map->MapID, Difficulty(difficulty)));
            if (!access || !access->reqItemLevel)
                continue;

            uint32 const authored = access->reqItemLevel;
            access->reqItemLevel = static_cast<uint16>(sItemBudgetScaler->ScaleRequiredAverageItemLevel(
                authored, ResolveMapEra(map->MapID, difficulty), _layout));

            if (access->reqItemLevel != authored)
                ++scaled;

            LOG_DEBUG("module.coa_content_scaling", "CoAContentScaling: map {} difficulty {} asks for item level {} instead of {}",
                      map->MapID, uint32(difficulty), access->reqItemLevel, authored);
        }
    }

    LOG_INFO("server.loading", "CoAContentScaling: {} instance item level requirements follow the scaled items", scaled);
}

void CoAContentScaling::InitializeLayout()
{
    uint8 const maxPlayerLevel = static_cast<uint8>(sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL));
    bool const tbcActive = _tbcEnabled;
    bool const wotlkActive = _wotlkEnabled;

    if (_progressionMode == "Journey")
    {
        _layout = ProgressionLayout::CreateJourney(maxPlayerLevel, tbcActive, wotlkActive, _journeyShape);
        if (maxPlayerLevel > _layout.journeyRules.GetShape().endgame)
            LOG_WARN("server.loading", "CoAContentScaling: MaxPlayerLevel {} is above the journey's endgame level {}; "
                     "characters past it meet no content of their level.", maxPlayerLevel,
                     _layout.journeyRules.GetShape().endgame);
    }
    else if (_progressionMode == "Custom")
    {
        _layout = ProgressionLayout::Create(
            maxPlayerLevel, tbcActive, wotlkActive, _customClassicEnd, _customTbcEnd);
    }
    else
    {
        _layout = ProgressionLayout::Create(maxPlayerLevel, tbcActive, wotlkActive);
    }

    std::string validationError;
    if (!_layout.Validate(validationError))
    {
        LOG_FATAL("server.loading", "CoAContentScaling: Progression layout invalid: {}", validationError);
        _enabled = false;
        return;
    }

    LOG_INFO("server.loading", "UniversalContentScaling: Active layout [Classic {}-{} | TBC: {} | WotLK: {}] Cap: {}",
             _layout.classic.minLevel, _layout.classic.maxLevel,
             _layout.tbc ? std::to_string(_layout.tbc->minLevel) + "-" + std::to_string(_layout.tbc->maxLevel) : "None",
             _layout.wotlk ? std::to_string(_layout.wotlk->minLevel) + "-" + std::to_string(_layout.wotlk->maxLevel) : "None",
             _layout.maxLevel);
}

uint8 CoAContentScaling::GetEffectiveCreatureLevel(CreatureTemplate const* cinfo, Creature const* creature, uint8 authoredLevel) const
{
    if (!_enabled || !cinfo)
        return authoredLevel;

    // Asked about a template alone - where a creature would be, not one that is - there is no map to
    // read, and map 0 would claim every Outland and Northrend creature for Eastern Kingdoms.
    Map const* map = creature ? creature->FindMap() : nullptr;
    if (IsAuthenticMap(map))
        return authoredLevel;

    // On the journey the templates already carry their levels; only a zone or an instance set by hand moves them.
    if (_layout.journey)
        return static_cast<uint8>(std::clamp<int>(int(authoredLevel) + sJourneyContent->GetCreatureShift(creature), 1, 255));

    uint32 const mapId = map ? map->GetId() : (creature ? 0 : ContentEraResolver::MAP_UNSPECIFIED);
    uint32 const areaId = creature ? creature->GetAreaId() : 0;

    ContentEra const era = _layout.ResolveContentEra(sContentEraResolver->ResolveEraForCreature(
        cinfo->Entry, mapId, areaId, cinfo->expansion, authoredLevel), authoredLevel);

    return _layout.MapAuthoredToEffective(era, authoredLevel);
}

int32 CoAContentScaling::GetEffectiveQuestLevel(Quest const* quest) const
{
    if (!_enabled || !quest)
        return quest ? quest->GetQuestLevel() : 0;

    int32 const authoredLevel = quest->GetQuestLevel();
    if (authoredLevel <= 0)
        return authoredLevel;

    if (_layout.journey)
    {
        std::optional<journey::QuestPlan> const plan = sJourneyContent->GetQuestPlan(quest->GetQuestId());
        if (!plan)
            return authoredLevel;
        if (!_layout.IsEraEnabled(EraOfJourney(plan->era)))
            return 255; // Lock out quest from disabled expansion
        return plan->level;
    }

    ContentEra const era = _layout.ResolveContentEra(sContentEraResolver->ResolveEraForQuest(
        quest->GetQuestId(), quest->GetZoneOrSort(), 0, static_cast<uint8>(authoredLevel)),
        static_cast<uint8>(authoredLevel));

    if (!_layout.IsEraEnabled(era))
        return 255; // Lock out quest from disabled expansion

    return static_cast<int32>(_layout.MapAuthoredToEffective(era, static_cast<uint8>(authoredLevel)));
}

uint32 CoAContentScaling::GetEffectiveQuestMinLevel(Quest const* quest) const
{
    if (!_enabled || !quest)
        return quest ? quest->GetMinLevel() : 0;

    uint32 const authoredMin = quest->GetMinLevel();
    if (authoredMin <= 1)
        return authoredMin;

    if (_layout.journey)
    {
        std::optional<journey::QuestPlan> const plan = sJourneyContent->GetQuestPlan(quest->GetQuestId());
        if (!plan)
            return authoredMin;
        if (!_layout.IsEraEnabled(EraOfJourney(plan->era)))
            return 255; // Lock out quest from disabled expansion
        return static_cast<uint32>(std::max(plan->minLevel, 1));
    }

    ContentEra const era = _layout.ResolveContentEra(sContentEraResolver->ResolveEraForQuest(
        quest->GetQuestId(), quest->GetZoneOrSort(), 0, static_cast<uint8>(authoredMin)),
        static_cast<uint8>(authoredMin));

    if (!_layout.IsEraEnabled(era))
        return 255; // Lock out quest from disabled expansion

    return static_cast<uint32>(_layout.MapAuthoredToEffective(era, static_cast<uint8>(authoredMin)));
}

uint8 CoAContentScaling::GetEffectiveAreaContentLevel(uint32 areaId, uint32 mapId, uint8 authoredLevel) const
{
    if (!_enabled || authoredLevel == 0 || IsAuthenticMap(mapId))
        return authoredLevel;

    ContentEra const era = _layout.ResolveContentEra(
        sContentEraResolver->ResolveEraForArea(areaId, mapId), authoredLevel);

    return _layout.MapAuthoredToEffective(era, authoredLevel);
}

uint8 CoAContentScaling::GetEffectiveAbilityRequiredLevel(uint8 authoredLevel) const
{
    if (!_enabled || authoredLevel <= 1)
        return authoredLevel;

    // An ability carries no zone and no expansion field, so its own level is the only thing that
    // says where its progression put it - the same last resort the quest and item paths fall back on.
    ContentEra const era = _layout.GetEraForAuthoredLevel(authoredLevel);
    if (!_layout.IsEraEnabled(era))
        return authoredLevel;

    // Never further out of reach than it was authored: a layout that leaves an era where it stands
    // must not turn a reachable rank into an unreachable one.
    return std::min(authoredLevel, _layout.MapAuthoredToEffective(era, authoredLevel));
}

CalculatedCombatBudget CoAContentScaling::CalculateCreatureBudget(CreatureTemplate const* cinfo, Creature* creature,
                                                                 CreatureScaleContext* outContext) const
{
    CreatureScaleContext ctx = sCombatBudgetProfile->BuildContext(cinfo, creature,
        creature ? creature->GetLevel() : (cinfo ? cinfo->maxlevel : 1));

    Map* map = creature ? creature->GetMap() : nullptr;

    uint32 calibratedHp = 0;
    if (_groupScalingEnabled && map && map->IsDungeon())
    {
        InstanceScaleContext instCtx = sInstanceScalingMgr->GetOrCreateContext(map);
        ctx.groupHealthScale = instCtx.healthScale;
        ctx.groupDamageScale = instCtx.damageScale;

        // Check calibrated coa_boss_flex profile
        if (map->IsRaid() && sInstanceScalingMgr->HasCalibratedBossHp(cinfo->Entry, map->GetSpawnMode()))
        {
            calibratedHp = sInstanceScalingMgr->GetCalibratedBossHp(
                cinfo->Entry, map->GetSpawnMode(), instCtx.effectivePlayers);
        }
    }

    CalculatedCombatBudget budget = sCombatBudgetProfile->CalculateBudget(cinfo, ctx);
    if (calibratedHp > 0)
        budget.health = calibratedHp;

    if (outContext)
        *outContext = ctx;

    return budget;
}

float CoAContentScaling::GetEffectiveCreatureArmor(CreatureTemplate const* cinfo, Creature const* creature,
                                                   float generatedArmor) const
{
    // The open world takes the armor of the level the creature was moved to; only instances carry a budget.
    Map const* map = creature ? creature->FindMap() : nullptr;
    if (!_enabled || !cinfo || !map || !map->IsDungeon() || IsAuthenticMap(map))
        return generatedArmor;

    CreatureScaleContext const ctx = sCombatBudgetProfile->BuildContext(cinfo, creature,
        creature ? creature->GetLevel() : cinfo->maxlevel);

    return sCombatBudgetProfile->CalculateBudget(cinfo, ctx).armor;
}

void RescaleLootDamageRequirement(Creature* creature, uint32 previousMaxHealth)
{
    if (!creature || !previousMaxHealth || creature->GetMaxHealth() >= previousMaxHealth)
        return;

    uint32 const previousRequirement = creature->GetPlayerDamageReq();
    uint32 const requirement = static_cast<uint32>(
        uint64(previousRequirement) * creature->GetMaxHealth() / previousMaxHealth);
    creature->LowerPlayerDamageReq(previousRequirement - requirement, false);
}

void CoAContentScaling::ApplyCreatureScaling(CreatureTemplate const* cinfo, Creature* creature)
{
    // The open world is not given a budget: a creature there takes the base statistics of the level it
    // was moved to, and the realm's own creature scaling lifts it from there.
    Map const* map = creature ? creature->FindMap() : nullptr;
    if (!_enabled || !cinfo || !map || !map->IsDungeon() || IsAuthenticMap(map))
        return;

    CalculatedCombatBudget const budget = CalculateCreatureBudget(cinfo, creature);

    uint32 const previousMaxHealth = creature->GetMaxHealth();
    float const pct = previousMaxHealth ? creature->GetHealthPct() : 100.0f;
    creature->SetCreateHealth(budget.health);
    creature->SetStatFlatModifier(UNIT_MOD_HEALTH, BASE_VALUE, float(budget.health));
    creature->UpdateMaxHealth();
    creature->SetHealth(std::max<uint32>(1, static_cast<uint32>(std::round(float(creature->GetMaxHealth()) * pct / 100.0f))));
    RescaleLootDamageRequirement(creature, previousMaxHealth);

    if (budget.mana > 0)
    {
        creature->SetCreateMana(budget.mana);
        creature->SetMaxPower(POWER_MANA, budget.mana);
        creature->SetPower(POWER_MANA, budget.mana);
        creature->SetStatFlatModifier(UNIT_MOD_MANA, BASE_VALUE, float(budget.mana));
    }

    creature->SetStatFlatModifier(UNIT_MOD_ARMOR, BASE_VALUE, budget.armor);
    creature->UpdateArmor();

    creature->SetStatFlatModifier(UNIT_MOD_ATTACK_POWER, BASE_VALUE, float(budget.attackPower));
    creature->UpdateAttackPowerAndDamage();

    creature->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, budget.minDamage);
    creature->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, budget.maxDamage);
    creature->SetBaseWeaponDamage(OFF_ATTACK, MINDAMAGE, budget.minDamage);
    creature->SetBaseWeaponDamage(OFF_ATTACK, MAXDAMAGE, budget.maxDamage);
    creature->SetBaseWeaponDamage(RANGED_ATTACK, MINDAMAGE, budget.minDamage);
    creature->SetBaseWeaponDamage(RANGED_ATTACK, MAXDAMAGE, budget.maxDamage);

    creature->UpdateDamagePhysical(BASE_ATTACK);
    creature->UpdateDamagePhysical(OFF_ATTACK);
    creature->UpdateDamagePhysical(RANGED_ATTACK);

    _creatureScaleApplied.fetch_add(1, std::memory_order_relaxed);
    _lastScaledEntry.store(cinfo->Entry, std::memory_order_relaxed);
    _lastScaledHealth.store(creature->GetMaxHealth(), std::memory_order_relaxed);
}

uint32 CoAContentScaling::RescaleDungeonCreatures(Map* map)
{
    if (!_enabled || !_groupScalingEnabled || !map || !map->IsDungeon() || IsAuthenticMap(map))
        return 0;

    // A creature is given its budget once, when it is created - and the map is created by whoever
    // walks in first. Everyone arriving behind them meets a dungeon built for one player, which is
    // the opposite of what group scaling is for. Nobody in combat is touched: an encounter already
    // under way keeps the numbers it was pulled with, and that is what the encounter lock is for.
    uint32 rescaled = 0;
    for (auto const& entry : map->GetCreatureBySpawnIdStore())
    {
        Creature* creature = entry.second;
        if (!creature || !creature->IsInWorld() || !creature->IsAlive() || creature->IsInCombat())
            continue;

        if (creature->IsPet() || creature->IsSummon() || creature->IsCritter())
            continue;

        CreatureTemplate const* cinfo = creature->GetCreatureTemplate();
        if (!cinfo)
            continue;

        ApplyCreatureScaling(cinfo, creature);
        ++rescaled;
    }

    return rescaled;
}

void CoAContentScaling::RecalculateEncounterCombatStats(Creature* boss, EncounterScaleSnapshot const& snapshot,
                                                        EncounterHealthTransferPolicy hpPolicy)
{
    if (!_enabled || !boss)
        return;

    CreatureTemplate const* cinfo = boss->GetCreatureTemplate();
    if (!cinfo)
        return;

    Map* map = boss->GetMap();
    if (!map || !map->IsDungeon() || IsAuthenticMap(map))
        return;

    CreatureScaleContext ctx = sCombatBudgetProfile->BuildContext(cinfo, boss, boss->GetLevel());
    ctx.groupHealthScale = snapshot.healthScale;
    ctx.groupDamageScale = snapshot.damageScale;

    uint32 calibratedHp = 0;
    if (map->IsRaid() && sInstanceScalingMgr->HasCalibratedBossHp(cinfo->Entry, map->GetSpawnMode()))
    {
        calibratedHp = sInstanceScalingMgr->GetCalibratedBossHp(
            cinfo->Entry, map->GetSpawnMode(), snapshot.effectivePlayers);
    }

    CalculatedCombatBudget budget = sCombatBudgetProfile->CalculateBudget(cinfo, ctx);
    if (calibratedHp > 0)
        budget.health = calibratedHp;

    uint32 const oldMaxHp = boss->GetMaxHealth();
    uint32 const oldCurHp = boss->GetHealth();
    float const pct = oldMaxHp ? (float(oldCurHp) * 100.0f / float(oldMaxHp)) : 100.0f;

    boss->SetCreateHealth(budget.health);
    boss->SetStatFlatModifier(UNIT_MOD_HEALTH, BASE_VALUE, float(budget.health));
    boss->UpdateMaxHealth();

    uint32 newCurHp = budget.health;
    if (hpPolicy == EncounterHealthTransferPolicy::FULL_ON_PULL)
    {
        newCurHp = budget.health;
    }
    else if (hpPolicy == EncounterHealthTransferPolicy::PRESERVE_PERCENT)
    {
        newCurHp = std::max<uint32>(1, static_cast<uint32>(std::round(float(boss->GetMaxHealth()) * pct / 100.0f)));
    }
    else if (hpPolicy == EncounterHealthTransferPolicy::PRESERVE_ABSOLUTE)
    {
        newCurHp = std::min<uint32>(oldCurHp, boss->GetMaxHealth());
        if (newCurHp == 0 && oldCurHp > 0)
            newCurHp = 1;
    }
    boss->SetHealth(newCurHp);
    RescaleLootDamageRequirement(boss, oldMaxHp);

    if (budget.mana > 0)
    {
        boss->SetCreateMana(budget.mana);
        boss->SetMaxPower(POWER_MANA, budget.mana);
        boss->SetPower(POWER_MANA, budget.mana);
        boss->SetStatFlatModifier(UNIT_MOD_MANA, BASE_VALUE, float(budget.mana));
    }

    boss->SetStatFlatModifier(UNIT_MOD_ARMOR, BASE_VALUE, budget.armor);
    boss->UpdateArmor();

    boss->SetStatFlatModifier(UNIT_MOD_ATTACK_POWER, BASE_VALUE, float(budget.attackPower));
    boss->UpdateAttackPowerAndDamage();

    boss->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, budget.minDamage);
    boss->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, budget.maxDamage);
    boss->SetBaseWeaponDamage(OFF_ATTACK, MINDAMAGE, budget.minDamage);
    boss->SetBaseWeaponDamage(OFF_ATTACK, MAXDAMAGE, budget.maxDamage);
    boss->SetBaseWeaponDamage(RANGED_ATTACK, MINDAMAGE, budget.minDamage);
    boss->SetBaseWeaponDamage(RANGED_ATTACK, MAXDAMAGE, budget.maxDamage);

    boss->UpdateDamagePhysical(BASE_ATTACK);
    boss->UpdateDamagePhysical(OFF_ATTACK);
    boss->UpdateDamagePhysical(RANGED_ATTACK);
}

namespace
{
    // A dead player on the way back to their own corpse is never kept out. Every check below this
    // is about whether someone may start this content; retrieving a corpse is finishing something
    // they already started, and a level requirement that moved under them while they lay there
    // would strand them as a ghost at the door. The walk up Parent covers a corpse left in an
    // inner instance of the map being entered.
    bool IsRetrievingOwnCorpse(Player const* player, uint32 mapId)
    {
        if (!player || player->IsAlive() || !player->HasCorpse())
            return false;

        uint32 corpseMap = player->GetCorpseLocation().GetMapId();
        while (corpseMap)
        {
            if (corpseMap == mapId)
                return true;

            InstanceTemplate const* corpseInstance = sObjectMgr->GetInstanceTemplate(corpseMap);
            corpseMap = corpseInstance ? corpseInstance->Parent : 0;
        }

        return false;
    }
}

bool CoAContentScaling::CanPlayerEnterMap(Player const* player, uint32 mapId, uint8 difficulty) const
{
    if (!_enabled || !player || player->IsGameMaster())
        return true;

    if (IsRetrievingOwnCorpse(player, mapId))
        return true;

    return _layout.IsEraEnabled(ResolveMapEra(mapId, difficulty));
}

ContentEra CoAContentScaling::ResolveMapEra(uint32 mapId, uint8 difficulty) const
{
    if (auto const era = sInstanceProfileRegistry->GetEraForMap(mapId, difficulty))
        return *era;

    return sContentEraResolver->ResolveEraForMap(mapId);
}

void CoAContentScaling::OnPlayerLogout(Player* player)
{
    if (!player)
        return;

    ScaledAuraFeed::Forget(player);
}

// =============================================================================
// Script Hooks Integration
// =============================================================================

namespace
{
    class coa_content_scaling_world : public WorldScript
    {
    public:
        coa_content_scaling_world() : WorldScript("coa_content_scaling_world") { }

        void OnAfterConfigLoad(bool reload) override
        {
            if (reload && sCoAContentScaling->IsLayoutInitialized())
            {
                sCoAContentScaling->LoadTuning();

                LOG_WARN("module.coa_content_scaling",
                         "UniversalContentScaling: Progression layout, expansion packs and MaxPlayerLevel cannot be changed at runtime! Server restart required.");
                return;
            }

            sCoAContentScaling->LoadConfig();
        }

        // Every store is loaded by the time this fires, and nobody can log in yet. It is also where
        // modules build their own caches from the item templates - the bots sort the gear they hand
        // out by the level it asks for - and this module's scripts are registered ahead of theirs,
        // so the templates are rewritten before any of them reads one. OnStartup came too late:
        // a bot of the cap never received gear from the expansions mapped into its levels.
        void OnBeforeWorldInitialized() override
        {
            sCoAContentScaling->LoadContentEras();
            sCoAContentScaling->ScaleItems();
            sCoAContentScaling->ScaleEnchantments();
        }

        void OnLoadCustomDatabaseTable() override
        {
            sCoAContentScaling->FinalizeAndInitialize();

            bool const enabled = sCoAContentScaling->IsEnabled();
            LocalLevelScaling::ContentScalingActive.store(enabled, std::memory_order_relaxed);

            if (enabled)
            {
                LocalLevelScaling::AuthenticMapOwner.store([](uint32 mapId, uint8 difficulty) -> bool
                {
                    return sCoAContentScaling->IsAuthenticMap(mapId, difficulty);
                }, std::memory_order_relaxed);

                bool const journey = sCoAContentScaling->GetLayout().journey;
                LocalLevelScaling::CreatureWindowOwner.store(journey ?
                    [](Player const* viewer, Creature const* creature, uint8 ownLevel) -> uint8
                    {
                        return sJourneyContent->GetWindowLevel(viewer, creature, ownLevel);
                    } : LocalLevelScaling::CreatureWindowResolver(nullptr), std::memory_order_relaxed);
                LocalLevelScaling::ScalingScopeOwner.store(journey ?
                    [](std::vector<uint32>& worldMaps, std::vector<std::pair<uint32, LocalLevelScaling::LevelBand>>& bands)
                    {
                        sJourneyContent->AddScalingScope(worldMaps, bands);
                    } : LocalLevelScaling::ScalingScopeResolver(nullptr), std::memory_order_relaxed);
                LocalLevelScaling::BotLevelPlanOwner.store(journey ?
                    [](LocalLevelScaling::BotLevelPlan& plan) -> bool
                    {
                        ProgressionLayout const& layout = sCoAContentScaling->GetLayout();
                        return layout.journey && journey::FillBotLevelPlan(layout.journeyRules,
                            sCoAContentScaling->GetJourneySettings().trainers, layout.maxLevel, plan);
                    } : LocalLevelScaling::BotLevelPlanResolver(nullptr), std::memory_order_relaxed);

                LocalLevelScaling::QuestBaseLevelOwner.store([](Quest const* quest) -> int32
                {
                    return sCoAContentScaling->GetEffectiveQuestLevel(quest);
                }, std::memory_order_relaxed);

                LocalLevelScaling::QuestMinLevelOwner.store([](Quest const* quest) -> uint32
                {
                    return sCoAContentScaling->GetEffectiveQuestMinLevel(quest);
                }, std::memory_order_relaxed);

                LocalLevelScaling::AbilityRequiredLevelOwner.store([](uint8 requiredLevel) -> uint8
                {
                    return sCoAContentScaling->GetEffectiveAbilityRequiredLevel(requiredLevel);
                }, std::memory_order_relaxed);

                LocalLevelScaling::CreatureBaseLevelOwner.store([](CreatureTemplate const* cinfo, Creature const* creature) -> uint8
                {
                    return sCoAContentScaling->GetEffectiveCreatureLevel(cinfo, creature, cinfo ? cinfo->maxlevel : 1);
                }, std::memory_order_relaxed);

                LocalLevelScaling::AreaContentLevelOwner.store([](uint32 areaId, uint32 mapId, uint8 authoredLevel) -> uint8
                {
                    return sCoAContentScaling->GetEffectiveAreaContentLevel(areaId, mapId, authoredLevel);
                }, std::memory_order_relaxed);

                LocalLevelScaling::CreatureArmorOwner.store([](CreatureTemplate const* cinfo, Creature const* creature,
                    float generatedArmor) -> float
                {
                    return sCoAContentScaling->GetEffectiveCreatureArmor(cinfo, creature, generatedArmor);
                }, std::memory_order_relaxed);

                // The flat damage or healing a spell cast from an item carries. Rewriting the item
                // leaves its spells alone, so the one number still written for the level the item
                // came from is asked about here, with the same factor its statistics were cut by.
                LocalLevelScaling::ItemEffectValueOwner.store([](uint32 itemEntry, int32 value) -> int32
                {
                    if (!sCoAContentScaling->IsEnabled())
                        return value;

                    float const multiplier = sItemBudgetScaler->GetStatMultiplier(itemEntry);
                    if (multiplier >= 1.0f)
                        return value;

                    int32 const scaled = static_cast<int32>(std::lround(float(value) * multiplier));
                    return value > 0 ? std::max(1, scaled) : std::min(-1, scaled);
                }, std::memory_order_relaxed);

                // What an item is worth to those who price it by its vendor value: moved to the level
                // the item has on this realm, the way its statistics were.
                LocalLevelScaling::ItemMarketValueOwner.store([](uint32 itemEntry, uint32 value) -> uint32
                {
                    if (!sCoAContentScaling->IsEnabled())
                        return value;
                    return sItemBudgetScaler->ScaleMarketValue(itemEntry, value);
                }, std::memory_order_relaxed);

                // Tables written against authored item levels read a character's gear at the level it was
                // written with.
                LocalLevelScaling::AuthoredItemLevelOwner.store([](uint32 itemEntry, uint32 itemLevel) -> uint32
                {
                    if (!sCoAContentScaling->IsEnabled())
                        return itemLevel;
                    return sItemBudgetScaler->AuthoredItemLevel(itemEntry, itemLevel);
                }, std::memory_order_relaxed);

                // An enchantment, a gem and a socket bonus are all the same kind of entry and all
                // ride on an item that was cut, so the host's factor is the one that applies.
                LocalLevelScaling::ItemEnchantmentAmountOwner.store(
                    [](uint32 hostItemEntry, uint32 enchantmentType, uint32 statType, uint32 amount) -> uint32
                {
                    // On the journey the enchantments themselves were cut at startup, by their own game.
                    if (!sCoAContentScaling->IsEnabled() || sCoAContentScaling->GetLayout().journey ||
                        !sConfigMgr->GetOption<bool>(CoAContentScalingConfigKeys::ScaleEnchantments, true))
                        return amount;

                    float multiplier = 1.0f;
                    switch (enchantmentType)
                    {
                        case ITEM_ENCHANTMENT_TYPE_STAT:
                            multiplier = GetItemModCategory(statType) == ItemModCategory::SecondaryRating ?
                                sItemBudgetScaler->GetRatingMultiplier(hostItemEntry) :
                                sItemBudgetScaler->GetStatMultiplier(hostItemEntry);
                            break;
                        case ITEM_ENCHANTMENT_TYPE_DAMAGE:
                        case ITEM_ENCHANTMENT_TYPE_RESISTANCE:
                        case ITEM_ENCHANTMENT_TYPE_TOTEM:
                            multiplier = sItemBudgetScaler->GetStatMultiplier(hostItemEntry);
                            break;
                        default:
                            return amount;
                    }

                    if (multiplier >= 1.0f)
                        return amount;

                    return std::max<uint32>(1, static_cast<uint32>(std::lround(float(amount) * multiplier)));
                }, std::memory_order_relaxed);

                // An enchantment carries no era of its own, but the level it asks for names one: a
                // gem that wants 80 is Northrend's. Read that way it moves with the items it goes
                // into, instead of staying out of reach of everyone on a realm that ends below it.
                LocalLevelScaling::EnchantmentRequiredLevelOwner.store([](uint32 requiredLevel) -> uint32
                {
                    if (!sCoAContentScaling->IsEnabled() || sCoAContentScaling->GetLayout().journey)
                        return requiredLevel;

                    ProgressionLayout const& layout = sCoAContentScaling->GetLayout();
                    uint8 const authored = static_cast<uint8>(std::min<uint32>(requiredLevel, 255));

                    return layout.MapAuthoredToEffective(layout.GetEraForAuthoredLevel(authored), authored);
                }, std::memory_order_relaxed);

                // Progression belongs to this module while it runs: the core stands down over the
                // same three questions, so they are answered once, here.
                LocalLevelScaling::QuestMoneyMaxLevelOwner.store([](Quest const* quest, uint32 defaultMoney) -> uint32
                {
                    if (!quest || !sCoAContentScaling->IsEnabled())
                        return defaultMoney;

                    auto const& layout = sCoAContentScaling->GetLayout();
                    ContentEra const era = sContentEraResolver->ResolveEraForQuest(
                        quest->GetQuestId(), quest->GetZoneOrSort(), 0, static_cast<uint8>(quest->GetQuestLevel()));
                    int32 const effectiveLevel = sCoAContentScaling->GetEffectiveQuestLevel(quest);
                    uint32 const effectiveXp = sProgressionRewardResolver->ResolveQuestXP(
                        quest->XPValue(layout.maxLevel, false), quest->GetQuestLevel(), effectiveLevel, layout, era);

                    return static_cast<uint32>(sProgressionRewardResolver->ResolveMoneyAtCap(effectiveXp, 1.0f));
                }, std::memory_order_relaxed);

                LocalLevelScaling::KillContentLevelOwner.store([](Player const* player, Unit const* /*victim*/,
                    uint8 defaultContent) -> uint8
                {
                    if (!player || !sCoAContentScaling->IsEnabled())
                        return defaultContent;

                    auto const& layout = sCoAContentScaling->GetLayout();
                    if (layout.IsStockIdentity())
                        return defaultContent;

                    // The band follows the character, not the zone: a compressed realm would
                    // otherwise pay the 580 base of a band the character cannot be in.
                    uint8 const playerLevel = player->GetLevel();
                    if (playerLevel >= 71)
                        return 2;
                    if (playerLevel >= 61)
                        return 1;

                    return 0;
                }, std::memory_order_relaxed);

                LocalLevelScaling::QuestRewardRateOwner.store([](Player const* player, Quest const* quest,
                    float defaultRate) -> float
                {
                    if (!player || !quest || !sCoAContentScaling->IsEnabled())
                        return defaultRate;

                    if (quest->IsDFQuest())
                        return sWorld->getRate(RATE_XP_QUEST_DF);

                    if (sCoAContentScaling->GetLayout().journey)
                        return sJourneyContent->GetQuestXpRate(quest->GetQuestId(), defaultRate);

                    ContentEra const era = sContentEraResolver->ResolveEraForQuest(
                        quest->GetQuestId(), quest->GetZoneOrSort(), 0, static_cast<uint8>(quest->GetQuestLevel()));

                    switch (era)
                    {
                        case ContentEra::WotLK:
                            return sWorld->getRate(RATE_XP_QUEST_WOTLK);
                        case ContentEra::TBC:
                            return sWorld->getRate(RATE_XP_QUEST_TBC);
                        case ContentEra::Classic:
                        default:
                            return sWorld->getRate(RATE_XP_QUEST);
                    }
                }, std::memory_order_relaxed);
            }
            else
            {
                LocalLevelScaling::AuthenticMapOwner.store(nullptr, std::memory_order_relaxed);
                LocalLevelScaling::CreatureWindowOwner.store(nullptr, std::memory_order_relaxed);
                LocalLevelScaling::ScalingScopeOwner.store(nullptr, std::memory_order_relaxed);
                LocalLevelScaling::BotLevelPlanOwner.store(nullptr, std::memory_order_relaxed);
                LocalLevelScaling::QuestBaseLevelOwner.store(nullptr, std::memory_order_relaxed);
                LocalLevelScaling::QuestMinLevelOwner.store(nullptr, std::memory_order_relaxed);
                LocalLevelScaling::AbilityRequiredLevelOwner.store(nullptr, std::memory_order_relaxed);
                LocalLevelScaling::CreatureBaseLevelOwner.store(nullptr, std::memory_order_relaxed);
                LocalLevelScaling::AreaContentLevelOwner.store(nullptr, std::memory_order_relaxed);
                LocalLevelScaling::CreatureArmorOwner.store(nullptr, std::memory_order_relaxed);
                LocalLevelScaling::ItemEffectValueOwner.store(nullptr, std::memory_order_relaxed);
                LocalLevelScaling::ItemMarketValueOwner.store(nullptr, std::memory_order_relaxed);
                LocalLevelScaling::AuthoredItemLevelOwner.store(nullptr, std::memory_order_relaxed);
                LocalLevelScaling::ItemEnchantmentAmountOwner.store(nullptr, std::memory_order_relaxed);
                LocalLevelScaling::EnchantmentRequiredLevelOwner.store(nullptr, std::memory_order_relaxed);
                LocalLevelScaling::QuestMoneyMaxLevelOwner.store(nullptr, std::memory_order_relaxed);
                LocalLevelScaling::KillContentLevelOwner.store(nullptr, std::memory_order_relaxed);
                LocalLevelScaling::QuestRewardRateOwner.store(nullptr, std::memory_order_relaxed);
            }
        }
    };

    class coa_content_scaling_global : public GlobalScript
    {
    public:
        coa_content_scaling_global() : GlobalScript("coa_content_scaling_global") { }

        void OnInitializeLockedDungeons(Player* player, uint8& /*level*/, uint32& lockData, lfg::LFGDungeonData const* dungeon) override
        {
            if (!sCoAContentScaling->IsEnabled() || !player || !dungeon)
                return;

            if (sCoAContentScaling->IsAuthenticMap(dungeon->map, dungeon->difficulty))
            {
                uint8 const minLevel = sCoAContentScaling->GetAuthenticMinLevel(dungeon->map, dungeon->difficulty,
                    dungeon->minlevel);
                if (player->GetLevel() < minLevel)
                    lockData = lfg::LFG_LOCKSTATUS_TOO_LOW_LEVEL;
                else if (minLevel != dungeon->minlevel && lockData == lfg::LFG_LOCKSTATUS_TOO_LOW_LEVEL)
                    lockData = 0;
                return;
            }

            ContentEra const era = sCoAContentScaling->ResolveMapEra(dungeon->map, dungeon->difficulty);

            // Check if era is disabled
            if ((era == ContentEra::TBC && !sCoAContentScaling->IsTbcEnabled()) ||
                (era == ContentEra::WotLK && !sCoAContentScaling->IsWotlkEnabled()))
            {
                lockData = lfg::LFG_LOCKSTATUS_INSUFFICIENT_EXPANSION;
                return;
            }

            // On the journey the finder's own entries were moved at startup, so its levels already hold.
            if (sCoAContentScaling->GetLayout().journey)
                return;

            // Compute the effective level at which this dungeon opens
            auto const& layout = sCoAContentScaling->GetLayout();
            uint8 const effMin = layout.MapAuthoredToEffective(era, dungeon->minlevel);
            uint8 const playerLevel = player->GetLevel();

            if (playerLevel < effMin)
            {
                lockData = lfg::LFG_LOCKSTATUS_TOO_LOW_LEVEL;
            }
            // Scaled content rises to whoever enters it, so there is no upper level that locks a
            // dungeon out of the finder. Only the entry level still applies.
            else if (lockData == lfg::LFG_LOCKSTATUS_TOO_LOW_LEVEL || lockData == lfg::LFG_LOCKSTATUS_TOO_HIGH_LEVEL)
            {
                lockData = 0;
            }
        }

        void OnBeforeSetBossState(uint32 id, EncounterState newState, EncounterState /*oldState*/, Map* instance) override
        {
            if (!sCoAContentScaling->IsEnabled() || !instance || !instance->IsDungeon())
                return;

            if (newState == IN_PROGRESS)
            {
                sInstanceScalingMgr->BeginEncounter(
                    instance,
                    EncounterLifecycleSource::INSTANCE_SCRIPT,
                    EncounterKey{EncounterKeyType::INSTANCE_ENCOUNTER, id},
                    nullptr);
            }
            else if (newState == DONE)
            {
                sInstanceScalingMgr->EndEncounter(
                    instance,
                    EncounterLifecycleSource::INSTANCE_SCRIPT,
                    EncounterKey{EncounterKeyType::INSTANCE_ENCOUNTER, id},
                    EncounterLockState::COMPLETED);
            }
            else if (newState == FAIL || newState == NOT_STARTED)
            {
                sInstanceScalingMgr->EndEncounter(
                    instance,
                    EncounterLifecycleSource::INSTANCE_SCRIPT,
                    EncounterKey{EncounterKeyType::INSTANCE_ENCOUNTER, id},
                    EncounterLockState::RESETTING);
            }
        }
    };

    class coa_content_scaling_creature : public AllCreatureScript
    {
    public:
        coa_content_scaling_creature() : AllCreatureScript("coa_content_scaling_creature") { }

        void OnBeforeCreatureSelectLevel(CreatureTemplate const* cinfo, Creature* creature, uint8& level) override
        {
            if (!sCoAContentScaling->IsEnabled())
                return;

            level = sCoAContentScaling->GetEffectiveCreatureLevel(cinfo, creature, level);
        }

        void OnCreatureSelectLevel(CreatureTemplate const* cinfo, Creature* creature) override
        {
            sCoAContentScaling->CountCreatureHookCall();

            if (!sCoAContentScaling->IsEnabled())
                return;

            sCoAContentScaling->ApplyCreatureScaling(cinfo, creature);
        }
    };

    class coa_content_scaling_unit : public UnitScript
    {
    public:
        coa_content_scaling_unit() : UnitScript("coa_content_scaling_unit") { }

        // The realm's dungeon difficulty, on top of what group scaling and solo assist decided: only
        // what the instance's enemies deal to players and what they control.
        static float DungeonDamageMultiplier(Unit const* target, Unit const* attacker)
        {
            return target && target->IsControlledByPlayer() && !attacker->IsControlledByPlayer()
                ? sCoAContentScaling->GetDamageMultiplier() : 1.0f;
        }

        void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
        {
            if (!sCoAContentScaling->IsEnabled() || !sCoAContentScaling->IsWorldLeechEnabled() || !damage ||
                !attacker || !attacker->IsPlayer() || !attacker->IsAlive() || !victim || attacker == victim ||
                !victim->IsAlive() || !victim->IsCreature() || victim->IsControlledByPlayer() ||
                victim->ToCreature()->IsEvadingAttacks() || victim->IsInFlight() ||
                !attacker->GetMap() || attacker->GetMap() != victim->GetMap() || !attacker->GetMap()->IsWorldMap())
                return;

            // Damage is already mitigated here, and overkill heals nothing.
            uint32 const dealt = std::min(damage, victim->GetHealth());
            uint32 const missing = attacker->GetMaxHealth() - attacker->GetHealth();
            uint32 const healing = static_cast<uint32>(std::min<double>(missing,
                dealt * static_cast<double>(sCoAContentScaling->GetWorldLeechPercent()) / 100.0));
            if (healing)
                Unit::DealHeal(attacker, attacker, healing);
        }

        void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
        {
            if (!sCoAContentScaling->IsEnabled() || !sCoAContentScaling->IsGroupScalingEnabled() || damage == 0)
                return;

            if (!attacker || !attacker->IsCreature() || !attacker->GetMap() || !attacker->GetMap()->IsDungeon() ||
                sCoAContentScaling->IsAuthenticMap(attacker->GetMap()))
                return;

            InstanceScaleContext const ctx = sInstanceScalingMgr->GetOrCreateContext(attacker->GetMap());
            bool const isSolo = (ctx.effectivePlayers <= 1.0f);
            float const soloMitigation = sSoloAssistPolicy->GetDamageMitigationMultiplier(isSolo, true);

            damage = static_cast<uint32>(std::ceil(float(damage) * ctx.damageScale * soloMitigation *
                DungeonDamageMultiplier(target, attacker)));
        }

        // Scripted lethal and percentage-of-health damage keeps its size.
        static bool KeepsItsSize(int64 damage, SpellInfo const* spellInfo)
        {
            if (damage >= 10000000)
                return true;
            if (spellInfo)
                for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
                    if (spellInfo->Effects[i].Effect == SPELL_EFFECT_INSTAKILL ||
                        spellInfo->Effects[i].ApplyAuraName == SPELL_AURA_PERIODIC_DAMAGE_PERCENT)
                        return true;
            return false;
        }

        // On the journey a moved creature's spells still carry the numbers of its original level.
        static double JourneySpellFactor(Unit const* attacker, int64 damage, SpellInfo const* spellInfo)
        {
            if (!sCoAContentScaling->GetLayout().journey || KeepsItsSize(damage, spellInfo))
                return 1.0;
            return sJourneyContent->GetSpellFactor(attacker);
        }

        void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
        {
            if (!sCoAContentScaling->IsEnabled() || damage <= 0)
                return;

            if (double const factor = JourneySpellFactor(attacker, damage, spellInfo); factor < 1.0)
                damage = std::max<int32>(1, int32(std::lround(double(damage) * factor)));

            if (!sCoAContentScaling->IsGroupScalingEnabled())
                return;

            if (!attacker || !attacker->IsCreature() || !attacker->GetMap() || !attacker->GetMap()->IsDungeon() ||
                sCoAContentScaling->IsAuthenticMap(attacker->GetMap()))
                return;

            // Instakill and extreme script damage mechanic protection
            if (damage >= 10000000)
                return;

            if (spellInfo)
            {
                if (spellInfo->HasAttribute(SPELL_ATTR0_CU_AURA_CC))
                    return;

                for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
                {
                    if (spellInfo->Effects[i].Effect == SPELL_EFFECT_INSTAKILL)
                        return;
                    if (spellInfo->Effects[i].ApplyAuraName == SPELL_AURA_PERIODIC_DAMAGE_PERCENT)
                        return;
                }
            }

            InstanceScaleContext const ctx = sInstanceScalingMgr->GetOrCreateContext(attacker->GetMap());
            bool const isSolo = (ctx.effectivePlayers <= 1.0f);
            float const soloMitigation = sSoloAssistPolicy->GetDamageMitigationMultiplier(isSolo, true);

            damage = static_cast<int32>(std::ceil(float(damage) * ctx.damageScale * soloMitigation *
                DungeonDamageMultiplier(target, attacker)));
        }

        void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage, SpellInfo const* spellInfo) override
        {
            if (!sCoAContentScaling->IsEnabled() || damage == 0)
                return;

            // The same hook carries heals over time; those are left alone.
            bool const harmful = !(spellInfo && spellInfo->IsPositive()) &&
                !(target && attacker && target->IsFriendlyTo(attacker));
            if (double const factor = harmful ? JourneySpellFactor(attacker, damage, spellInfo) : 1.0; factor < 1.0)
                damage = std::max<uint32>(1, uint32(std::lround(double(damage) * factor)));

            if (!sCoAContentScaling->IsGroupScalingEnabled())
                return;

            if (!attacker || !attacker->IsCreature() || !attacker->GetMap() || !attacker->GetMap()->IsDungeon() ||
                sCoAContentScaling->IsAuthenticMap(attacker->GetMap()))
                return;

            // Scripted lethal and percentage-of-health ticks keep their size, as the direct spell hook does.
            float difficulty = damage >= 10000000 ? 1.0f : DungeonDamageMultiplier(target, attacker);
            if (spellInfo)
                for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
                    if (spellInfo->Effects[i].Effect == SPELL_EFFECT_INSTAKILL ||
                        spellInfo->Effects[i].ApplyAuraName == SPELL_AURA_PERIODIC_DAMAGE_PERCENT)
                        difficulty = 1.0f;

            InstanceScaleContext const ctx = sInstanceScalingMgr->GetOrCreateContext(attacker->GetMap());
            damage = static_cast<uint32>(std::ceil(float(damage) * ctx.damageScale * difficulty));
        }

        void ModifyHealReceived(Unit* target, Unit* healer, uint32& heal, SpellInfo const* /*spellInfo*/) override
        {
            if (!sCoAContentScaling->IsEnabled() || !sCoAContentScaling->IsGroupScalingEnabled() || heal == 0)
                return;

            if (!target || !target->IsCreature() || !target->GetMap() || !target->GetMap()->IsDungeon() ||
                sCoAContentScaling->IsAuthenticMap(target->GetMap()))
                return;

            InstanceScaleContext const ctx = sInstanceScalingMgr->GetOrCreateContext(target->GetMap());
            heal = static_cast<uint32>(std::ceil(float(heal) * ctx.healingScale));
        }

        void OnAfterAuraEffectCalculateAmount(AuraEffect const* effect, Unit* caster, int32& amount) override
        {
            if (!sCoAContentScaling->IsEnabled() || !effect || !amount)
                return;

            ScaleItemGrantedAmount(effect, amount);

            if (!sCoAContentScaling->IsGroupScalingEnabled() || amount <= 0)
                return;

            if (!caster || !caster->IsCreature() || !caster->GetMap() || !caster->GetMap()->IsDungeon() ||
                sCoAContentScaling->IsAuthenticMap(caster->GetMap()))
                return;

            if (effect->GetAuraType() == SPELL_AURA_SCHOOL_ABSORB ||
                effect->GetAuraType() == SPELL_AURA_MANA_SHIELD)
            {
                InstanceScaleContext const ctx = sInstanceScalingMgr->GetOrCreateContext(caster->GetMap());
                amount = static_cast<int32>(std::ceil(float(amount) * ctx.absorbScale));
            }
        }

        // What an item hands out through a spell is not touched when its template is rewritten: those
        // numbers belong to the spell. A weapon whose statistics were cut to a third keeps a proc
        // written for the level it came from, and that proc is then the one number on it still out of
        // proportion.
        //
        // Only flat amounts, and only from an item that was actually cut. Percentages are a different
        // aura type and never reach the switch, and whatever a coefficient adds is already in
        // proportion because the statistics it reads were cut.
        static void ScaleItemGrantedAmount(AuraEffect const* effect, int32& amount)
        {
            Aura const* aura = effect->GetBase();
            if (!aura)
                return;

            // The template entry, not the item: a consumable is used up by the time its buff is cast
            // (the last food of a stack), and it has to be cut exactly like the stack it came from.
            uint32 const castItemEntry = aura->GetCastItemEntry();
            if (!castItemEntry)
                return;

            Unit* owner = aura->GetCaster();
            Player* player = owner ? owner->ToPlayer() : nullptr;
            if (!player)
                return;

            float multiplier = 1.0f;
            switch (GetAuraAmountFactor(effect->GetAuraType()))
            {
                case ScaledAmountFactor::Stat:
                    multiplier = sItemBudgetScaler->GetStatMultiplier(castItemEntry);
                    break;
                case ScaledAmountFactor::Rating:
                    multiplier = sItemBudgetScaler->GetRatingMultiplier(castItemEntry);
                    break;
                case ScaledAmountFactor::None:
                    return;
            }

            if (multiplier >= 1.0f)
                return;

            int32 const authored = amount;
            int32 const scaled = static_cast<int32>(std::lround(float(amount) * multiplier));
            amount = amount > 0 ? std::max(1, scaled) : std::min(-1, scaled);

            ScaledAuraFeed::Publish(player, aura->GetId(), effect->GetEffIndex(), authored, amount);
        }

        void OnUnitEnterCombat(Unit* unit, Unit* /*victim*/) override
        {
            if (!sCoAContentScaling->IsEnabled() || !unit)
                return;

            Creature* creature = unit->ToCreature();
            if (!creature || !creature->GetMap() || !creature->GetMap()->IsDungeon())
                return;

            CreatureTemplate const* cinfo = creature->GetCreatureTemplate();
            // Authoritative encounter start: only bosses lock the encounter snapshot! Trash never locks.
            if (cinfo && (cinfo->rank == CREATURE_ELITE_WORLDBOSS || (cinfo->flags_extra & CREATURE_FLAG_EXTRA_DUNGEON_BOSS)))
            {
                sInstanceScalingMgr->BeginEncounter(
                    creature->GetMap(),
                    EncounterLifecycleSource::CREATURE_FALLBACK,
                    EncounterKey{EncounterKeyType::CREATURE_ENTRY, creature->GetEntry()},
                    creature);
            }
        }

        void OnUnitExitCombat(Unit* unit) override
        {
            if (!sCoAContentScaling->IsEnabled() || !unit)
                return;

            Creature* creature = unit->ToCreature();
            if (!creature || !creature->GetMap() || !creature->GetMap()->IsDungeon())
                return;

            CreatureTemplate const* cinfo = creature->GetCreatureTemplate();
            if (cinfo && (cinfo->rank == CREATURE_ELITE_WORLDBOSS || (cinfo->flags_extra & CREATURE_FLAG_EXTRA_DUNGEON_BOSS)))
            {
                sInstanceScalingMgr->EndEncounter(
                    creature->GetMap(),
                    EncounterLifecycleSource::CREATURE_FALLBACK,
                    EncounterKey{EncounterKeyType::CREATURE_ENTRY, creature->GetEntry()},
                    EncounterLockState::RESETTING);
            }
        }

        void OnUnitDeath(Unit* unit, Unit* /*killer*/) override
        {
            if (!sCoAContentScaling->IsEnabled() || !unit)
                return;

            Creature* creature = unit->ToCreature();
            if (!creature || !creature->GetMap() || !creature->GetMap()->IsDungeon())
                return;

            CreatureTemplate const* cinfo = creature->GetCreatureTemplate();
            if (cinfo && (cinfo->rank == CREATURE_ELITE_WORLDBOSS || (cinfo->flags_extra & CREATURE_FLAG_EXTRA_DUNGEON_BOSS)))
            {
                sInstanceScalingMgr->EndEncounter(
                    creature->GetMap(),
                    EncounterLifecycleSource::CREATURE_FALLBACK,
                    EncounterKey{EncounterKeyType::CREATURE_ENTRY, creature->GetEntry()},
                    EncounterLockState::COMPLETED);
            }
        }
    };

    class coa_content_scaling_player : public PlayerScript
    {
    public:
        coa_content_scaling_player() : PlayerScript("coa_content_scaling_player") { }

        bool OnPlayerCanEnterMap(Player* player, MapEntry const* entry, InstanceTemplate const* /*instance*/,
                                 MapDifficulty const* /*mapDiff*/, bool /*loginCheck*/) override
        {
            if (!sCoAContentScaling->IsEnabled())
                return true;

            return sCoAContentScaling->CanPlayerEnterMap(player, entry->MapID);
        }

        bool OnPlayerCanTakeQuest(Player const* /*player*/, Quest const* quest) override
        {
            if (!sCoAContentScaling->IsEnabled() || !quest)
                return true;

            ContentEra const era = sContentEraResolver->ResolveEraForQuest(
                quest->GetQuestId(), quest->GetZoneOrSort(), 0, static_cast<uint8>(quest->GetQuestLevel()));

            if (!sCoAContentScaling->GetLayout().IsEraEnabled(era))
                return false;

            return true;
        }

        void OnResolveDungeonAccessLevels(Player const* player, uint32 mapId, Difficulty difficulty,
            uint8& minLevel, uint8& maxLevel) override
        {
            if (!sCoAContentScaling->IsEnabled() || !player)
                return;

            if (sCoAContentScaling->IsAuthenticMap(mapId, difficulty))
            {
                if (!IsRetrievingOwnCorpse(player, mapId))
                    minLevel = sCoAContentScaling->GetAuthenticMinLevel(mapId, difficulty, minLevel);
                return;
            }

            if (IsRetrievingOwnCorpse(player, mapId))
            {
                minLevel = 0;
                maxLevel = 0;
                return;
            }

            ContentEra const era = sCoAContentScaling->ResolveMapEra(mapId, uint8(difficulty));

            // Check if era is disabled
            if ((era == ContentEra::TBC && !sCoAContentScaling->IsTbcEnabled()) ||
                (era == ContentEra::WotLK && !sCoAContentScaling->IsWotlkEnabled()))
            {
                minLevel = 255; // Lock out
                return;
            }

            auto const& layout = sCoAContentScaling->GetLayout();
            if (layout.journey)
            {
                // An instance opens at its journey entry and stays open to the cap.
                std::optional<uint8> entry = sJourneyContent->GetInstanceEntry(mapId, uint8(difficulty));
                if (!entry && difficulty != REGULAR_DIFFICULTY)
                    entry = sJourneyContent->GetInstanceEntry(mapId, REGULAR_DIFFICULTY);
                if (entry && minLevel > 0)
                    minLevel = *entry;
                maxLevel = 0;
                return;
            }

            if (minLevel > 0)
                minLevel = layout.MapAuthoredToEffective(era, minLevel);
            if (maxLevel > 0)
                maxLevel = layout.MapAuthoredToEffective(era, maxLevel);
        }

        void OnPlayerLogout(Player* player) override
        {
            if (!sCoAContentScaling->IsEnabled())
                return;
            sCoAContentScaling->OnPlayerLogout(player);
        }

        // At login the ZoneLevels addon learns the journey's zone and dungeon levels and what gems and enchantments
        // really give.
        void OnPlayerLogin(Player* player) override
        {
            TellAddon(player);
        }

        // The addon asks again once the interface is up: what came at login may have come before it was loaded.
        bool OnPlayerCanUseChat(Player* player, uint32 /*type*/, uint32 language, std::string& msg,
            Player* receiver) override
        {
            if (language != LANG_ADDON || receiver != player || !journey::addon::IsRequest(msg) ||
                !sCoAContentScaling->IsEnabled() || !sCoAContentScaling->GetLayout().journey)
                return true;

            TellAddon(player);
            return false;
        }

    private:
        static void TellAddon(Player* player)
        {
            if (!player || !player->GetSession() || !sCoAContentScaling->IsEnabled() ||
                !sCoAContentScaling->GetLayout().journey)
                return;

            for (std::string const& body : sJourneyContent->GetAddonMessages())
            {
                WorldPacket data;
                ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player,
                    std::string(journey::addon::Prefix) + "\t" + body);
                player->GetSession()->SendPacket(&data);
            }
        }
    };

    class coa_content_scaling_misc : public MiscScript
    {
    public:
        coa_content_scaling_misc() : MiscScript("coa_content_scaling_misc") { }

        void OnAfterLootTemplateProcess(Loot* loot, LootTemplate const* /*tab*/, LootStore const& /*store*/,
                                       Player* lootOwner, bool /*personal*/, bool /*noEmptyError*/,
                                       uint16 /*lootMode*/) override
        {
            if (!sCoAContentScaling->IsEnabled() || !loot || !lootOwner)
                return;

            // Mail loot is generated while a character loads, before it has a map (achievement rewards in
            // Player::LoadFromDB): GetMap() would assert there.
            Map* map = lootOwner->FindMap();
            if (!map || !map->IsDungeon() || sCoAContentScaling->IsAuthenticMap(map))
                return;

            InstanceScaleContext const ctx = sInstanceScalingMgr->GetOrCreateContext(map);
            if (ctx.effectivePlayers >= float(ctx.intendedPlayers))
                return; // Full group, standard loot

            float const ratio = ctx.effectivePlayers / float(ctx.intendedPlayers);

            // Separate quest items and regular items:
            // 1) 100% preservation of quest items
            // 2) Shuffle regular items to eliminate positional slot bias
            std::vector<LootItem> questItems;
            std::vector<LootItem> regularItems;

            for (LootItem const& item : loot->items)
            {
                ItemTemplate const* proto = sObjectMgr->GetItemTemplate(item.itemid);
                if (proto && (proto->Class == ITEM_CLASS_QUEST || proto->Bonding == BIND_QUEST_ITEM || proto->Bonding == BIND_QUEST_ITEM1 || proto->StartQuest > 0))
                    questItems.push_back(item);
                else
                    regularItems.push_back(item);
            }

            if (!regularItems.empty())
            {
                uint32 const keepCount = std::max<uint32>(1, static_cast<uint32>(std::round(float(regularItems.size()) * ratio)));
                if (keepCount < regularItems.size())
                {
                    Acore::Containers::RandomShuffle(regularItems);
                    regularItems.resize(keepCount);
                }
            }

            loot->items.clear();
            loot->items.insert(loot->items.end(), questItems.begin(), questItems.end());
            loot->items.insert(loot->items.end(), regularItems.begin(), regularItems.end());
            loot->unlootedCount = static_cast<uint8>(loot->items.size());
        }
    };

    class coa_content_scaling_map : public AllMapScript
    {
    public:
        coa_content_scaling_map() : AllMapScript("coa_content_scaling_map") { }
        void OnPlayerEnterAll(Map* map, Player* player) override
        {
            Rescale(map, player);
        }

        void OnPlayerLeaveAll(Map* map, Player* player) override
        {
            Rescale(map, player);
        }

        void OnDestroyMap(Map* map) override
        {
            if (map)
                sInstanceScalingMgr->RemoveMapContext(map->GetId(), map->GetInstanceId());
        }

        void OnResolveEncounterMechanic(Map* map, uint32 encounterId, uint32 mechanicId, uint8 mechanicType, uint32 authoredValue, uint32& resolvedValue) override
        {
            if (!sCoAContentScaling->IsEnabled() || !sCoAContentScaling->IsAdaptiveMechanicsEnabled() || !map ||
                sCoAContentScaling->IsAuthenticMap(map))
                return;

            EncounterContext ctx = sInstanceScalingMgr->BuildEncounterContext(map, encounterId);
            resolvedValue = sAdaptiveEncounterMgr->ResolveMechanic(
                map->GetId(), encounterId, mechanicId, static_cast<EncounterMechanicType>(mechanicType), authoredValue, ctx);
        }

    private:
        static void Rescale(Map* map, Player const* player)
        {
            if (!player || player->IsDuringRemoveFromWorld())
                return;

            uint32 const rescaled = sCoAContentScaling->RescaleDungeonCreatures(map);
            if (rescaled)
            {
                LOG_DEBUG("module.coa_content_scaling",
                          "CoAContentScaling: rescaled {} creatures on map {} for the group that is there now.",
                          rescaled, map->GetId());
            }
        }
    };
}

void RegisterCuratedEncounterAdapters();

void AddCoAContentScalingScripts()
{
    RegisterCuratedEncounterAdapters();
    new coa_content_scaling_world();
    new coa_content_scaling_global();
    new coa_content_scaling_creature();
    new coa_content_scaling_unit();
    new coa_content_scaling_player();
    new coa_content_scaling_misc();
    new coa_content_scaling_map();
    AddScaledAuraFeedScripts();
    AddCoAContentScalingCommands();
}
