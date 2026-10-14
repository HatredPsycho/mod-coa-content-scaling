/*
 * CoA Universal Content Scaling
 * JourneyContent: the journey layout applied to the loaded world - creature templates, quest levels, instance
 * entries, the Dungeon Finder and the level window.
 */

#ifndef COA_JOURNEY_CONTENT_H
#define COA_JOURNEY_CONTENT_H

#include "Define.h"
#include "JourneyRules.h"
#include "LocalLevelScaling.h"
#include <array>
#include <atomic>
#include <functional>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

class Creature;
class Player;
class Unit;

struct JourneySettings
{
    bool journeyBaseStats{true};
    bool window{true};
    int windowBelow{2};
    int windowAbove{2};
    bool trainers{true};
    bool battlegrounds{true};
    bool enchantments{true};
    // On top of the core's quest experience rate, for the old world, Outland, Northrend and the endgame.
    std::array<float, 4> questXpRates{ 1.0f, 1.0f, 1.0f, 1.0f };
};

struct JourneyBuildReport
{
    uint32 creatureTemplates{0};
    uint32 creaturesMoved{0};
    uint32 questsMoved{0};
    uint32 dungeonFinderEntries{0};
    uint32 trainerSpells{0};
    uint32 battlegrounds{0};
};

struct JourneyEnchantmentReport
{
    uint32 enchantments{0};
    uint32 enchantmentLevels{0};
    float outlandEndgameRatio{1.0f};
    float northrendEndgameRatio{1.0f};
};

class JourneyContent
{
public:
    static JourneyContent* Instance();

    // Rewrites the creature templates and the Dungeon Finder, and works out the quest levels, once every store
    // is loaded. `keepMap` names the maps the journey leaves as authored.
    JourneyBuildReport Build(journey::Rules const& rules, JourneySettings const& settings, uint8 maxPlayerLevel,
        std::function<bool(uint32 mapId, uint8 difficulty)> const& keepMap);

    // Gems, the enchantments of Outland's and Northrend's professions and socket bonuses, once the items are cut:
    // by what the top rewards of their game kept of their statistics, or by what their item kept.
    JourneyEnchantmentReport BuildEnchantments();

    [[nodiscard]] bool IsBuilt() const { return _built.load(std::memory_order_acquire); }

    // The experience rate a quest pays on the journey, given the core's own.
    [[nodiscard]] float GetQuestXpRate(uint32 questId, float defaultRate) const;

    // The levels a zone or an instance set by hand moved its creatures by.
    [[nodiscard]] int GetCreatureShift(Creature const* creature) const;

    [[nodiscard]] std::optional<journey::QuestPlan> GetQuestPlan(uint32 questId) const;
    [[nodiscard]] std::optional<uint8> GetInstanceEntry(uint32 mapId, uint8 difficulty) const;

    // How hard a creature's spells - or those of its totems and summons - hit on the journey.
    [[nodiscard]] double GetSpellFactor(Unit const* attacker) const;

    // Where a lifted creature stands for a viewer: its place in its zone, inside the window around the viewer.
    [[nodiscard]] uint8 GetWindowLevel(Player const* viewer, Creature const* creature, uint8 ownLevel) const;

    // The journey runs through Outland and Northrend, and every normal dungeon is held between its new entry level
    // and the cap.
    void AddScalingScope(std::vector<uint32>& worldMaps,
                         std::vector<std::pair<uint32, LocalLevelScaling::LevelBand>>& dungeonBands) const;

private:
    JourneyContent() = default;

    struct Shift
    {
        uint8 originalMin;
        uint8 originalMax;
        uint8 newMin;
        uint8 newMax;
        uint8 originalExpansion;
        uint8 newExpansion;
    };

    struct ZonePlace
    {
        uint8 originalEntry;
        uint8 part;
    };

    void BuildCreatures(journey::Rules const& rules, JourneySettings const& settings,
        std::function<bool(uint32, uint8)> const& keepMap, JourneyBuildReport& report);
    void BuildQuests(journey::Rules const& rules, JourneyBuildReport& report);
    void BuildPlaces(journey::Rules const& rules);
    void BuildDungeonFinder(JourneyBuildReport& report);
    void BuildTrainers(journey::Rules const& rules, JourneyBuildReport& report);
    void BuildBattlegrounds(journey::Rules const& rules, JourneyBuildReport& report);
    void CollectEnchantmentSources();

    [[nodiscard]] int OriginalLevel(Creature const* creature) const;

    std::atomic<bool> _built{false};
    JourneySettings _settings;
    uint8 _maxPlayerLevel{60};

    std::unordered_map<uint32, uint32> _spawnMaps;
    std::unordered_map<uint32, Shift> _shifts;
    std::unordered_map<uint32, journey::QuestPlan> _quests;
    std::unordered_map<uint32, int> _zoneShifts;
    std::unordered_map<uint32, int> _mapShifts;
    std::unordered_map<uint32, ZonePlace> _zones;
    std::unordered_map<uint64, uint8> _instanceEntries;

    journey::Rules _rules;
    std::unordered_map<uint32, journey::ItemEra> _enchantmentEras;       // gem and craft enchantments
    std::unordered_map<uint32, std::vector<uint32>> _socketBonusItems;   // socket bonus -> items carrying it
};

#define sJourneyContent JourneyContent::Instance()

#endif // COA_JOURNEY_CONTENT_H
