/*
 * CoA Universal Content Scaling
 * InstanceScaleContext: Implementation of group scaling curves and authoritative encounter locking.
 */

#include "InstanceScaleContext.h"
#include "AdaptiveEncounterAPI.h"
#include "CoAContentScaling.h"
#include "ContentPackRegistry.h"
#include "DatabaseEnv.h"
#include "Field.h"
#include "InstanceProfile.h"
#include "Log.h"
#include "Map.h"
#include "Player.h"
#include "QueryResult.h"
#include <algorithm>

void InstanceScaleContext::CalculateMultipliers(float realRatio)
{
    float const ratio = std::clamp<float>(realRatio, 0.025f, 1.0f);

    // Health curve: near-linear, safe minimum 5% to prevent trivialized HP
    healthScale = std::max<float>(0.05f, std::pow(ratio, 0.95f));

    // Damage curve: sub-linear with 25% floor so encounter mechanics maintain bite
    damageScale = std::clamp<float>(0.25f + 0.75f * std::pow(ratio, 0.65f), 0.25f, 1.0f);

    // Healing curve: 20% floor
    healingScale = std::max<float>(0.20f, std::pow(ratio, 0.90f));

    // Absorb curve
    absorbScale = damageScale;
}

InstanceScalingMgr* InstanceScalingMgr::Instance()
{
    static InstanceScalingMgr instance;
    return &instance;
}

uint32 InstanceScalingMgr::CountActualPlayers(Map* map) const
{
    if (!map)
        return 1;

    uint32 count = 0;
    map->DoForAllPlayers([&count](Player* player)
    {
        if (!player || player->IsGameMaster())
            return;

        // Humans + playerbots count as physical participants
        ++count;
    });

    return std::max(1u, count);
}

float InstanceScalingMgr::CountEffectivePlayers(Map* map) const
{
    return float(CountActualPlayers(map));
}

InstanceScaleContext InstanceScalingMgr::GetOrCreateContext(Map* map)
{
    if (!map)
        return InstanceScaleContext{};

    uint32 const actualPlayers = CountActualPlayers(map);
    uint32 const mapId = map->GetId();
    uint32 const instanceId = map->GetInstanceId();
    uint64 const key = (static_cast<uint64>(mapId) << 32) | instanceId;

    std::lock_guard<std::mutex> lock(_lock);
    auto it = _contexts.find(key);
    if (it != _contexts.end())
    {
        // If combat is already locked, return frozen snapshot
        if (it->second.encounterLocked)
            return it->second;

        // If not in combat, update current participant count for display
        uint32 virtualChallenge = 0;
        auto cit = _challengeSizes.find(key);
        if (cit != _challengeSizes.end())
            virtualChallenge = cit->second;

        auto compIt = _compositionModes.find(key);
        if (compIt != _compositionModes.end())
            it->second.compositionMode = compIt->second;

        it->second.actualParticipants = actualPlayers;
        it->second.challengeSize = virtualChallenge;
        it->second.combatEffectivePlayers = (virtualChallenge > 0) ? float(virtualChallenge) : float(actualPlayers);
        it->second.mechanicParticipants = (virtualChallenge > 0) ? std::min<uint32>(actualPlayers, virtualChallenge) : actualPlayers;
        it->second.effectivePlayers = it->second.combatEffectivePlayers;

        it->second.CalculateMultipliers(it->second.combatEffectivePlayers / float(it->second.intendedPlayers));
        return it->second;
    }

    // New context
    InstanceScaleContext ctx;
    ctx.mapId = mapId;
    ctx.instanceId = instanceId;
    ctx.era = sContentPackRegistry->ResolveEraForMap(mapId);
    ctx.difficulty = map->GetDifficulty();
    ctx.intendedPlayers = sInstanceProfileRegistry->GetIntendedPlayers(mapId, ctx.difficulty);

    if (map->IsRaid())
    {
        ctx.tier = map->IsHeroic() ? ContentTier::RAID_END : ContentTier::RAID_MID;
    }
    else if (map->IsDungeon())
    {
        ctx.tier = map->IsHeroic() ? ContentTier::DUNGEON_HEROIC : ContentTier::DUNGEON_NORMAL;
    }
    else
    {
        ctx.tier = ContentTier::WORLD;
    }

    uint32 virtualChallenge = 0;
    auto cit = _challengeSizes.find(key);
    if (cit != _challengeSizes.end())
        virtualChallenge = cit->second;

    auto compIt = _compositionModes.find(key);
    if (compIt != _compositionModes.end())
        ctx.compositionMode = compIt->second;

    ctx.actualParticipants = actualPlayers;
    ctx.challengeSize = virtualChallenge;
    ctx.combatEffectivePlayers = (virtualChallenge > 0) ? float(virtualChallenge) : float(actualPlayers);
    ctx.mechanicParticipants = (virtualChallenge > 0) ? std::min<uint32>(actualPlayers, virtualChallenge) : actualPlayers;
    ctx.effectivePlayers = ctx.combatEffectivePlayers;

    ctx.CalculateMultipliers(ctx.combatEffectivePlayers / float(ctx.intendedPlayers));

    _contexts[key] = ctx;
    return ctx;
}

