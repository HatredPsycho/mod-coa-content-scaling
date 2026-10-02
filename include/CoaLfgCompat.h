/*
 * CoA Universal Content Scaling - fork note.
 *
 * This realm took the scaling and left the Dungeon Finder alone. The two types below live in the
 * core on the author's fork, in the half of his core-hooks patch that rewrites LFGQueue to honour a
 * per-player queue policy - the half that changes what every player meets every day, scaling or not.
 *
 * They are pure data, so they are declared here instead and the queue is never told about them.
 * Nothing in this core resolves a queue policy, so the composition mode a player can set is recorded
 * and never asked for. The module compiles unchanged, which keeps pulling the author's work a merge
 * rather than a repair.
 *
 * If the Dungeon Finder half is ever taken as well, this file is what collides with it, and deleting
 * it is the whole of the removal.
 */

#ifndef COA_LFG_COMPAT_H
#define COA_LFG_COMPAT_H

#include "Define.h"

namespace lfg
{
    enum class LfgCompositionMode : uint8
    {
        MATCHMAKING   = 0,
        BOT_FILL      = 1,
        CURRENT_PARTY = 2
    };

    struct LfgQueuePolicy
    {
        LfgCompositionMode compositionMode{LfgCompositionMode::MATCHMAKING};
        uint8 challengeSize{0}; // 0 = Adaptive
        bool bypassMatchmaking{false};
        bool requireStandardRoles{true};
        uint8 minPlayers{5};
        uint8 targetPlayers{5};
    };
}

#endif
