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
#include "ContentPackRegistry.h"
#include "Creature.h"
#include "CreatureData.h"
#include "DBCEnums.h"
#include "DatabaseEnv.h"
#include "Field.h"
#include "GameTime.h"
#include "Group.h"
#include "GroupMgr.h"
#include "GeneratedContentCensus.h"
#include "InstanceProfile.h"
#include "InstanceScaleContext.h"
#include "ItemBudgetScaler.h"
#include "LFGMgr.h"
#include "LocalLevelScaling.h"
#include "Log.h"
#include "LootMgr.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ProgressionRewardResolver.h"
#include "QueryResult.h"
#include "QuestDef.h"
#include "ScriptMgr.h"
#include "SoloAssistPolicy.h"
#include "SpellAuraDefines.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "World.h"
#include <algorithm>
#include <cmath>

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

    std::string const defaultModeStr = sConfigMgr->GetOption<std::string>(CoAContentScalingConfigKeys::LfgDefaultMode, "Matchmaking");
    _defaultLfgCompositionMode = CoAContentScalingConfig::ParseLfgCompositionMode(defaultModeStr);

    if (_defaultLfgCompositionMode == lfg::LfgCompositionMode::BOT_FILL && !sScriptMgr->HasLfgAutoFillProvider())
    {
        LOG_WARN("module.coa_content_scaling", "CoAContentScaling: LFG default mode configured as BotFill, but no bot fill provider is registered! Falling back to Matchmaking.");
        _defaultLfgCompositionMode = lfg::LfgCompositionMode::MATCHMAKING;
    }

    uint32 const rawChallenge = sConfigMgr->GetOption<uint32>(CoAContentScalingConfigKeys::LfgDefaultChallengeSize, 0);
    if (rawChallenge > 40)
    {
        LOG_WARN("module.coa_content_scaling",
                 "CoAContentScaling: Invalid LFG.DefaultChallengeSize {} (must be 0..40). Falling back to 0 (Adaptive).", rawChallenge);
        _defaultLfgChallengeSize = 0;
    }
    else
    {
        _defaultLfgChallengeSize = rawChallenge;
    }

    std::string const soloMode = sConfigMgr->GetOption<std::string>(CoAContentScalingConfigKeys::SoloAssistMode, "0");
    sSoloAssistPolicy->SetMode(CoAContentScalingConfig::ParseSoloAssistMode(soloMode));
}

void CoAContentScaling::FinalizeAndInitialize()
{
    if (sContentPackRegistry->IsFinalized())
        return;

    // Switched off means switched off: no layout, no database read, no item templates rewritten. The
    // table this reads belongs to another module, and upstream read it before anything asked whether
    // this one was even enabled.
    if (!_enabled)
        return;

    // 1. Finalize content pack registrations
    sContentPackRegistry->Finalize();

    // 2. Compute immutable progression layout
    InitializeLayout();

    // 3. Load calibrated boss flex profiles
    sInstanceScalingMgr->LoadCalibratedBossFlex();

    // 4. Scale item templates if enabled
    if (sConfigMgr->GetOption<bool>(CoAContentScalingConfigKeys::ScaleItems, true))
    {
        sItemBudgetScaler->ScaleAllItems(_layout);
    }

    LOG_INFO("server.loading", "CoAContentScaling: Finalized lifecycle and built immutable ProgressionLayout (Cap {})",
             _layout.maxLevel);
}

void CoAContentScaling::InitializeLayout()
{
    uint8 const maxPlayerLevel = static_cast<uint8>(sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL));
    bool const tbcActive = _tbcEnabled && sContentPackRegistry->HasPack(ContentEra::TBC);
    bool const wotlkActive = _wotlkEnabled && sContentPackRegistry->HasPack(ContentEra::WotLK);

    if (_progressionMode == "Custom")
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

    Map const* map = creature ? creature->GetMap() : nullptr;
    uint32 const mapId = map ? map->GetId() : 0;
    uint32 const areaId = creature ? creature->GetAreaId() : 0;

    ContentEra const era = sContentPackRegistry->ResolveEraForCreature(
        cinfo->Entry, mapId, areaId, cinfo->expansion, authoredLevel);

    return _layout.MapAuthoredToEffective(era, authoredLevel);
}