InstanceScaleContext InstanceScalingMgr::GetContext(uint32 mapId, uint32 instanceId)
{
    uint64 const key = (static_cast<uint64>(mapId) << 32) | instanceId;
    std::lock_guard<std::mutex> lock(_lock);
    auto it = _contexts.find(key);
    if (it != _contexts.end())
        return it->second;
    return InstanceScaleContext{};
}

EncounterScaleSnapshot InstanceScalingMgr::LockEncounterContext(Map* map, EncounterLifecycleSource source, EncounterKey key, EncounterHealthTransferPolicy hpPolicy)
{
    uint32 const actualPlayers = CountActualPlayers(map);
    uint32 const mapId = map->GetId();
    uint32 const instanceId = map->GetInstanceId();
    uint64 const mkey = (static_cast<uint64>(mapId) << 32) | instanceId;

    std::lock_guard<std::mutex> lock(_lock);
    InstanceScaleContext& ctx = _contexts[mkey];

    uint32 virtualChallenge = 0;
    auto cit = _challengeSizes.find(mkey);
    if (cit != _challengeSizes.end())
        virtualChallenge = cit->second;

    auto compIt = _compositionModes.find(mkey);
    if (compIt != _compositionModes.end())
        ctx.compositionMode = compIt->second;

    ctx.actualParticipants = actualPlayers;
    ctx.challengeSize = virtualChallenge;
    ctx.combatEffectivePlayers = (virtualChallenge > 0) ? float(virtualChallenge) : float(actualPlayers);
    ctx.mechanicParticipants = (virtualChallenge > 0) ? std::min<uint32>(actualPlayers, virtualChallenge) : actualPlayers;
    ctx.effectivePlayers = ctx.combatEffectivePlayers;

    if (ctx.intendedPlayers == 0)
        ctx.intendedPlayers = sInstanceProfileRegistry->GetIntendedPlayers(mapId, map->GetDifficulty());

    ctx.CalculateMultipliers(ctx.combatEffectivePlayers / float(ctx.intendedPlayers));

    ctx.lockState = EncounterLockState::ACTIVE;
    ctx.encounterLocked = true;
    ctx.lockEncounterId = key.id;
    ctx.activeEncounterSource = source;
    ctx.activeEncounterKey = key;
    ctx.hpPolicy = hpPolicy;

    EncounterScaleSnapshot snapshot;
    snapshot.actualParticipants = ctx.actualParticipants;
    snapshot.combatEffectivePlayers = ctx.combatEffectivePlayers;
    snapshot.mechanicParticipants = ctx.mechanicParticipants;
    snapshot.intendedPlayers = ctx.intendedPlayers;
    snapshot.challengeSize = ctx.challengeSize;
    snapshot.compositionMode = ctx.compositionMode;
    snapshot.isPhysicallySolo = (ctx.actualParticipants <= 1);
    snapshot.isMechanicSolo = (ctx.mechanicParticipants <= 1);

    snapshot.healthScale = ctx.healthScale;
    snapshot.damageScale = ctx.damageScale;
    snapshot.healingScale = ctx.healingScale;
    snapshot.absorbScale = ctx.absorbScale;
    snapshot.generation = ++ctx.snapshotGeneration;
    snapshot.valid = true;
    snapshot.effectivePlayers = ctx.combatEffectivePlayers;

    ctx.activeSnapshot = snapshot;

    LOG_INFO("server.loading", "UniversalContentScaling: Encounter {}:{} locked on map {} (instance {}) [gen {}]: {} actual, {} combat eff, {} mechanic parts (intended {}) -> HP x{:.2f}, Dmg x{:.2f}",
             uint32(key.type), key.id, mapId, instanceId, snapshot.generation, snapshot.actualParticipants, snapshot.combatEffectivePlayers, snapshot.mechanicParticipants, snapshot.intendedPlayers, snapshot.healthScale, snapshot.damageScale);

    return snapshot;
}

