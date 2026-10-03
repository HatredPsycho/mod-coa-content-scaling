/*
 * CoA Universal Content Scaling
 * InstanceScaleContext: Group and instance scaling context with authoritative encounter snapshot locking.
 */

#ifndef COA_INSTANCE_SCALE_CONTEXT_H
#define COA_INSTANCE_SCALE_CONTEXT_H

#include "AdaptiveEncounterAPI.h"
#include "ContentEra.h"
#include "ContentTier.h"
#include "DBCEnums.h"
#include "Define.h"
#include "LFG.h"
#include "ObjectGuid.h"
#include <cmath>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

class Map;
class Player;
class Unit;
class Creature;

enum class EncounterLockState : uint8
{
    IDLE      = 0,
    ACTIVE    = 1,
    COMPLETED = 2,
    RESETTING = 3
};

enum class EncounterLifecycleSource : uint8
{
    INSTANCE_SCRIPT    = 0,
    REGISTERED_ADAPTER = 1,
    CREATURE_FALLBACK  = 2
};

enum class EncounterKeyType : uint8
{
    INSTANCE_ENCOUNTER = 0,
    CREATURE_ENTRY     = 1,
    CUSTOM_ADAPTER     = 2
};

struct EncounterKey
{
    EncounterKeyType type{EncounterKeyType::INSTANCE_ENCOUNTER};
    uint32 id{0};

    bool operator==(EncounterKey const& other) const = default;
};

template <>
struct std::hash<EncounterKey>
{
    std::size_t operator()(EncounterKey const& key) const noexcept
    {
        return (static_cast<std::size_t>(key.type) << 32) | key.id;
    }
};

enum class EncounterHealthTransferPolicy : uint8
{
    FULL_ON_PULL      = 0,
    PRESERVE_PERCENT  = 1,
    PRESERVE_ABSOLUTE = 2
};

struct EncounterScaleSnapshot
{
    uint32 actualParticipants{1};
    float combatEffectivePlayers{1.0f};
    uint32 mechanicParticipants{1};
    uint32 intendedPlayers{5};
    uint32 challengeSize{0};
    lfg::LfgCompositionMode compositionMode{lfg::LfgCompositionMode::MATCHMAKING};

    float healthScale{1.0f};
    float damageScale{1.0f};
    float healingScale{1.0f};
    float absorbScale{1.0f};

    uint64 generation{0};
    bool valid{false};

    bool isPhysicallySolo{true};
    bool isMechanicSolo{true};

    // Backwards-compatibility alias
    float effectivePlayers{1.0f};
};

struct InstanceScaleContext
{
    uint32 mapId{0};
    uint32 instanceId{0};

    ContentEra era{ContentEra::Classic};
    ContentTier tier{ContentTier::DUNGEON_NORMAL};

    uint32 actualParticipants{1};
    float combatEffectivePlayers{5.0f};
    uint32 mechanicParticipants{1};
    uint32 intendedPlayers{5};
    uint32 challengeSize{0};
    lfg::LfgCompositionMode compositionMode{lfg::LfgCompositionMode::MATCHMAKING};

    Difficulty difficulty{DUNGEON_DIFFICULTY_NORMAL};

    float healthScale{1.0f};
    float damageScale{1.0f};
    float healingScale{1.0f};
    float absorbScale{1.0f};

    EncounterLockState lockState{EncounterLockState::IDLE};
    bool encounterLocked{false};
    uint32 lockEncounterId{0};

    EncounterKey activeEncounterKey{EncounterKeyType::INSTANCE_ENCOUNTER, 0};
    EncounterLifecycleSource activeEncounterSource{EncounterLifecycleSource::CREATURE_FALLBACK};
    uint64 snapshotGeneration{0};
    EncounterScaleSnapshot activeSnapshot;
    std::unordered_set<ObjectGuid> bossGuids;
    std::unordered_map<ObjectGuid, uint64> bossAppliedGenerations;
    EncounterHealthTransferPolicy hpPolicy{EncounterHealthTransferPolicy::FULL_ON_PULL};

    // Backwards-compatibility alias
    float effectivePlayers{5.0f};

    void CalculateMultipliers(float realRatio);
};

class InstanceScalingMgr
{
public:
    static InstanceScalingMgr* Instance();

    // Context management per instance map
    InstanceScaleContext GetOrCreateContext(Map* map);
    InstanceScaleContext GetContext(uint32 mapId, uint32 instanceId);
    void RemoveMapContext(uint32 mapId, uint32 instanceId);

    // Count participants in map (actual physical human + bot players)
    uint32 CountActualPlayers(Map* map) const;
    float CountEffectivePlayers(Map* map) const;

    // Authoritative Encounter Lifecycle
    EncounterScaleSnapshot LockEncounterContext(Map* map, EncounterLifecycleSource source, EncounterKey key, EncounterHealthTransferPolicy hpPolicy);
    void ApplySnapshotToBoss(Creature* boss, EncounterScaleSnapshot const& snapshot, EncounterHealthTransferPolicy hpPolicy);

    void BeginEncounter(Map* map, EncounterLifecycleSource source, EncounterKey key, Creature* boss = nullptr,
                        EncounterHealthTransferPolicy hpPolicy = EncounterHealthTransferPolicy::FULL_ON_PULL);
    void EndEncounter(Map* map, EncounterLifecycleSource source, EncounterKey key, EncounterLockState endState,
                      ObjectGuid endingGuid = ObjectGuid::Empty);

    // Legacy / Convenience wrappers
    void OnEncounterStart(Map* map, Creature* boss, uint32 encounterId);
    void OnEncounterEnd(Map* map, uint32 encounterId);

    // Challenge mode setting (0 = Adaptive, 1 = Solo, 2 = 2-player, 5 = 5-player, etc.)
    void SetChallengeSize(uint32 mapId, uint32 instanceId, uint32 virtualSize);
    uint32 GetChallengeSize(uint32 mapId, uint32 instanceId) const;

    // Composition mode setting (MATCHMAKING, BOT_FILL, CURRENT_PARTY)
    void SetCompositionMode(uint32 mapId, uint32 instanceId, lfg::LfgCompositionMode mode);
    lfg::LfgCompositionMode GetCompositionMode(uint32 mapId, uint32 instanceId) const;

    // Build EncounterContext for AdaptiveEncounterMgr queries
    EncounterContext BuildEncounterContext(Map* map, uint32 encounterId);

    // Boss flex health integration (priority: explicit encounter override > coa_boss_flex > generic instance scaling)
    uint32 GetCalibratedBossHp(uint32 entry, uint8 difficulty, float effectivePlayers) const;
    bool HasCalibratedBossHp(uint32 entry, uint8 difficulty) const;
    void LoadCalibratedBossFlex();

    void Clear();

private:
    InstanceScalingMgr() = default;

    mutable std::mutex _lock;
    // key: (mapId << 32) | instanceId
    std::unordered_map<uint64, InstanceScaleContext> _contexts;
    std::unordered_map<uint64, uint32> _challengeSizes;
    std::unordered_map<uint64, lfg::LfgCompositionMode> _compositionModes;

    // coa_boss_flex cache: entry -> perPlayer[4]
    std::unordered_map<uint32, std::array<uint32, MAX_RAID_DIFFICULTY>> _bossFlexCache;
};

#define sInstanceScalingMgr InstanceScalingMgr::Instance()

#endif // COA_INSTANCE_SCALE_CONTEXT_H