int32 CoAContentScaling::GetEffectiveQuestLevel(Quest const* quest) const
{
    if (!_enabled || !quest)
        return quest ? quest->GetQuestLevel() : 0;

    int32 const authoredLevel = quest->GetQuestLevel();
    if (authoredLevel <= 0)
        return authoredLevel;

    ContentEra const era = sContentPackRegistry->ResolveEraForQuest(
        quest->GetQuestId(), quest->GetZoneOrSort(), 0, static_cast<uint8>(authoredLevel));

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

    ContentEra const era = sContentPackRegistry->ResolveEraForQuest(
        quest->GetQuestId(), quest->GetZoneOrSort(), 0, static_cast<uint8>(authoredMin));

    if (!_layout.IsEraEnabled(era))
        return 255; // Lock out quest from disabled expansion

    return static_cast<uint32>(_layout.MapAuthoredToEffective(era, static_cast<uint8>(authoredMin)));
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
    if (!_enabled || !cinfo)
        return generatedArmor;

    CreatureScaleContext const ctx = sCombatBudgetProfile->BuildContext(cinfo, creature,
        creature ? creature->GetLevel() : cinfo->maxlevel);

    return sCombatBudgetProfile->CalculateBudget(cinfo, ctx).armor;
}

void CoAContentScaling::ApplyCreatureScaling(CreatureTemplate const* cinfo, Creature* creature)
{
    if (!_enabled || !cinfo || !creature)
        return;

    CalculatedCombatBudget const budget = CalculateCreatureBudget(cinfo, creature);

    float const pct = creature->GetMaxHealth() ? creature->GetHealthPct() : 100.0f;
    creature->SetCreateHealth(budget.health);
    creature->SetStatFlatModifier(UNIT_MOD_HEALTH, BASE_VALUE, float(budget.health));
    creature->UpdateMaxHealth();
    creature->SetHealth(std::max<uint32>(1, static_cast<uint32>(std::round(float(creature->GetMaxHealth()) * pct / 100.0f))));

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
    if (!_enabled || !_groupScalingEnabled || !map || !map->IsDungeon())
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
    if (!map || !map->IsDungeon())
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
    if (!player || player->IsGameMaster())
        return true;

    if (IsRetrievingOwnCorpse(player, mapId))
        return true;

    ContentEra const era = sContentPackRegistry->ResolveEraForMap(mapId);
    if (!_layout.IsEraEnabled(era))
        return false;

    // The difficulty is carried for the profiles that answer per difficulty; nothing here reads it yet.
    (void)difficulty;

    return true;
}

uint8 CoAContentScaling::ResolveLfgRewardLevel(Player const* /*player*/, uint32 dungeonId, uint8 playerLevel) const
{
    if (!_enabled)
        return playerLevel;

    ContentEra era = ContentEra::Classic;
    if (auto const* lfgProfile = FindGeneratedLfgProfile(dungeonId))
    {
        era = lfgProfile->era;
    }
    else if (lfg::LFGDungeonData const* dungeon = sLFGMgr->GetLFGDungeon(dungeonId))
    {
        era = sContentPackRegistry->ResolveEraForMap(dungeon->map);
    }

    return sProgressionRewardResolver->ResolveLfgRewardLevel(era, playerLevel, _layout);
}

void CoAContentScaling::SetPlayerLfgMode(ObjectGuid guid, lfg::LfgCompositionMode mode)
{
    {
        std::lock_guard<std::mutex> lock(_lfgSettingsLock);
        _playerLfgSettings[guid].compositionMode = mode;
    }
    if (Player* player = ObjectAccessor::FindPlayer(guid))
        SavePlayerLfgSettings(player);
}

lfg::LfgCompositionMode CoAContentScaling::GetPlayerLfgMode(ObjectGuid guid) const
{
    std::lock_guard<std::mutex> lock(_lfgSettingsLock);
    auto it = _playerLfgSettings.find(guid);
    return (it != _playerLfgSettings.end()) ? it->second.compositionMode : _defaultLfgCompositionMode;
}

