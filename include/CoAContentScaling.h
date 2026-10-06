/*
 * CoA Universal Content Scaling
 * CoAContentScaling: Master controller and lifecycle manager.
 */

#ifndef COA_CONTENT_SCALING_H
#define COA_CONTENT_SCALING_H

#include "CombatBudgetProfile.h"
#include "ContentEra.h"
#include "ContentTier.h"
#include "Define.h"
#include "DungeonFinding/LFG.h"
#include "InstanceScaleContext.h"
#include "ObjectGuid.h"
#include "ProgressionLayout.h"
#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

class Creature;
class Player;
class Unit;
class Quest;
class Map;
class InstanceMap;
class Group;
struct CreatureTemplate;

namespace lfg
{
    struct LfgProposal;
}

struct PlayerLfgSettings
{
    lfg::LfgCompositionMode compositionMode{lfg::LfgCompositionMode::MATCHMAKING};
    uint32 challengeSize{0}; // 0 = Adaptive, 1 = Solo, 2..40 = fixed
};

struct PendingInstanceScalePolicy
{
    uint32 mapId{0};
    ObjectGuid groupGuid;
    uint32 challengeSize{0};
    lfg::LfgCompositionMode compositionMode{lfg::LfgCompositionMode::MATCHMAKING};
    uint64 generation{0};
    uint32 createdAt{0}; // GameTime::GetGameTime()
};

class CoAContentScaling
{
public:
    static CoAContentScaling* Instance();

    void LoadConfig();
    // The settings that only tune numbers, read again on .reload config.
    void LoadTuning();
    void FinalizeAndInitialize();
    void InitializeLayout();

    // Rewrites the item templates, once the world has loaded them.
    void ScaleItems();

    // Instance item level requirements, read in the item levels the rewritten templates carry.
    void ScaleAccessRequirements();

    [[nodiscard]] bool IsEnabled() const { return _enabled; }
    [[nodiscard]] bool IsGroupScalingEnabled() const { return _groupScalingEnabled; }
    [[nodiscard]] float GetDamageMultiplier() const { return _damageMultiplier.load(std::memory_order_relaxed); }
    [[nodiscard]] bool IsWorldLeechEnabled() const { return _worldLeechEnabled.load(std::memory_order_relaxed); }
    [[nodiscard]] float GetWorldLeechPercent() const { return _worldLeechPercent.load(std::memory_order_relaxed); }
    [[nodiscard]] bool IsAdaptiveMechanicsEnabled() const { return _adaptiveMechanicsEnabled; }
    [[nodiscard]] bool IsDebugEnabled() const { return _debug; }

    // Maps played at their authored levels: no creature, access, loot or item from them is scaled.
    // Read once at startup, like the layout, because the items cut at startup cannot follow a reload.
    [[nodiscard]] bool IsAuthenticMap(uint32 mapId) const { return _authenticMaps.count(mapId) != 0; }
    [[nodiscard]] bool IsAuthenticMap(Map const* map) const;
    [[nodiscard]] std::unordered_set<uint32> const& GetAuthenticMaps() const { return _authenticMaps; }

