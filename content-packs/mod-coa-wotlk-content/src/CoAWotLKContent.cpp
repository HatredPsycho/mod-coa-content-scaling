/*
 * CoA Universal Content Scaling
 * CoAWotLKContent: Implementation of WotLK content pack.
 */

#include "CoAWotLKContent.h"
#include "CoAContentScaling.h"
#include "Config.h"
#include "Log.h"
#include "ScriptMgr.h"
#include <unordered_set>

namespace
{
    // Canonical WotLK maps: Northrend, Dungeons, Raids
    std::unordered_set<uint32> const g_wotlkMaps = {
        571, // Northrend
        574, 575,           // Utgarde Keep, Utgarde Pinnacle
        576, 578, 616,      // The Nexus, The Oculus, Eye of Eternity
        601, 619,           // Azjol-Nerub, Ahn'kahet: The Old Kingdom
        600,                // Drak'Tharon Keep
        604,                // Gundrak
        599, 602,           // Halls of Stone, Halls of Lightning
        608,                // Violet Hold
        650, 649,           // Trial of the Champion, Trial of the Crusader
        632, 658, 668,      // Forge of Souls, Pit of Saron, Halls of Reflection
        631,                // Icecrown Citadel
        533,                // Naxxramas (Northrend)
        615, 724,           // The Obsidian Sanctum, The Ruby Sanctum
        624,                // Vault of Archavon
        603                 // Ulduar
    };

    class coa_wotlk_content_world : public WorldScript
    {
    public:
        coa_wotlk_content_world() : WorldScript("coa_wotlk_content_world") { }

        void OnAfterConfigLoad(bool reload) override
        {
            if (reload && sContentPackRegistry->IsFinalized())
            {
                LOG_WARN("module.coa_content_scaling", "CoAWotLKContent: Expansion pack enablement cannot be changed at runtime. Server restart required.");
                return;
            }

            bool const enabled = sConfigMgr->GetOption<bool>("CoAWotLK.Enable", false);
            if (enabled)
            {
                sCoAContentScaling->SetWotlkEnabled(true);
                sContentPackRegistry->RegisterPack(std::make_shared<CoAWotLKContentPack>());
                LOG_INFO("server.loading", "CoAWotLKContent: Wrath of the Lich King content pack registered");
            }
            else
            {
                sCoAContentScaling->SetWotlkEnabled(false);
                sContentPackRegistry->UnregisterPack(ContentEra::WotLK);
                LOG_INFO("server.loading", "CoAWotLKContent: Wrath of the Lich King content pack is disabled (locked)");
            }
        }
    };
}

bool CoAWotLKContentPack::HandlesMap(uint32 mapId) const
{
    return g_wotlkMaps.find(mapId) != g_wotlkMaps.end();
}

bool CoAWotLKContentPack::HandlesArea(uint32 /*areaId*/) const
{
    return false;
}

bool CoAWotLKContentPack::HandlesCreature(uint32 /*entry*/) const
{
    return false;
}

bool CoAWotLKContentPack::HandlesQuest(uint32 /*questId*/) const
{
    return false;
}

bool CoAWotLKContentPack::HandlesItem(uint32 /*itemId*/) const
{
    return false;
}

void AddCoAWotLKContentScripts()
{
    new coa_wotlk_content_world();
}
