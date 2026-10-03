/*
 * CoA Universal Content Scaling
 * ScaledAuraFeed: Implementation of the addon channel carrying applied aura amounts.
 */

#include "ScaledAuraFeed.h"
#include "Chat.h"
#include "CoAContentScalingConfig.h"
#include "Config.h"
#include "ItemBudgetScaler.h"
#include "ItemTemplate.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "DBCStores.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
constexpr char const* AddonPrefix = "CoAScale";

// The shape the core's own AddonChannelCommandHandler uses: the client splits prefix from body on
// the first tab.
constexpr char Separator = '\t';

// SMSG_MESSAGECHAT carries the body as a null terminated string, but the client's own addon channel
// is held to a far smaller line, so a compact budget keeps an item's whole chain in one message.
constexpr std::size_t MaximumBody = 200;
constexpr uint8 MaximumTriggerDepth = 3;
constexpr std::size_t MaximumEnchantmentsAsked = 5;

// An aura amount is recalculated whenever anything it reads changes, so a single fight would spend
// dozens of messages restating a number the client already holds. What was last sent is remembered
// per player and only a different figure goes out again.
std::unordered_map<ObjectGuid, std::unordered_map<uint64, int32>> publishedAmounts;
std::mutex publishedAmountsMutex;

uint64 FeedKey(uint32 spellId, uint8 effectIndex)
{
    return (uint64(spellId) << 8) | uint64(effectIndex);
}

bool Enabled()
{
    return sConfigMgr->GetOption<bool>(CoAContentScalingConfigKeys::PublishAuraAmounts, true);
}

void Send(Player* player, std::string const& body)
{
    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player,
        std::string(AddonPrefix) + Separator + body);
    player->GetSession()->SendPacket(&data);
}

int32 ScaleAmount(int32 value, float multiplier)
{
    int32 const scaled = static_cast<int32>(std::lround(float(value) * multiplier));
    return value > 0 ? std::max(1, scaled) : std::min(-1, scaled);
}

// Mirrors SpellEffectInfo::CalcValue for the level independent case, which is what a flat amount on
// an item is. The client prints the same arithmetic from its own copy of the spell, so reproducing
// it here is what lets the addon find the number inside a finished sentence.
void AuthoredRange(SpellEffectInfo const& effect, int32& minimum, int32& maximum)
{
    int32 const dieSides = int32(effect.DieSides);

    minimum = effect.BasePoints;
    maximum = effect.BasePoints;

    if (dieSides == 1)
    {
        minimum += 1;
        maximum += 1;
    }
    else if (dieSides > 1)
    {
        minimum += 1;
        maximum += dieSides;
    }
}

// An item's own spell is often only the aura that watches for the proc; the figure the tooltip
// prints lives in what that aura sets off, which the sentence reaches across. So the chain is
// followed. Where a hop is made by a script rather than written in the spell's data there is
// nothing here to follow, and those amounts stay as they were authored.
void CollectSpellAmounts(uint32 spellId, float statMultiplier, float ratingMultiplier,
    std::unordered_set<uint32>& visited, uint8 depth, std::string& body)
{
    if (!spellId || depth > MaximumTriggerDepth || body.size() > MaximumBody)
        return;

    if (!visited.insert(spellId).second)
        return;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return;

    for (uint8 index = 0; index < MAX_SPELL_EFFECTS; ++index)
    {
        SpellEffectInfo const& effect = spellInfo->Effects[index];
        if (!effect.Effect)
            continue;

        CollectSpellAmounts(effect.TriggerSpell, statMultiplier, ratingMultiplier, visited, depth + 1, body);

        if (effect.RealPointsPerLevel != 0.0f)
            continue;

        ScaledAmountFactor const factor = effect.Effect == SPELL_EFFECT_APPLY_AURA ?
            GetAuraAmountFactor(effect.ApplyAuraName) : GetSpellEffectAmountFactor(effect.Effect);

        float multiplier = 1.0f;
        switch (factor)
        {
            case ScaledAmountFactor::Stat:
                multiplier = statMultiplier;
                break;
            case ScaledAmountFactor::Rating:
                multiplier = ratingMultiplier;
                break;
            case ScaledAmountFactor::None:
                continue;
        }

        if (multiplier >= 1.0f)
            continue;

        int32 authoredMinimum = 0;
        int32 authoredMaximum = 0;
        AuthoredRange(effect, authoredMinimum, authoredMaximum);
        if (!authoredMinimum && !authoredMaximum)
            continue;

        std::string const row = std::to_string(authoredMinimum) + ',' + std::to_string(ScaleAmount(authoredMinimum, multiplier)) +
            ',' + std::to_string(authoredMaximum) + ',' + std::to_string(ScaleAmount(authoredMaximum, multiplier));

        if (body.find(row) != std::string::npos)
            continue;

        if (body.size() + row.size() + 1 > MaximumBody)
            return;

        if (!body.empty())
            body += ';';
        body += row;
    }
}