bool CoAContentScaling::SetPlayerLfgChallenge(ObjectGuid guid, uint32 challengeSize)
{
    if (challengeSize > 40)
        return false;

    {
        std::lock_guard<std::mutex> lock(_lfgSettingsLock);
        _playerLfgSettings[guid].challengeSize = challengeSize;
    }
    if (Player* player = ObjectAccessor::FindPlayer(guid))
        SavePlayerLfgSettings(player);

    return true;
}

uint32 CoAContentScaling::GetPlayerLfgChallenge(ObjectGuid guid) const
{
    std::lock_guard<std::mutex> lock(_lfgSettingsLock);
    auto it = _playerLfgSettings.find(guid);
    return (it != _playerLfgSettings.end()) ? it->second.challengeSize : _defaultLfgChallengeSize;
}

void CoAContentScaling::LoadPlayerLfgSettings(Player* player)
{
    if (!player || !_enabled)
        return;

    ObjectGuid const guid = player->GetGUID();
    QueryResult result = CharacterDatabase.Query(
        "SELECT composition_mode, challenge_size FROM character_coa_lfg_settings WHERE guid = {}",
        guid.GetCounter());

    std::lock_guard<std::mutex> lock(_lfgSettingsLock);
    if (result)
    {
        Field* fields = result->Fetch();
        uint8 const modeVal = fields[0].Get<uint8>();
        uint32 const challenge = fields[1].Get<uint32>();

        PlayerLfgSettings settings;
        if (modeVal <= static_cast<uint8>(lfg::LfgCompositionMode::CURRENT_PARTY))
            settings.compositionMode = static_cast<lfg::LfgCompositionMode>(modeVal);
        else
            settings.compositionMode = _defaultLfgCompositionMode;

        if (challenge <= 40)
            settings.challengeSize = challenge;
        else
            settings.challengeSize = _defaultLfgChallengeSize;

        _playerLfgSettings[guid] = settings;
    }
    else
    {
        PlayerLfgSettings settings;
        settings.compositionMode = _defaultLfgCompositionMode;
        settings.challengeSize = _defaultLfgChallengeSize;
        _playerLfgSettings[guid] = settings;
    }
}

void CoAContentScaling::SavePlayerLfgSettings(Player* player)
{
    if (!_enabled)
        return;

    if (!player)
        return;

    ObjectGuid const guid = player->GetGUID();
    PlayerLfgSettings settings;
    {
        std::lock_guard<std::mutex> lock(_lfgSettingsLock);
        auto it = _playerLfgSettings.find(guid);
        if (it != _playerLfgSettings.end())
            settings = it->second;
        else
        {
            settings.compositionMode = _defaultLfgCompositionMode;
            settings.challengeSize = _defaultLfgChallengeSize;
        }
    }

    CharacterDatabase.Execute(
        "REPLACE INTO character_coa_lfg_settings (guid, composition_mode, challenge_size) VALUES ({}, {}, {})",
        guid.GetCounter(), static_cast<uint8>(settings.compositionMode), settings.challengeSize);
}

void CoAContentScaling::OnPlayerLogout(Player* player)
{
    if (!player)
        return;

    SavePlayerLfgSettings(player);

    ObjectGuid const guid = player->GetGUID();
    {
        std::lock_guard<std::mutex> lock(_lfgSettingsLock);
        _playerLfgSettings.erase(guid);
    }
    {
        std::lock_guard<std::mutex> pLock(_pendingPolicyLock);
        auto pIt = _pendingPlayerPolicies.find(guid);
        if (pIt != _pendingPlayerPolicies.end())
        {
            uint64 const gen = pIt->second.generation;
            ObjectGuid const grpGuid = pIt->second.groupGuid;

            // Purge player alias
            _pendingPlayerPolicies.erase(pIt);

            // If player was in a group associated with this pending policy, check if group policy should also be cleaned
            // (e.g. If player is group leader or no remaining members in pending map)
            if (grpGuid)
            {
                Group* grp = player->GetGroup();
                if (!grp || grp->GetLeaderGUID() == guid || grp->GetMembersCount() <= 1)
                {
                    auto gIt = _pendingInstancePolicies.find(grpGuid);
                    if (gIt != _pendingInstancePolicies.end() && gIt->second.generation == gen)
                    {
                        _pendingInstancePolicies.erase(gIt);
                    }
                }
            }
        }
    }
}

