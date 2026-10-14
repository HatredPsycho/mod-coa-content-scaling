/*
 * CoA Universal Content Scaling
 * JourneyBotPlan: Implementation of the random bots' plan on the journey.
 *
 * After mod-world-journey by aspollon (JourneyDb.cpp, the zones and bots, faed6d6, GNU AGPL v3).
 */

#include "JourneyBotPlan.h"
#include "JourneyData.h"
#include <algorithm>
#include <unordered_map>

namespace journey
{
    bool FillBotLevelPlan(Rules const& rules, bool mounts, uint8_t maxPlayerLevel,
        LocalLevelScaling::BotLevelPlan& plan)
    {
        plan.maxLevel = uint8_t(std::clamp<int>(rules.GetShape().endgame, 1, std::max<int>(maxPlayerLevel, 1)));
        if (mounts)
        {
            std::array<int, 4> const levels = rules.MountLevels();
            plan.groundMountLevel = uint8_t(levels[0]);
            plan.fastGroundMountLevel = uint8_t(levels[1]);
            plan.flyMountLevel = uint8_t(levels[2]);
            plan.fastFlyMountLevel = uint8_t(levels[3]);
        }

        // How far a zone set by hand moved from where the journey would have put it.
        std::unordered_map<uint32_t, int> zoneShifts;
        for (ZoneData const& zone : Zones)
            if (int const delta = rules.ZoneEntry(zone.zone, zone.part, zone.originalEntry, true) -
                rules.ZoneEntry(zone.zone, zone.part, zone.originalEntry, false))
                zoneShifts[zone.zone] = delta;

        plan.zoneBrackets.clear();
        for (BotZoneData const& zone : BotZones)
        {
            auto const shift = zoneShifts.find(zone.zone);
            BotBracket const bracket = rules.ZoneBotBracket(zone.zone, zone.part, zone.originalMin, zone.originalMax,
                shift != zoneShifts.end() ? shift->second : 0);
            plan.zoneBrackets.push_back({ zone.zone, { uint8_t(bracket.minLevel), uint8_t(bracket.maxLevel) } });
        }
        return true;
    }
}