// Anything whose value moves with the caster's level is left out: the client computes those from the
// reader's own level and the server cannot say in advance what it printed. Nothing is sent for them,
// so the addon leaves the line as it found it instead of putting a wrong number in its place.
// An enchantment, a gem and a socket bonus print their amount from the client's own copy of the
// enchantment, and which ones an item carries belongs to that one copy of it rather than to the
// template, so the client names them in its question.
void CollectEnchantmentAmounts(uint32 enchantmentId, float statMultiplier, float ratingMultiplier, std::string& body)
{
    SpellItemEnchantmentEntry const* enchantment =
        enchantmentId ? sSpellItemEnchantmentStore.LookupEntry(enchantmentId) : nullptr;
    if (!enchantment)
        return;

    for (uint8 index = 0; index < MAX_SPELL_ITEM_ENCHANTMENT_EFFECTS; ++index)
    {
        uint32 const authored = enchantment->amount[index];
        if (!authored)
            continue;

        float multiplier = 1.0f;
        switch (enchantment->type[index])
        {
            case ITEM_ENCHANTMENT_TYPE_STAT:
                multiplier = GetItemModCategory(enchantment->spellid[index]) == ItemModCategory::SecondaryRating ?
                    ratingMultiplier : statMultiplier;
                break;
            case ITEM_ENCHANTMENT_TYPE_DAMAGE:
            case ITEM_ENCHANTMENT_TYPE_RESISTANCE:
            case ITEM_ENCHANTMENT_TYPE_TOTEM:
                multiplier = statMultiplier;
                break;
            default:
                continue;
        }

        if (multiplier >= 1.0f)
            continue;

        uint32 const applied = std::max<uint32>(1, static_cast<uint32>(std::lround(float(authored) * multiplier)));
        std::string const row = std::to_string(authored) + ',' + std::to_string(applied) + ',' +
            std::to_string(authored) + ',' + std::to_string(applied);

        if (body.find(row) != std::string::npos || body.size() + row.size() + 1 > MaximumBody)
            continue;

        if (!body.empty())
            body += ';';
        body += row;
    }
}

std::string DescribeItemEffects(ItemTemplate const* proto, std::vector<uint32> const& enchantments)
{
    float const statMultiplier = sItemBudgetScaler->GetStatMultiplier(proto->ItemId);
    float const ratingMultiplier = sItemBudgetScaler->GetRatingMultiplier(proto->ItemId);
    if (statMultiplier >= 1.0f && ratingMultiplier >= 1.0f)
        return "";

    std::string body;

    std::unordered_set<uint32> visited;

    for (uint8 spellSlot = 0; spellSlot < MAX_ITEM_PROTO_SPELLS; ++spellSlot)
        CollectSpellAmounts(uint32(proto->Spells[spellSlot].SpellId), statMultiplier, ratingMultiplier,
            visited, 0, body);

    CollectEnchantmentAmounts(proto->socketBonus, statMultiplier, ratingMultiplier, body);

    for (uint32 enchantmentId : enchantments)
        CollectEnchantmentAmounts(enchantmentId, statMultiplier, ratingMultiplier, body);

    return body;
}
}