    [[nodiscard]] uint64 GetCreatureHookCalls() const { return _creatureHookCalls.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64 GetCreatureScaleApplied() const { return _creatureScaleApplied.load(std::memory_order_relaxed); }
    [[nodiscard]] uint32 GetLastScaledEntry() const { return _lastScaledEntry.load(std::memory_order_relaxed); }
    [[nodiscard]] uint32 GetLastScaledHealth() const { return _lastScaledHealth.load(std::memory_order_relaxed); }

    void SetEnabled(bool enabled) { _enabled = enabled; }
    void SetAdaptiveMechanicsEnabled(bool enabled) { _adaptiveMechanicsEnabled = enabled; }

    [[nodiscard]] ProgressionLayout const& GetLayout() const { return _layout; }

    // Effective Level Mapping
    [[nodiscard]] uint8 GetEffectiveCreatureLevel(CreatureTemplate const* cinfo, Creature const* creature, uint8 authoredLevel) const;
    [[nodiscard]] int32 GetEffectiveQuestLevel(Quest const* quest) const;
    [[nodiscard]] uint32 GetEffectiveQuestMinLevel(Quest const* quest) const;
    [[nodiscard]] uint8 GetEffectiveAbilityRequiredLevel(uint8 authoredLevel) const;
    [[nodiscard]] uint8 GetEffectiveAreaContentLevel(uint32 areaId, uint32 mapId, uint8 authoredLevel) const;

    // One source of truth for what a creature is worth: the same context, group scale and
    // calibrated boss profile the spawn path applies, so a report cannot drift from reality.
    [[nodiscard]] CalculatedCombatBudget CalculateCreatureBudget(CreatureTemplate const* cinfo, Creature* creature,
                                                                CreatureScaleContext* outContext = nullptr) const;
    [[nodiscard]] float GetEffectiveCreatureArmor(CreatureTemplate const* cinfo, Creature const* creature,
                                                  float generatedArmor) const;

    // Combat scaling calculations
    void ApplyCreatureScaling(CreatureTemplate const* cinfo, Creature* creature);

    // Gives every creature on a dungeon map its budget again, for the group that is there now.
    // Returns how many were rescaled. Creatures in combat are left alone.
    uint32 RescaleDungeonCreatures(Map* map);
    void CountCreatureHookCall() { _creatureHookCalls.fetch_add(1, std::memory_order_relaxed); }
    void RecalculateEncounterCombatStats(Creature* boss, EncounterScaleSnapshot const& snapshot,
                                         EncounterHealthTransferPolicy hpPolicy = EncounterHealthTransferPolicy::FULL_ON_PULL);

    // Expansion enablement
    void SetTbcEnabled(bool enabled) { _tbcEnabled = enabled; }
    void SetWotlkEnabled(bool enabled) { _wotlkEnabled = enabled; }
    [[nodiscard]] bool IsTbcEnabled() const { return _tbcEnabled; }
    [[nodiscard]] bool IsWotlkEnabled() const { return _wotlkEnabled; }

    // Map access validation
    [[nodiscard]] bool CanPlayerEnterMap(Player const* player, uint32 mapId, uint8 difficulty = 0) const;

    // Which reward bracket a finished random dungeon pays from, for the content rather than for
    // the number on the character.
    [[nodiscard]] uint8 ResolveLfgRewardLevel(Player const* player, uint32 dungeonId, uint8 playerLevel) const;

    // LFG Policy & Composition Integration
    void SetPlayerLfgMode(ObjectGuid guid, lfg::LfgCompositionMode mode);
    [[nodiscard]] lfg::LfgCompositionMode GetPlayerLfgMode(ObjectGuid guid) const;
    bool SetPlayerLfgChallenge(ObjectGuid guid, uint32 challengeSize);
    [[nodiscard]] uint32 GetPlayerLfgChallenge(ObjectGuid guid) const;
    void LoadPlayerLfgSettings(Player* player);
    void SavePlayerLfgSettings(Player* player);
    void OnPlayerLogout(Player* player);

    void OnResolveLfgQueuePolicy(ObjectGuid const& guid, lfg::LfgQueuePolicy& policy);
    void OnLfgProposalMadeGroup(lfg::LfgProposal const& proposal, Group* group);
    void OnInstanceMapCreated(InstanceMap* instanceMap, Player* player);

    // Pending Policy Lifecycle & Diagnostics
    std::optional<PendingInstanceScalePolicy> ConsumePendingInstancePolicy(uint32 mapId, ObjectGuid groupGuid, ObjectGuid playerGuid);
    void PurgeExpiredPendingPolicies();
    [[nodiscard]] size_t GetPendingGroupPoliciesCount() const;
    [[nodiscard]] size_t GetPendingPlayerPoliciesCount() const;
    std::vector<PendingInstanceScalePolicy> GetAllPendingPolicies() const;

private:
    CoAContentScaling() = default;

    [[nodiscard]] std::unordered_set<uint32> CollectAuthenticItems() const;

    bool _enabled{false};
    bool _tbcEnabled{false};
    bool _wotlkEnabled{false};

    bool _groupScalingEnabled{true};
    bool _lockOnEncounterStart{true};
    bool _adaptiveMechanicsEnabled{true};
    bool _scaleLootCount{true};
    std::atomic<float> _damageMultiplier{1.0f};
    std::atomic<bool> _worldLeechEnabled{false};
    std::atomic<float> _worldLeechPercent{5.0f};
    bool _allowSoloRaids{true};
    bool _debug{false};

    std::atomic<uint64> _creatureHookCalls{0};
    std::atomic<uint64> _creatureScaleApplied{0};
    std::atomic<uint32> _lastScaledEntry{0};
    std::atomic<uint32> _lastScaledHealth{0};

    std::string _progressionMode{"Auto"};
    uint8 _customClassicEnd{0};
    uint8 _customTbcEnd{0};

    ProgressionLayout _layout;
    std::unordered_set<uint32> _authenticMaps;

    lfg::LfgCompositionMode _defaultLfgCompositionMode{lfg::LfgCompositionMode::MATCHMAKING};
    uint32 _defaultLfgChallengeSize{0};

    mutable std::mutex _lfgSettingsLock;
    std::unordered_map<ObjectGuid, PlayerLfgSettings> _playerLfgSettings;

    mutable std::mutex _pendingPolicyLock;
    uint64 _pendingPolicyGeneration{0};
    std::unordered_map<ObjectGuid, PendingInstanceScalePolicy> _pendingInstancePolicies;
    std::unordered_map<ObjectGuid, PendingInstanceScalePolicy> _pendingPlayerPolicies;
};

#define sCoAContentScaling CoAContentScaling::Instance()

#endif // COA_CONTENT_SCALING_H