void InstanceScalingMgr::ApplySnapshotToBoss(Creature* boss, EncounterScaleSnapshot const& snapshot, EncounterHealthTransferPolicy hpPolicy)
{
    if (!boss || !boss->GetMap())
        return;

    uint32 const mapId = boss->GetMap()->GetId();
    uint32 const instanceId = boss->GetMap()->GetInstanceId();
    uint64 const mkey = (static_cast<uint64>(mapId) << 32) | instanceId;

    {
        std::lock_guard<std::mutex> lock(_lock);
        auto it = _contexts.find(mkey);
        if (it != _contexts.end())
        {
            auto git = it->second.bossAppliedGenerations.find(boss->GetGUID());
            if (git != it->second.bossAppliedGenerations.end() && git->second == snapshot.generation)
            {
                // Already applied authoritative pull stats for this generation
                return;
            }
            it->second.bossAppliedGenerations[boss->GetGUID()] = snapshot.generation;
        }
    }

    sCoAContentScaling->RecalculateEncounterCombatStats(boss, snapshot, hpPolicy);
}

void InstanceScalingMgr::BeginEncounter(Map* map, EncounterLifecycleSource source, EncounterKey key, Creature* boss,
                                       EncounterHealthTransferPolicy hpPolicy)
{
    if (!map || !map->IsDungeon())
        return;

    uint32 const mapId = map->GetId();
    uint32 const instanceId = map->GetInstanceId();
    uint64 const mkey = (static_cast<uint64>(mapId) << 32) | instanceId;

    EncounterScaleSnapshot snapshot;
    bool needApplyBoss = false;

    {
        std::lock_guard<std::mutex> lock(_lock);
        auto it = _contexts.find(mkey);
        if (it != _contexts.end() && it->second.encounterLocked)
        {
            InstanceScaleContext& ctx = it->second;

            // If an authoritative source is already active
            if (ctx.activeEncounterSource <= source)
            {
                if (boss)
                {
                    ctx.bossGuids.insert(boss->GetGUID());
                    snapshot = ctx.activeSnapshot;
                    needApplyBoss = true;
                }
                // Do NOT early return here: drop out of mutex and call ApplySnapshotToBoss if boss was provided!
            }
            else
            {
                // Upgrade from lower priority (e.g. creature fallback) to higher priority (instance script)
                ctx.activeEncounterSource = source;
                ctx.activeEncounterKey = key;
                if (boss)
                {
                    ctx.bossGuids.insert(boss->GetGUID());
                    snapshot = ctx.activeSnapshot;
                    needApplyBoss = true;
                }
            }
        }
    }

    if (needApplyBoss && boss)
    {
        ApplySnapshotToBoss(boss, snapshot, hpPolicy);
        return;
    }

    // New encounter lock
    snapshot = LockEncounterContext(map, source, key, hpPolicy);

    if (boss)
    {
        {
            std::lock_guard<std::mutex> lock(_lock);
            _contexts[mkey].bossGuids.insert(boss->GetGUID());
        }
        ApplySnapshotToBoss(boss, snapshot, hpPolicy);
    }
}

