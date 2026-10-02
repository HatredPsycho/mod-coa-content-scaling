/*
 * CoA Universal Content Scaling
 * CoATBCContent: Implementation of TBC content pack.
 */

#include "CoATBCContent.h"
#include "CoAContentScaling.h"
#include "Config.h"
#include "Log.h"
#include "ScriptMgr.h"
#include <unordered_set>

namespace
{
    // Canonical TBC maps: Outland, Dungeons, Raids
    std::unordered_set<uint32> const g_tbcMaps = {
        530, // Outland
        540, 542, 543, 544, // Hellfire (Shattered Halls, Blood Furnace, Ramparts, Magtheridon)
        545, 546, 547, 548, // Coilfang (Steamvault, Underbog, Slave Pens, SSC)
        550, 552, 553, 554, // Tempest Keep (Eye, Arcatraz, Botanica, Mechanar)
        555, 556, 557, 558, // Auchindoun (Shadow Labyrinth, Sethekk Halls, Mana-Tombs, Auchenai Crypts)
        269, 560,           // Caverns of Time (Black Morass, Old Hillsbrad)
        564,                // Black Temple
        565,                // Gruul's Lair
        568,                // Zul'Aman
        580,                // Sunwell Plateau
        585                 // Magisters' Terrace
    };

    class coa_tbc_content_world : public WorldScript
    {
    public:
        coa_tbc_content_world() : WorldScript("coa_tbc_content_world") { }

        void OnAfterConfigLoad(bool reload) override
        {
            if (reload && sContentPackRegistry->IsFinalized())
            {
                LOG_WARN("module.coa_content_scaling", "CoATBCContent: Expansion pack enablement cannot be changed at runtime. Server restart required.");
                return;
            }

            bool const enabled = sConfigMgr->GetOption<bool>("CoATBC.Enable", false);
            if (enabled)
            {
                sCoAContentScaling->SetTbcEnabled(true);
                sContentPackRegistry->RegisterPack(std::make_shared<CoATBCContentPack>());
                LOG_INFO("server.loading", "CoATBCContent: The Burning Crusade content pack registered");
            }
            else
            {
                sCoAContentScaling->SetTbcEnabled(false);
                sContentPackRegistry->UnregisterPack(ContentEra::TBC);
                LOG_INFO("server.loading", "CoATBCContent: The Burning Crusade content pack is disabled (locked)");
            }
        }
    };
}

bool CoATBCContentPack::HandlesMap(uint32 mapId) const
{
    return g_tbcMaps.find(mapId) != g_tbcMaps.end();
}

bool CoATBCContentPack::HandlesArea(uint32 /*areaId*/) const
{
    return false;
}

bool CoATBCContentPack::HandlesCreature(uint32 /*entry*/) const
{
    return false;
}

bool CoATBCContentPack::HandlesQuest(uint32 /*questId*/) const
{
    return false;
}

bool CoATBCContentPack::HandlesItem(uint32 /*itemId*/) const
{
    return false;
}

void AddCoATBCContentScripts()
{
    new coa_tbc_content_world();
}
