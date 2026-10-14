/*
 * CoA Universal Content Scaling
 * CoAContentScalingConfig: Canonical configuration keys and centralized parsers.
 */

#ifndef COA_CONTENT_SCALING_CONFIG_H
#define COA_CONTENT_SCALING_CONFIG_H

#include "Define.h"
#include "JourneyRules.h"
#include "SoloAssistPolicy.h"
#include <string>
#include <string_view>
#include <unordered_set>

namespace CoAContentScalingConfigKeys
{
    inline constexpr char const* Enable = "CoAContentScaling.Enable";
    inline constexpr char const* ProgressionMode = "CoAContentScaling.Progression.Mode";
    inline constexpr char const* ProgressionClassicEnd = "CoAContentScaling.Progression.ClassicEnd";
    inline constexpr char const* ProgressionTbcEnd = "CoAContentScaling.Progression.TbcEnd";
    inline constexpr char const* TbcEnable = "CoAContentScaling.TBC.Enable";
    inline constexpr char const* WotlkEnable = "CoAContentScaling.WotLK.Enable";
    inline constexpr char const* JourneyClassicRange = "CoAContentScaling.Journey.Classic.Range";
    inline constexpr char const* JourneyOutlandRange = "CoAContentScaling.Journey.Outland.Range";
    inline constexpr char const* JourneyNorthrendRange = "CoAContentScaling.Journey.Northrend.Range";
    inline constexpr char const* JourneyEndgameLevel = "CoAContentScaling.Journey.Endgame.Level";
    inline constexpr char const* JourneyEndgameBossLevels = "CoAContentScaling.Journey.Endgame.BossLevels";
    inline constexpr char const* JourneyOutlandItemLevels = "CoAContentScaling.Journey.Endgame.Outland.ItemLevels";
    inline constexpr char const* JourneyNorthrendItemLevels = "CoAContentScaling.Journey.Endgame.Northrend.ItemLevels";
    inline constexpr char const* JourneyZonePrefix = "CoAContentScaling.Journey.Zone.";
    inline constexpr char const* JourneyCreatureBaseStats = "CoAContentScaling.Journey.Creatures.BaseStats";
    inline constexpr char const* JourneyWindowEnable = "CoAContentScaling.Journey.Window.Enable";
    inline constexpr char const* JourneyWindowBelow = "CoAContentScaling.Journey.Window.Below";
    inline constexpr char const* JourneyWindowAbove = "CoAContentScaling.Journey.Window.Above";
    inline constexpr char const* JourneyTrainersEnable = "CoAContentScaling.Journey.Trainers.Enable";
    inline constexpr char const* JourneyBattlegroundsEnable = "CoAContentScaling.Journey.Battlegrounds.Enable";
    inline constexpr char const* JourneyEnchantmentsEnable = "CoAContentScaling.Journey.Enchantments.Enable";
    inline constexpr char const* JourneyQuestXpRate = "CoAContentScaling.Journey.Quests.XpRate";
    inline constexpr char const* JourneyInstancePrefix = "CoAContentScaling.Journey.Instance.";
    inline constexpr char const* GroupScalingEnable = "CoAContentScaling.GroupScaling.Enable";
    inline constexpr char const* GroupScalingLockOnEncounterStart = "CoAContentScaling.GroupScaling.LockOnEncounterStart";
    inline constexpr char const* GroupScalingAllowSoloRaids = "CoAContentScaling.GroupScaling.AllowSoloRaids";
    inline constexpr char const* AdaptiveMechanicsEnable = "CoAContentScaling.AdaptiveMechanics.Enable";
    inline constexpr char const* SoloAssistMode = "CoAContentScaling.SoloAssist.Mode";
    inline constexpr char const* DamageMultiplier = "CoAContentScaling.Difficulty.DamageMultiplier";
    inline constexpr char const* WorldLeechEnable = "CoAContentScaling.World.Leech.Enable";
    inline constexpr char const* WorldLeechPercent = "CoAContentScaling.World.Leech.Percent";
    inline constexpr char const* RewardsScaleLootCount = "CoAContentScaling.Rewards.ScaleLootCount";
    inline constexpr char const* ScaleItems = "CoAContentScaling.ScaleItems";
    inline constexpr char const* PublishAuraAmounts = "CoAContentScaling.PublishAuraAmounts";
    inline constexpr char const* ScaleEnchantments = "CoAContentScaling.ScaleEnchantments";
    inline constexpr char const* Debug = "CoAContentScaling.Debug";
    inline constexpr char const* AuthenticMaps = "CoAContentScaling.AuthenticMaps";
    inline constexpr char const* AuthenticHeroicMaps = "CoAContentScaling.AuthenticHeroicMaps";
    inline constexpr char const* AuthenticHeroicMinLevel = "CoAContentScaling.AuthenticHeroicMinLevel";
}

namespace CoAContentScalingConfig
{
    std::string Trim(std::string_view str);

    // A comma separated list of map ids. Entries that are not a number are skipped with a warning.
    std::unordered_set<uint32> ParseMapList(std::string_view raw);

    std::string ParseProgressionMode(std::string_view raw);

    // A level range written as "from-to". Anything else is the fallback, with a warning naming the key.
    journey::Range ParseLevelRange(std::string_view key, std::string_view raw, journey::Range fallback);

    SoloAssistMode ParseSoloAssistMode(std::string_view raw);
}

#endif // COA_CONTENT_SCALING_CONFIG_H