void InstanceScalingMgr::EndEncounter(Map* map, EncounterLifecycleSource source, EncounterKey key, EncounterLockState endState,
                                     ObjectGuid endingGuid)
{
    if (!map || !map->IsDungeon())
        return;

    uint32 const mapId = map->GetId();
    uint32 const instanceId = map->GetInstanceId();
    uint64 const mkey = (static_cast<uint64>(mapId) << 32) | instanceId;

    std::lock_guard<std::mutex> lock(_lock);
    auto it = _contexts.find(mkey);
    if (it == _contexts.end() || !it->second.encounterLocked)
        return;

    InstanceScaleContext& ctx = it->second;

    // Check ownership
    if (ctx.activeEncounterSource == EncounterLifecycleSource::INSTANCE_SCRIPT)
    {
        // Only InstanceScript callbacks (DONE, FAIL, reset) can unlock an InstanceScript encounter!
        if (source != EncounterLifecycleSource::INSTANCE_SCRIPT)
        {
            LOG_DEBUG("server.loading", "UniversalContentScaling: Ignored non-InstanceScript encounter end request on map {} while under InstanceScript lock.", mapId);
            return;
        }

        if (key != ctx.activeEncounterKey)
        {
            LOG_DEBUG("server.loading", "UniversalContentScaling: Encounter key mismatch on end (active {}:{}, incoming {}:{}).",
                      uint32(ctx.activeEncounterKey.type), ctx.activeEncounterKey.id, uint32(key.type), key.id);
            return;
        }

        ctx.lockState = endState;
        ctx.encounterLocked = false;
        ctx.lockEncounterId = 0;
        ctx.bossGuids.clear();
        ctx.bossAppliedGenerations.clear();
        ctx.activeSnapshot.valid = false;
        LOG_INFO("server.loading", "UniversalContentScaling: Encounter {}:{} unlocked via InstanceScript on map {} (instance {})",
                 uint32(key.type), key.id, mapId, instanceId);
        return;
    }

    if (ctx.activeEncounterSource == EncounterLifecycleSource::CREATURE_FALLBACK)
    {
        if (source != EncounterLifecycleSource::CREATURE_FALLBACK)
            return;

        if (!endingGuid.IsEmpty())
        {
            ctx.bossGuids.erase(endingGuid);

            // Council check: are other council bosses still alive and in combat?
            bool anyAliveInCombat = false;
            for (ObjectGuid const& bguid : ctx.bossGuids)
            {
                if (Creature* c = map->GetCreature(bguid))
                {
                    if (c->IsAlive() && c->IsInCombat())
                    {
                        anyAliveInCombat = true;
                        break;
                    }
                }
            }

            if (anyAliveInCombat)
            {
                LOG_DEBUG("server.loading", "UniversalContentScaling: Council member finished combat, but others remain active on map {}.", mapId);
                return;
            }
        }

        ctx.lockState = endState;
        ctx.encounterLocked = false;
        ctx.lockEncounterId = 0;
        ctx.bossGuids.clear();
        ctx.bossAppliedGenerations.clear();
        ctx.activeSnapshot.valid = false;
        LOG_INFO("server.loading", "UniversalContentScaling: Creature fallback encounter {}:{} unlocked on map {} (instance {})",
                 uint32(key.type), key.id, mapId, instanceId);
    }
}

