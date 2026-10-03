/*
 * CoA Universal Content Scaling
 * ScaledAuraFeed: Hands the client the amount an item-granted aura actually applies.
 */

#ifndef COA_SCALED_AURA_FEED_H
#define COA_SCALED_AURA_FEED_H

#include "Define.h"

#include <string>

class Player;

// A buff's text is rendered entirely client side from its own Spell.dbc: SMSG_AURA_UPDATE carries
// a slot, a spell, flags, a caster, stacks and durations, and no effect amounts at all. So an item
// proc keeps advertising the figure it was authored with however far the item it came from was cut,
// and the player has no way to see what was really applied.
//
// This feed sends both numbers to the owner. The authored one travels with it so the addon can
// replace it where it stands in the sentence rather than having to parse a description it does not
// know the shape or the language of.
// An item's tooltip has the same two halves. Its statistics come from the server and are already
// right, while the sentences underneath are built from the client's own Spell.dbc and are not. Those
// cannot be pushed the way an applied aura can: the player may be looking at a vendor's stock or a
// link in chat and never have held the item. So the client asks and the server answers.
namespace ScaledAuraFeed
{
    void Publish(Player* player, uint32 spellId, uint8 effectIndex, int32 authoredAmount, int32 appliedAmount);
    void Forget(Player const* player);

    bool AnswerRequest(Player* player, uint32 language, std::string const& message);
}

void AddScaledAuraFeedScripts();

#endif // COA_SCALED_AURA_FEED_H
