/*
 * CoA Universal Content Scaling
 * CoAContentScalingConfig: Canonical configuration keys and centralized parsers.
 */

#ifndef COA_CONTENT_SCALING_CONFIG_H
#define COA_CONTENT_SCALING_CONFIG_H

#include "Define.h"
#include "DungeonFinding/LFG.h"
#include "SoloAssistPolicy.h"
#include <string>
#include <string_view>

namespace CoAContentScalingConfigKeys
{
    inline constexpr char const* Enable = "CoAContentScaling.Enable";
    inline constexpr char const* ProgressionMode = "CoAContentScaling.Progression.Mode";
    inline constexpr char const* ProgressionClassicEnd = "CoAContentScaling.Progression.ClassicEnd";
    inline constexpr char const* ProgressionTbcEnd = "CoAContentScaling.Progression.TbcEnd";
    inline constexpr char const* GroupScalingEnable = "CoAContentScaling.GroupScaling.Enable";
    inline constexpr char const* GroupScalingLockOnEncounterStart = "CoAContentScaling.GroupScaling.LockOnEncounterStart";
    inline constexpr char const* GroupScalingAllowSoloRaids = "CoAContentScaling.GroupScaling.AllowSoloRaids";
    inline constexpr char const* AdaptiveMechanicsEnable = "CoAContentScaling.AdaptiveMechanics.Enable";
    inline constexpr char const* SoloAssistMode = "CoAContentScaling.SoloAssist.Mode";
    inline constexpr char const* RewardsScaleLootCount = "CoAContentScaling.Rewards.ScaleLootCount";
    inline constexpr char const* ScaleItems = "CoAContentScaling.ScaleItems";
    inline constexpr char const* PublishAuraAmounts = "CoAContentScaling.PublishAuraAmounts";
    inline constexpr char const* ScaleEnchantments = "CoAContentScaling.ScaleEnchantments";
    inline constexpr char const* LfgDefaultMode = "CoAContentScaling.LFG.DefaultMode";
    inline constexpr char const* LfgDefaultChallengeSize = "CoAContentScaling.LFG.DefaultChallengeSize";
    inline constexpr char const* Debug = "CoAContentScaling.Debug";
}

namespace CoAContentScalingConfig
{
    std::string Trim(std::string_view str);

    std::string ParseProgressionMode(std::string_view raw);

    SoloAssistMode ParseSoloAssistMode(std::string_view raw);

    lfg::LfgCompositionMode ParseLfgCompositionMode(std::string_view raw);
}

#endif // COA_CONTENT_SCALING_CONFIG_H