void InstanceScalingMgr::OnEncounterStart(Map* map, Creature* boss, uint32 encounterId)
{
    EncounterLifecycleSource const src = boss ? EncounterLifecycleSource::CREATURE_FALLBACK : EncounterLifecycleSource::INSTANCE_SCRIPT;
    EncounterKeyType const ktype = boss ? EncounterKeyType::CREATURE_ENTRY : EncounterKeyType::INSTANCE_ENCOUNTER;
    BeginEncounter(map, src, { ktype, encounterId }, boss);
}

void InstanceScalingMgr::OnEncounterEnd(Map* map, uint32 encounterId)
{
    EndEncounter(map, EncounterLifecycleSource::INSTANCE_SCRIPT, { EncounterKeyType::INSTANCE_ENCOUNTER, encounterId }, EncounterLockState::COMPLETED);
}

void InstanceScalingMgr::SetChallengeSize(uint32 mapId, uint32 instanceId, uint32 virtualSize)
{
    uint64 const key = (static_cast<uint64>(mapId) << 32) | instanceId;
    std::lock_guard<std::mutex> lock(_lock);
    if (virtualSize == 0)
        _challengeSizes.erase(key);
    else
        _challengeSizes[key] = virtualSize;
}

uint32 InstanceScalingMgr::GetChallengeSize(uint32 mapId, uint32 instanceId) const
{
    uint64 const key = (static_cast<uint64>(mapId) << 32) | instanceId;
    std::lock_guard<std::mutex> lock(_lock);
    auto it = _challengeSizes.find(key);
    if (it != _challengeSizes.end())
        return it->second;
    return 0;
}

void InstanceScalingMgr::SetCompositionMode(uint32 mapId, uint32 instanceId, lfg::LfgCompositionMode mode)
{
    uint64 const key = (static_cast<uint64>(mapId) << 32) | instanceId;
    std::lock_guard<std::mutex> lock(_lock);
    _compositionModes[key] = mode;
}

lfg::LfgCompositionMode InstanceScalingMgr::GetCompositionMode(uint32 mapId, uint32 instanceId) const
{
    uint64 const key = (static_cast<uint64>(mapId) << 32) | instanceId;
    std::lock_guard<std::mutex> lock(_lock);
    auto it = _compositionModes.find(key);
    if (it != _compositionModes.end())
        return it->second;
    return lfg::LfgCompositionMode::MATCHMAKING;
}

EncounterContext InstanceScalingMgr::BuildEncounterContext(Map* map, uint32 encounterId)
{
    if (!map)
        return EncounterContext{};

    uint32 const mapId = map->GetId();
    uint32 const instanceId = map->GetInstanceId();
    uint64 const key = (static_cast<uint64>(mapId) << 32) | instanceId;

    std::lock_guard<std::mutex> lock(_lock);
    auto it = _contexts.find(key);
    if (it != _contexts.end() && it->second.encounterLocked && it->second.activeSnapshot.valid)
    {
        EncounterScaleSnapshot const& snap = it->second.activeSnapshot;
        EncounterContext ctx;
        ctx.mapId = mapId;
        ctx.instanceId = instanceId;
        ctx.encounterId = encounterId;
        ctx.actualParticipants = snap.actualParticipants;
        ctx.combatEffectivePlayers = snap.combatEffectivePlayers;
        ctx.mechanicParticipants = snap.mechanicParticipants;
        ctx.intendedPlayers = snap.intendedPlayers;
        ctx.challengeSize = snap.challengeSize;
        ctx.difficulty = map->GetDifficulty();
        ctx.compositionMode = snap.compositionMode;
        ctx.isPhysicallySolo = snap.isPhysicallySolo;
        ctx.isMechanicSolo = snap.isMechanicSolo;
        ctx.snapshotGeneration = snap.generation;
        return ctx;
    }

    // If not locked in encounter, construct dynamically from current state
    uint32 const actualPlayers = CountActualPlayers(map);
    uint32 virtualChallenge = 0;
    auto cit = _challengeSizes.find(key);
    if (cit != _challengeSizes.end())
        virtualChallenge = cit->second;

    lfg::LfgCompositionMode compMode = lfg::LfgCompositionMode::MATCHMAKING;
    auto compIt = _compositionModes.find(key);
    if (compIt != _compositionModes.end())
        compMode = compIt->second;

    uint32 const intended = sInstanceProfileRegistry->GetIntendedPlayers(mapId, map->GetDifficulty());

    EncounterContext ctx;
    ctx.mapId = mapId;
    ctx.instanceId = instanceId;
    ctx.encounterId = encounterId;
    ctx.actualParticipants = actualPlayers;
    ctx.combatEffectivePlayers = (virtualChallenge > 0) ? float(virtualChallenge) : float(actualPlayers);
    ctx.mechanicParticipants = (virtualChallenge > 0) ? std::min<uint32>(actualPlayers, virtualChallenge) : actualPlayers;
    ctx.intendedPlayers = intended;
    ctx.challengeSize = virtualChallenge;
    ctx.difficulty = map->GetDifficulty();
    ctx.compositionMode = compMode;
    ctx.isPhysicallySolo = (actualPlayers <= 1);
    ctx.isMechanicSolo = (ctx.mechanicParticipants <= 1);
    ctx.snapshotGeneration = (it != _contexts.end()) ? it->second.snapshotGeneration : 0;
    return ctx;
}

