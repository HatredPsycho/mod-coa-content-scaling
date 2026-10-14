/*
 * CoA Universal Content Scaling
 * JourneyBotPlan: what the random bots take from the journey - where they stop, when they ride, where they hunt.
 */

#ifndef COA_JOURNEY_BOT_PLAN_H
#define COA_JOURNEY_BOT_PLAN_H

#include "ContentScalingHooks.h"
#include "JourneyRules.h"

namespace journey
{
    /// Computed from the rules and the tables alone, so it holds before the world is loaded. `mounts` is whether the
    /// journey's riding trainers set the mount levels.
    bool FillBotLevelPlan(Rules const& rules, bool mounts, uint8_t maxPlayerLevel,
        LocalLevelScaling::BotLevelPlan& plan);
}

#endif // COA_JOURNEY_BOT_PLAN_H
