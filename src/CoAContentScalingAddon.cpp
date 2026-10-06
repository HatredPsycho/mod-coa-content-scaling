/*
 * CoA Universal Content Scaling
 * CoAContentScalingAddon: answers the CoALFGMode addon. It reads and changes the same per-character
 * Dungeon Finder settings as .lfgmode and .lfgchallenge, and always replies with the resulting state.
 */

#include "Chat.h"
#include "CoAContentScaling.h"
#include "LFG.h"
#include "LfgAddonProtocol.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "WorldPacket.h"
#include "WorldSession.h"

namespace
{
    static_assert(uint8(CoALfgAddon::Mode::Matchmaking) == uint8(lfg::LfgCompositionMode::MATCHMAKING));
    static_assert(uint8(CoALfgAddon::Mode::Bots) == uint8(lfg::LfgCompositionMode::BOT_FILL));
    static_assert(uint8(CoALfgAddon::Mode::Party) == uint8(lfg::LfgCompositionMode::CURRENT_PARTY));

    CoALfgAddon::State CurrentState(Player const* player)
    {
        CoALfgAddon::State state;
        state.mode = CoALfgAddon::Mode(uint8(sCoAContentScaling->GetPlayerLfgMode(player->GetGUID())));
        state.challenge = sCoAContentScaling->GetPlayerLfgChallenge(player->GetGUID());
        state.scalingEnabled = sCoAContentScaling->IsEnabled();
        state.botFillAvailable = sScriptMgr->HasLfgAutoFillProvider();
        return state;
    }

    void SendState(Player* player)
    {
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player,
            std::string(CoALfgAddon::Prefix) + '\t' + CoALfgAddon::FormatState(CurrentState(player)));
        player->GetSession()->SendPacket(&data);
    }

    void Apply(Player* player, CoALfgAddon::Request const& request)
    {
        switch (request.kind)
        {
            case CoALfgAddon::RequestKind::SetMode:
                sCoAContentScaling->SetPlayerLfgMode(player->GetGUID(), lfg::LfgCompositionMode(uint8(request.mode)));
                sCoAContentScaling->SavePlayerLfgSettings(player);
                break;
            case CoALfgAddon::RequestKind::SetChallenge:
                if (sCoAContentScaling->SetPlayerLfgChallenge(player->GetGUID(), request.challenge))
                    sCoAContentScaling->SavePlayerLfgSettings(player);
                break;
            default:
                break;
        }
    }
}

class coa_content_scaling_lfg_addon : public PlayerScript
{
public:
    coa_content_scaling_lfg_addon() : PlayerScript("coa_content_scaling_lfg_addon",
        {PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT}) { }

    bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 language, std::string& msg, Player* receiver) override
    {
        if (type != CHAT_MSG_WHISPER || language != LANG_ADDON || !player || receiver != player || !player->GetSession())
            return true;

        std::optional<std::string_view> const body = CoALfgAddon::Body(msg);
        if (!body)
            return true;

        if (std::optional<CoALfgAddon::Request> const request = CoALfgAddon::ParseRequest(*body))
            Apply(player, *request);

        SendState(player);
        return false;
    }
};

void AddCoAContentScalingAddonScripts()
{
    new coa_content_scaling_lfg_addon();
}