void ScaledAuraFeed::Publish(Player* player, uint32 spellId, uint8 effectIndex, int32 authoredAmount, int32 appliedAmount)
{
    if (!player || !player->GetSession() || authoredAmount == appliedAmount || !Enabled())
        return;

    uint64 const key = FeedKey(spellId, effectIndex);

    {
        std::lock_guard<std::mutex> guard(publishedAmountsMutex);
        auto& known = publishedAmounts[player->GetGUID()];
        auto const itr = known.find(key);
        if (itr != known.end() && itr->second == appliedAmount)
            return;

        known[key] = appliedAmount;
    }

    Send(player, "A:" + std::to_string(spellId) + ':' + std::to_string(uint32(effectIndex)) + ':' +
        std::to_string(authoredAmount) + ':' + std::to_string(appliedAmount));
}

void ScaledAuraFeed::Forget(Player const* player)
{
    if (!player)
        return;

    std::lock_guard<std::mutex> guard(publishedAmountsMutex);
    publishedAmounts.erase(player->GetGUID());
}

bool ScaledAuraFeed::AnswerRequest(Player* player, uint32 language, std::string const& message)
{
    if (language != LANG_ADDON || !player || !player->GetSession())
        return false;

    std::string const header = std::string(AddonPrefix) + Separator + "Q:";
    if (message.compare(0, header.size(), header) != 0)
        return false;

    if (!Enabled())
        return true;

    // "<item>" or "<item>:<enchantment>:<gem>:<gem>:<gem>:<gem>", as the client reads them out of
    // the link it is about to draw. An unreadable field ends the list rather than the answer.
    std::string const asked = message.substr(header.size());

    uint32 itemId = 0;
    std::vector<uint32> enchantments;

    std::size_t cursor = header.size();
    while (cursor <= message.size())
    {
        std::size_t const separator = message.find(':', cursor);
        std::string const field = message.substr(cursor, separator - cursor);

        uint32 value = 0;
        try
        {
            value = field.empty() ? 0 : uint32(std::stoul(field));
        }
        catch (std::exception const&)
        {
            break;
        }

        if (!itemId && enchantments.empty() && cursor == header.size())
            itemId = value;
        else if (value && enchantments.size() < MaximumEnchantmentsAsked)
            enchantments.push_back(value);

        if (separator == std::string::npos)
            break;

        cursor = separator + 1;
    }

    ItemTemplate const* proto = itemId ? sObjectMgr->GetItemTemplate(itemId) : nullptr;

    // The question is echoed back verbatim, because it names an item together with the enchantment
    // and gems of this one copy of it and only the whole of that identifies what the answer is for.
    //
    // An answer goes back even when there is nothing to change, so the addon can tell an item it has
    // already asked about from one it has not and stops asking for the same tooltip over and over.
    Send(player, "I:" + asked + '|' + (proto ? DescribeItemEffects(proto, enchantments) : std::string()));
    return true;
}

namespace
{
class coa_scaled_aura_feed : public PlayerScript
{
public:
    coa_scaled_aura_feed() : PlayerScript("coa_scaled_aura_feed",
        {PLAYERHOOK_CAN_PLAYER_USE_CHAT, PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT}) { }

    bool OnPlayerCanUseChat(Player* player, uint32 /*type*/, uint32 language, std::string& msg) override
    {
        return !ScaledAuraFeed::AnswerRequest(player, language, msg);
    }

    bool OnPlayerCanUseChat(Player* player, uint32 /*type*/, uint32 language, std::string& msg, Player* /*receiver*/) override
    {
        return !ScaledAuraFeed::AnswerRequest(player, language, msg);
    }
};
}

void AddScaledAuraFeedScripts()
{
    new coa_scaled_aura_feed();
}