void CoAContentScaling::OnResolveLfgQueuePolicy(ObjectGuid const& guid, lfg::LfgQueuePolicy& policy)
{
    // A group queues under its own guid, not its leader's: LFGMgr calls AddQueueData with gguid
    // once the role check finishes. Asked with that, there are no player settings to find and no
    // player to count a party from, so the mode the leader picked was lost and every group was
    // treated as one player - which then failed its own target of one.
    Group const* group = nullptr;
    ObjectGuid owner = guid;
    if (guid.IsGroup())
    {
        group = sGroupMgr->GetGroupByGUID(guid.GetCounter());
        if (group)
            owner = group->GetLeaderGUID();
    }
    else if (Player const* player = ObjectAccessor::FindPlayer(guid))
    {
        group = player->GetGroup();
    }

    PlayerLfgSettings settings;
    {
        std::lock_guard<std::mutex> lock(_lfgSettingsLock);
        auto it = _playerLfgSettings.find(owner);
        if (it != _playerLfgSettings.end())
            settings = it->second;
        else
        {
            settings.compositionMode = _defaultLfgCompositionMode;
            settings.challengeSize = _defaultLfgChallengeSize;
        }
    }

    // Capability check: If player selected BOT_FILL but server has no provider, fall back to MATCHMAKING
    if (settings.compositionMode == lfg::LfgCompositionMode::BOT_FILL && !sScriptMgr->HasLfgAutoFillProvider())
    {
        LOG_WARN("module.coa_content_scaling",
                 "CoAContentScaling: Player {} requested BOT_FILL but no provider registered. Falling back to MATCHMAKING.",
                 guid.ToString());
        settings.compositionMode = lfg::LfgCompositionMode::MATCHMAKING;
    }

    policy.compositionMode = settings.compositionMode;
    policy.challengeSize = settings.challengeSize;

    switch (settings.compositionMode)
    {
        case lfg::LfgCompositionMode::MATCHMAKING:
            policy.bypassMatchmaking = false;
            policy.requireStandardRoles = true;
            policy.minPlayers = 5;
            policy.targetPlayers = 5;
            break;

        case lfg::LfgCompositionMode::BOT_FILL:
        {
            // Upstream wrote this mode as matchmaking by another name, which meant a player who
            // picked it still waited for four strangers and the provider was never reached. Asking
            // for bots means not waiting: the group that is here forms at once, and whoever
            // provides bots fills it when the instance opens.
            policy.bypassMatchmaking = true;
            policy.requireStandardRoles = false;
            uint8 const currentPartySize = group ? uint8(group->GetMembersCount()) : 1;
            policy.minPlayers = currentPartySize;
            policy.targetPlayers = currentPartySize;
            break;
        }

        case lfg::LfgCompositionMode::CURRENT_PARTY:
        {
            policy.bypassMatchmaking = true;
            policy.requireStandardRoles = false;
            uint8 const currentPartySize = group ? uint8(group->GetMembersCount()) : 1;
            policy.minPlayers = currentPartySize;
            policy.targetPlayers = currentPartySize;
            break;
        }
    }
}

void CoAContentScaling::OnLfgProposalMadeGroup(lfg::LfgProposal const& proposal, Group* group)
{
    if (!_enabled || !group)
        return;

    lfg::LFGDungeonData const* dungeon = sLFGMgr->GetLFGDungeon(proposal.dungeonId);
    if (!dungeon)
        return;

    std::lock_guard<std::mutex> lock(_pendingPolicyLock);
    ++_pendingPolicyGeneration;

    PendingInstanceScalePolicy pendingPolicy;
    pendingPolicy.mapId = dungeon->map;
    pendingPolicy.groupGuid = group->GetGUID();
    pendingPolicy.challengeSize = proposal.policy.challengeSize;
    pendingPolicy.compositionMode = proposal.policy.compositionMode;
    pendingPolicy.generation = _pendingPolicyGeneration;
    pendingPolicy.createdAt = GameTime::GetGameTime().count();

    _pendingInstancePolicies[group->GetGUID()] = pendingPolicy;
}