bool InstanceScalingMgr::HasCalibratedBossHp(uint32 entry, uint8 difficulty) const
{
    if (difficulty >= MAX_RAID_DIFFICULTY)
        return false;

    std::lock_guard<std::mutex> lock(_lock);
    auto it = _bossFlexCache.find(entry);
    if (it == _bossFlexCache.end())
        return false;

    return it->second[difficulty] > 0;
}

uint32 InstanceScalingMgr::GetCalibratedBossHp(uint32 entry, uint8 difficulty, float effectivePlayers) const
{
    if (difficulty >= MAX_RAID_DIFFICULTY)
        return 0;

    std::lock_guard<std::mutex> lock(_lock);
    auto it = _bossFlexCache.find(entry);
    if (it == _bossFlexCache.end())
        return 0;

    uint32 const perPlayerHp = it->second[difficulty];
    if (perPlayerHp == 0)
        return 0;

    return static_cast<uint32>(std::round(float(perPlayerHp) * effectivePlayers));
}

void InstanceScalingMgr::LoadCalibratedBossFlex()
{
    std::lock_guard<std::mutex> lock(_lock);
    _bossFlexCache.clear();

    // coa_boss_flex belongs to this realm's raid difficulty module and keeps one row per boss, with
    // the difficulty in the column name: hp_d0 Normal, hp_d1 Heroic, hp_d2 Mythic, hp_d3 Ascended.
    // Upstream reads a row per (entry, difficulty) with a per_player_health column, which is the shape
    // on the author's fork - asking for a difficulty column here aborts the worldserver at startup.
    QueryResult result = WorldDatabase.Query("SELECT entry, hp_d0, hp_d1, hp_d2, hp_d3 FROM coa_boss_flex");
    if (!result)
        return;

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();
        uint32 const entry = fields[0].Get<uint32>();

        for (uint8 diff = 0; diff < 4 && diff < MAX_RAID_DIFFICULTY; ++diff)
        {
            uint32 const perPlayerHp = fields[1 + diff].Get<uint32>();
            if (!perPlayerHp)
                continue;

            _bossFlexCache[entry][diff] = perPlayerHp;
            ++count;
        }
    } while (result->NextRow());

    LOG_INFO("server.loading", "UniversalContentScaling: Loaded {} calibrated boss flex profiles from coa_boss_flex", count);
}

void InstanceScalingMgr::Clear()
{
    std::lock_guard<std::mutex> lock(_lock);
    _contexts.clear();
    _challengeSizes.clear();
    _compositionModes.clear();
    _bossFlexCache.clear();
}
