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
#include "InstanceScaleContext.h"
#include "ObjectGuid.h"
#include "ProgressionLayout.h"
#include <atomic>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

class Creature;
class Player;
class Unit;
class Quest;
class Map;
class InstanceMap;
struct CreatureTemplate;

class CoAContentScaling
{
public:
    static CoAContentScaling* Instance();

    void LoadConfig();
    // The settings that only tune numbers, read again on .reload config.
    void LoadTuning();
    static journey::Shape LoadJourneyShape();
    void FinalizeAndInitialize();
    void LoadContentEras();
    void InitializeLayout();

    // Rewrites the item templates, once the world has loaded them.
    void ScaleItems();

    // Instance item level requirements, read in the item levels the rewritten templates carry.
    void ScaleAccessRequirements();

    [[nodiscard]] bool IsEnabled() const { return _enabled; }
    [[nodiscard]] bool IsLayoutInitialized() const { return _layoutInitialized; }
    [[nodiscard]] bool IsGroupScalingEnabled() const { return _groupScalingEnabled; }
    [[nodiscard]] float GetDamageMultiplier() const { return _damageMultiplier.load(std::memory_order_relaxed); }
    [[nodiscard]] bool IsWorldLeechEnabled() const { return _worldLeechEnabled.load(std::memory_order_relaxed); }
    [[nodiscard]] float GetWorldLeechPercent() const { return _worldLeechPercent.load(std::memory_order_relaxed); }
    [[nodiscard]] bool IsAdaptiveMechanicsEnabled() const { return _adaptiveMechanicsEnabled; }
    [[nodiscard]] bool IsDebugEnabled() const { return _debug; }

    // Maps played at their authored levels: no creature, access, loot or item from them is scaled.
    // Some are authentic on every difficulty, others only on Heroic and Mythic. Read once at startup,
    // like the layout, because the items cut at startup cannot follow a reload.
    [[nodiscard]] bool IsAuthenticMap(uint32 mapId, uint8 difficulty = 0) const;
    [[nodiscard]] bool IsAuthenticMap(Map const* map) const;
    // The level an authentic map asks for: as authored, raised to AuthenticHeroicMinLevel on the
    // Heroic and Mythic difficulties of the dungeons in AuthenticHeroicMaps.
    [[nodiscard]] uint8 GetAuthenticMinLevel(uint32 mapId, uint8 difficulty, uint8 authoredMinLevel) const;

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

    // The era of a map on one of its difficulties: Onyxia's Lair is Classic or Northrend content
    // depending on which one is entered.
    [[nodiscard]] ContentEra ResolveMapEra(uint32 mapId, uint8 difficulty = 0) const;

    // Map access validation
    [[nodiscard]] bool CanPlayerEnterMap(Player const* player, uint32 mapId, uint8 difficulty = 0) const;

    void OnPlayerLogout(Player* player);

private:
    CoAContentScaling() = default;

    [[nodiscard]] std::unordered_set<uint32> CollectAuthenticItems() const;

    bool _enabled{false};
    bool _layoutInitialized{false};
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
    journey::Shape _journeyShape;
    std::unordered_set<uint32> _authenticMaps;
    std::unordered_set<uint32> _authenticHeroicMaps;
    uint8 _authenticHeroicMinLevel{60};
};

#define sCoAContentScaling CoAContentScaling::Instance()

#endif // COA_CONTENT_SCALING_H