std::optional<PendingInstanceScalePolicy> CoAContentScaling::ConsumePendingInstancePolicy(
    uint32 mapId, ObjectGuid groupGuid, ObjectGuid playerGuid)
{
    std::lock_guard<std::mutex> lock(_pendingPolicyLock);
    PurgeExpiredPendingPolicies();

    PendingInstanceScalePolicy policy;
    bool found = false;

    // Check group pending policy first
    if (groupGuid)
    {
        auto it = _pendingInstancePolicies.find(groupGuid);
        if (it != _pendingInstancePolicies.end() && it->second.mapId == mapId)
        {
            policy = it->second;
            found = true;
        }
    }

    // Check player pending policy fallback
    if (!found && playerGuid)
    {
        auto it = _pendingPlayerPolicies.find(playerGuid);
        if (it != _pendingPlayerPolicies.end() && it->second.mapId == mapId)
        {
            policy = it->second;
            found = true;
        }
    }

    if (!found)
        return std::nullopt;

    // Atomically erase matching group entry and all player aliases that share this generation
    uint64 const targetGeneration = policy.generation;

    if (policy.groupGuid)
    {
        auto gIt = _pendingInstancePolicies.find(policy.groupGuid);
        if (gIt != _pendingInstancePolicies.end() && gIt->second.generation == targetGeneration)
        {
            _pendingInstancePolicies.erase(gIt);
        }
    }

    for (auto it = _pendingPlayerPolicies.begin(); it != _pendingPlayerPolicies.end();)
    {
        if (it->second.generation == targetGeneration)
        {
            it = _pendingPlayerPolicies.erase(it);
        }
        else
        {
            ++it;
        }
    }

    return policy;
}

void CoAContentScaling::PurgeExpiredPendingPolicies()
{
    // 5 minutes (300 seconds) TTL
    constexpr uint32 PENDING_POLICY_TTL_SEC = 300;
    uint32 const now = GameTime::GetGameTime().count();

    for (auto it = _pendingInstancePolicies.begin(); it != _pendingInstancePolicies.end();)
    {
        if (now > it->second.createdAt && (now - it->second.createdAt) > PENDING_POLICY_TTL_SEC)
            it = _pendingInstancePolicies.erase(it);
        else
            ++it;
    }

    for (auto it = _pendingPlayerPolicies.begin(); it != _pendingPlayerPolicies.end();)
    {
        if (now > it->second.createdAt && (now - it->second.createdAt) > PENDING_POLICY_TTL_SEC)
            it = _pendingPlayerPolicies.erase(it);
        else
            ++it;
    }
}

size_t CoAContentScaling::GetPendingGroupPoliciesCount() const
{
    std::lock_guard<std::mutex> lock(_pendingPolicyLock);
    return _pendingInstancePolicies.size();
}

size_t CoAContentScaling::GetPendingPlayerPoliciesCount() const
{
    std::lock_guard<std::mutex> lock(_pendingPolicyLock);
    return _pendingPlayerPolicies.size();
}

std::vector<PendingInstanceScalePolicy> CoAContentScaling::GetAllPendingPolicies() const
{
    std::lock_guard<std::mutex> lock(_pendingPolicyLock);
    std::vector<PendingInstanceScalePolicy> result;
    result.reserve(_pendingInstancePolicies.size());
    for (auto const& pair : _pendingInstancePolicies)
    {
        result.push_back(pair.second);
    }
    return result;
}

void CoAContentScaling::OnInstanceMapCreated(InstanceMap* instanceMap, Player* player)
{
    if (!instanceMap)
        return;

    uint32 const mapId = instanceMap->GetId();
    uint32 const instanceId = instanceMap->GetInstanceId();

    ObjectGuid groupGuid;
    ObjectGuid playerGuid;

    if (player)
    {
        playerGuid = player->GetGUID();
        if (Group* group = player->GetGroup())
            groupGuid = group->GetGUID();
    }

    std::optional<PendingInstanceScalePolicy> policyOpt = ConsumePendingInstancePolicy(mapId, groupGuid, playerGuid);
    if (policyOpt)
    {
        sInstanceScalingMgr->SetCompositionMode(mapId, instanceId, policyOpt->compositionMode);

        if (policyOpt->challengeSize > 0)
        {
            sInstanceScalingMgr->SetChallengeSize(mapId, instanceId, policyOpt->challengeSize);
            LOG_INFO("module.coa_content_scaling",
                     "CoAContentScaling: Applied pending LFG challenge size {} to instance (mapId: {}, instanceId: {})",
                     policyOpt->challengeSize, mapId, instanceId);
        }
    }
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
            if (reload && sContentPackRegistry->IsFinalized())
            {
                LOG_WARN("module.coa_content_scaling",
                         "UniversalContentScaling: Progression layout, expansion packs and MaxPlayerLevel cannot be changed at runtime! Server restart required.");
                return;
            }

            sCoAContentScaling->LoadConfig();
        }

        void OnLoadCustomDatabaseTable() override
        {
            sCoAContentScaling->FinalizeAndInitialize();

            bool const enabled = sCoAContentScaling->IsEnabled();
            LocalLevelScaling::ContentScalingActive.store(enabled, std::memory_order_relaxed);

            if (enabled)
            {
                LocalLevelScaling::QuestBaseLevelOwner.store([](Quest const* quest) -> int32
                {
                    return sCoAContentScaling->GetEffectiveQuestLevel(quest);
                }, std::memory_order_relaxed);

                LocalLevelScaling::QuestMinLevelOwner.store([](Quest const* quest) -> uint32
                {
                    return sCoAContentScaling->GetEffectiveQuestMinLevel(quest);
                }, std::memory_order_relaxed);

                LocalLevelScaling::CreatureBaseLevelOwner.store([](CreatureTemplate const* cinfo, Creature const* creature) -> uint8
                {
                    return sCoAContentScaling->GetEffectiveCreatureLevel(cinfo, creature, cinfo ? cinfo->maxlevel : 1);
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

                // Progression belongs to this module while it runs: the core stands down over the
                // same three questions, so they are answered once, here.
                LocalLevelScaling::QuestMoneyMaxLevelOwner.store([](Quest const* quest, uint32 defaultMoney) -> uint32
                {
                    if (!quest || !sCoAContentScaling->IsEnabled())
                        return defaultMoney;

                    auto const& layout = sCoAContentScaling->GetLayout();
                    ContentEra const era = sContentPackRegistry->ResolveEraForQuest(
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
                    if (layout.maxLevel == 80 && layout.tbcEnabled && layout.wotlkEnabled)
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

                    ContentEra const era = sContentPackRegistry->ResolveEraForQuest(
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
                LocalLevelScaling::QuestBaseLevelOwner.store(nullptr, std::memory_order_relaxed);
                LocalLevelScaling::QuestMinLevelOwner.store(nullptr, std::memory_order_relaxed);
                LocalLevelScaling::CreatureBaseLevelOwner.store(nullptr, std::memory_order_relaxed);
                LocalLevelScaling::CreatureArmorOwner.store(nullptr, std::memory_order_relaxed);
                LocalLevelScaling::ItemEffectValueOwner.store(nullptr, std::memory_order_relaxed);
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

        void OnResolveLfgQueuePolicy(ObjectGuid const& guid, lfg::LfgQueuePolicy& policy) override
        {
            sCoAContentScaling->OnResolveLfgQueuePolicy(guid, policy);
        }

        void OnLfgProposalMadeGroup(lfg::LfgProposal const& proposal, Group* group) override
        {
            sCoAContentScaling->OnLfgProposalMadeGroup(proposal, group);
        }

        void OnInitializeLockedDungeons(Player* player, uint8& /*level*/, uint32& lockData, lfg::LFGDungeonData const* dungeon) override
        {
            if (!sCoAContentScaling->IsEnabled() || !player || !dungeon)
                return;

            auto const* lfgProf = FindGeneratedLfgProfile(dungeon->id);
            if (!lfgProf)
                return;

            // Check if era is disabled
            if ((lfgProf->era == ContentEra::TBC && !sCoAContentScaling->IsTbcEnabled()) ||
                (lfgProf->era == ContentEra::WotLK && !sCoAContentScaling->IsWotlkEnabled()))
            {
                lockData = lfg::LFG_LOCKSTATUS_INSUFFICIENT_EXPANSION;
                return;
            }

            // Compute effective min and max levels
            auto const& layout = sCoAContentScaling->GetLayout();
            uint8 const effMin = layout.MapAuthoredToEffective(lfgProf->era, lfgProf->authoredMin);
            uint8 const effMax = layout.MapAuthoredToEffective(lfgProf->era, lfgProf->authoredMax);
            uint8 const playerLevel = player->GetLevel();

            if (playerLevel < effMin)
            {
                lockData = lfg::LFG_LOCKSTATUS_TOO_LOW_LEVEL;
            }
            else if (playerLevel > effMax)
            {
                lockData = lfg::LFG_LOCKSTATUS_TOO_HIGH_LEVEL;
            }
            else
            {
                // If level requirement is satisfied under effective progression, clear level locks
                if (lockData == lfg::LFG_LOCKSTATUS_TOO_LOW_LEVEL || lockData == lfg::LFG_LOCKSTATUS_TOO_HIGH_LEVEL)
                    lockData = 0;
            }
        }

        void OnInstanceMapCreated(InstanceMap* instanceMap, Player* player) override
        {
            if (sCoAContentScaling->IsEnabled())
            {
                sCoAContentScaling->OnInstanceMapCreated(instanceMap, player);
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

        void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
        {
            if (!sCoAContentScaling->IsEnabled() || !sCoAContentScaling->IsGroupScalingEnabled() || damage == 0)
                return;

            if (!attacker || !attacker->IsCreature() || !attacker->GetMap() || !attacker->GetMap()->IsDungeon())
                return;

            InstanceScaleContext const ctx = sInstanceScalingMgr->GetOrCreateContext(attacker->GetMap());
            bool const isSolo = (ctx.effectivePlayers <= 1.0f);
            float const soloMitigation = sSoloAssistPolicy->GetDamageMitigationMultiplier(isSolo, true);

            damage = static_cast<uint32>(std::ceil(float(damage) * ctx.damageScale * soloMitigation));
        }

        void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
        {
            if (!sCoAContentScaling->IsEnabled() || !sCoAContentScaling->IsGroupScalingEnabled() || damage <= 0)
                return;

            if (!attacker || !attacker->IsCreature() || !attacker->GetMap() || !attacker->GetMap()->IsDungeon())
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

            damage = static_cast<int32>(std::ceil(float(damage) * ctx.damageScale * soloMitigation));
        }

        void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage, SpellInfo const* /*spellInfo*/) override
        {
            if (!sCoAContentScaling->IsEnabled() || !sCoAContentScaling->IsGroupScalingEnabled() || damage == 0)
                return;

            if (!attacker || !attacker->IsCreature() || !attacker->GetMap() || !attacker->GetMap()->IsDungeon())
                return;

            InstanceScaleContext const ctx = sInstanceScalingMgr->GetOrCreateContext(attacker->GetMap());
            damage = static_cast<uint32>(std::ceil(float(damage) * ctx.damageScale));
        }

        void ModifyHealReceived(Unit* target, Unit* healer, uint32& heal, SpellInfo const* /*spellInfo*/) override
        {
            if (!sCoAContentScaling->IsEnabled() || !sCoAContentScaling->IsGroupScalingEnabled() || heal == 0)
                return;

            if (!target || !target->IsCreature() || !target->GetMap() || !target->GetMap()->IsDungeon())
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

            if (!caster || !caster->IsCreature() || !caster->GetMap() || !caster->GetMap()->IsDungeon())
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

            ObjectGuid const castItemGuid = aura->GetCastItemGUID();
            if (!castItemGuid)
                return;

            Unit* owner = aura->GetCaster();
            Player* player = owner ? owner->ToPlayer() : nullptr;
            Item const* castItem = player ? player->GetItemByGuid(castItemGuid) : nullptr;
            if (!castItem)
                return;

            float multiplier = 1.0f;
            switch (effect->GetAuraType())
            {
                case SPELL_AURA_MOD_STAT:
                case SPELL_AURA_MOD_INCREASE_HEALTH:
                case SPELL_AURA_MOD_DAMAGE_DONE:
                case SPELL_AURA_MOD_HEALING_DONE:
                case SPELL_AURA_MOD_ATTACK_POWER:
                case SPELL_AURA_MOD_RANGED_ATTACK_POWER:
                case SPELL_AURA_SCHOOL_ABSORB:
                    multiplier = sItemBudgetScaler->GetStatMultiplier(castItem->GetEntry());
                    break;
                case SPELL_AURA_MOD_RATING:
                    multiplier = sItemBudgetScaler->GetRatingMultiplier(castItem->GetEntry());
                    break;
                default:
                    return;
            }

            if (multiplier >= 1.0f)
                return;

            int32 const scaled = static_cast<int32>(std::lround(float(amount) * multiplier));
            amount = amount > 0 ? std::max(1, scaled) : std::min(-1, scaled);
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

            ContentEra const era = sContentPackRegistry->ResolveEraForQuest(
                quest->GetQuestId(), quest->GetZoneOrSort(), 0, static_cast<uint8>(quest->GetQuestLevel()));

            if (!sCoAContentScaling->GetLayout().IsEraEnabled(era))
                return false;

            return true;
        }

        void OnResolveLfgRewardLevel(Player const* player, uint32 dungeonId, uint8& level) override
        {
            level = sCoAContentScaling->ResolveLfgRewardLevel(player, dungeonId, level);
        }

        void OnResolveDungeonAccessLevels(Player const* player, uint32 mapId, Difficulty difficulty,
            uint8& minLevel, uint8& maxLevel) override
        {
            if (!sCoAContentScaling->IsEnabled() || !player)
                return;

            if (IsRetrievingOwnCorpse(player, mapId))
            {
                minLevel = 0;
                maxLevel = 0;
                return;
            }

            // A profile for this difficulty if there is one, otherwise the map's own.
            auto const* accessProf = FindGeneratedAccessProfile(mapId, uint8(difficulty));
            if (!accessProf && difficulty != REGULAR_DIFFICULTY)
                accessProf = FindGeneratedAccessProfile(mapId);
            if (!accessProf)
                return;

            // Check if era is disabled
            if ((accessProf->era == ContentEra::TBC && !sCoAContentScaling->IsTbcEnabled()) ||
                (accessProf->era == ContentEra::WotLK && !sCoAContentScaling->IsWotlkEnabled()))
            {
                minLevel = 255; // Lock out
                return;
            }

            auto const& layout = sCoAContentScaling->GetLayout();
            if (minLevel > 0)
                minLevel = layout.MapAuthoredToEffective(accessProf->era, minLevel);
            if (maxLevel > 0)
                maxLevel = layout.MapAuthoredToEffective(accessProf->era, maxLevel);
        }

        void OnPlayerLogin(Player* player) override
        {
            sCoAContentScaling->LoadPlayerLfgSettings(player);
        }

        void OnPlayerLogout(Player* player) override
        {
            sCoAContentScaling->OnPlayerLogout(player);
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

            Map* map = lootOwner->GetMap();
            if (!map || !map->IsDungeon())
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

        void OnResolveEncounterMechanic(Map* map, uint32 encounterId, uint32 mechanicId, uint8 mechanicType, uint32 authoredValue, uint32& resolvedValue) override
        {
            if (!sCoAContentScaling->IsEnabled() || !sCoAContentScaling->IsAdaptiveMechanicsEnabled() || !map)
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
    AddCoAContentScalingCommands();
}
