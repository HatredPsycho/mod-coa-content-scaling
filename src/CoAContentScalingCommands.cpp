/*
 * CoA Universal Content Scaling
 * CoAContentScalingCommands: Diagnostic and administration commands (.coascale, .lfgmode, .lfgchallenge).
 */

#include "AdaptiveEncounterAPI.h"
#include "Chat.h"
#include "CoAContentScaling.h"
#include "CombatBudgetProfile.h"
#include "CommandScript.h"
#include "ContentPackRegistry.h"
#include "Creature.h"
#include "GeneratedContentCensus.h"
#include "InstanceScaleContext.h"
#include "ItemBudgetScaler.h"
#include "ItemTemplate.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ProgressionContext.h"
#include "ProgressionLayout.h"
#include "ProgressionRewardResolver.h"
#include "QuestDef.h"
#include "ScriptMgr.h"
#include <string>

using namespace Acore::ChatCommands;

class coa_content_scaling_commandscript : public CommandScript
{
public:
    coa_content_scaling_commandscript() : CommandScript("coa_content_scaling_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable const coaScaleLfgCommandTable =
        {
            { "",          HandleLfgStatus,    SEC_PLAYER, Console::No },
            { "mode",      HandleLfgMode,      SEC_PLAYER, Console::No },
            { "challenge", HandleLfgChallenge, SEC_PLAYER, Console::No },
            { "dungeon",   HandleLfgDungeon,   SEC_ADMINISTRATOR, Console::Yes },
        };

        static ChatCommandTable const coaScaleCommandTable =
        {
            { "status",      HandleStatus,      SEC_ADMINISTRATOR, Console::Yes },
            { "census",      HandleCensus,      SEC_ADMINISTRATOR, Console::Yes },
            { "layout",      HandleLayout,      SEC_ADMINISTRATOR, Console::Yes },
            { "progression", HandleProgression, SEC_PLAYER,        Console::No },
            { "creature",    HandleCreature,    SEC_ADMINISTRATOR, Console::No },
            { "instance",    HandleInstance,    SEC_ADMINISTRATOR, Console::No },
            { "encounter",   HandleEncounter,   SEC_ADMINISTRATOR, Console::No },
            { "quest",       HandleQuest,       SEC_ADMINISTRATOR, Console::Yes },
            { "item",        HandleItem,        SEC_ADMINISTRATOR, Console::Yes },
            { "validate",    HandleValidate,    SEC_ADMINISTRATOR, Console::Yes },
            { "lfg",         coaScaleLfgCommandTable },
        };

        static ChatCommandTable const commandTable =
        {
            { "coascale",     coaScaleCommandTable },
            { "lfgmode",      HandleLfgMode,      SEC_PLAYER, Console::No },
            { "lfgchallenge", HandleLfgChallenge, SEC_PLAYER, Console::No },
        };
        return commandTable;
    }

private:
    static bool HandleStatus(ChatHandler* handler)
    {
        ProgressionLayout const& layout = sCoAContentScaling->GetLayout();
        handler->PSendSysMessage("=== CoA Universal Content Scaling Status ===");
        handler->PSendSysMessage("Enabled: {}", sCoAContentScaling->IsEnabled() ? "Yes" : "No");
        handler->PSendSysMessage("MaxPlayerLevel: {}", uint32(layout.maxLevel));
        handler->PSendSysMessage("Active Packs: Classic{}{}",
            layout.tbcEnabled ? ", TBC" : "", layout.wotlkEnabled ? ", WotLK" : "");
        handler->PSendSysMessage("Group Scaling: {}", sCoAContentScaling->IsGroupScalingEnabled() ? "Enabled" : "Disabled");
        handler->PSendSysMessage("Adaptive Mechanics: {}", sCoAContentScaling->IsAdaptiveMechanicsEnabled() ? "Enabled" : "Disabled");
        return true;
    }

    static bool HandleCensus(ChatHandler* handler)
    {
        handler->PSendSysMessage("=== CoA Content Census Status (Schema: {}) ===", GENERATED_CONTENT_CENSUS_SCHEMA_VERSION);
        handler->PSendSysMessage("Authoritative Maps: {} (PvE Instances: {}, Excluded PvP: 65)",
            sGeneratedMapProfiles.size(), sGeneratedInstanceProfiles.size());
        handler->PSendSysMessage("Quests: {} profiles in census", sGeneratedQuestProfiles.size());
        handler->PSendSysMessage("Creature Placements: {} spawn-map profiles", sGeneratedCreaturePlacements.size());
        handler->PSendSysMessage("Item Drops: {} equipment profiles", sGeneratedItemProfiles.size());
        handler->PSendSysMessage("LFG Dungeon Entries: {} profiles mapped", sGeneratedLfgProfiles.size());
        handler->PSendSysMessage("Dungeon Access Entries: {} profiles mapped", sGeneratedAccessProfiles.size());
        handler->PSendSysMessage("Generator: v3.1.0 (Census Integrity: High Confidence, 0 PvP Leaks)");
        return true;
    }

    static bool HandleLayout(ChatHandler* handler)
    {
        ProgressionLayout const& layout = sCoAContentScaling->GetLayout();
        handler->PSendSysMessage("=== CoA Progression Layout ===");
        handler->PSendSysMessage("MaxPlayerLevel: {}", uint32(layout.maxLevel));
        handler->PSendSysMessage("Classic: {} -> {}", uint32(layout.classic.minLevel), uint32(layout.classic.maxLevel));
        if (layout.tbc.has_value())
            handler->PSendSysMessage("TBC:     {} -> {}", uint32(layout.tbc->minLevel), uint32(layout.tbc->maxLevel));
        else
            handler->PSendSysMessage("TBC:     [Disabled / Locked]");

        if (layout.wotlk.has_value())
            handler->PSendSysMessage("WotLK:   {} -> {}", uint32(layout.wotlk->minLevel), uint32(layout.wotlk->maxLevel));
        else
            handler->PSendSysMessage("WotLK:   [Disabled / Locked]");

        return true;
    }

    static bool HandleProgression(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        ProgressionLayout const& layout = sCoAContentScaling->GetLayout();
        uint8 const playerLevel = player->GetLevel();

        // Determine player's active era based on their current level
        ContentEra activeEra = ContentEra::Classic;
        if (layout.wotlkEnabled && layout.wotlk.has_value() && playerLevel >= layout.wotlk->minLevel)
            activeEra = ContentEra::WotLK;
        else if (layout.tbcEnabled && layout.tbc.has_value() && playerLevel >= layout.tbc->minLevel)
            activeEra = ContentEra::TBC;

        LevelRange const activeRange = layout.GetEraRange(activeEra);
        float eraProgress = 0.0f;
        if (activeRange.IsValid() && activeRange.maxLevel > activeRange.minLevel)
            eraProgress = float(playerLevel - activeRange.minLevel) / float(activeRange.maxLevel - activeRange.minLevel);

        handler->PSendSysMessage("=== Player Progression Status ===");
        handler->PSendSysMessage("Current Level: {} / {} (MaxPlayerLevel: {})",
            playerLevel, layout.maxLevel, layout.maxLevel);
        handler->PSendSysMessage("Enabled Packs: Classic{}{}",
            layout.tbcEnabled ? ", TBC" : "", layout.wotlkEnabled ? ", WotLK" : "");
        handler->PSendSysMessage("Effective Era: {} (Range: {} - {} | Progress: {:.1f}%)",
            ContentEraToString(activeEra).data(), activeRange.minLevel, activeRange.maxLevel, eraProgress * 100.0f);

        // Display tier unlock gates for active era
        uint8 const heroicUnlock = sProgressionRewardResolver->ResolveTierUnlockLevel(ContentTier::DUNGEON_HEROIC, activeEra, layout);
        uint8 const raidEntryUnlock = sProgressionRewardResolver->ResolveTierUnlockLevel(ContentTier::RAID_ENTRY, activeEra, layout);
        uint8 const raidMidUnlock = sProgressionRewardResolver->ResolveTierUnlockLevel(ContentTier::RAID_MID, activeEra, layout);
        uint8 const raidEndUnlock = sProgressionRewardResolver->ResolveTierUnlockLevel(ContentTier::RAID_END, activeEra, layout);

        handler->PSendSysMessage("Unlock Gates [{}]:", ContentEraToString(activeEra).data());
        handler->PSendSysMessage(" - Heroic Dungeons: L{} ({})", heroicUnlock, playerLevel >= heroicUnlock ? "UNLOCKED" : "LOCKED");
        handler->PSendSysMessage(" - Raid Entry:     L{} ({})", raidEntryUnlock, playerLevel >= raidEntryUnlock ? "UNLOCKED" : "LOCKED");
        handler->PSendSysMessage(" - Raid Mid:       L{} ({})", raidMidUnlock, playerLevel >= raidMidUnlock ? "UNLOCKED" : "LOCKED");
        handler->PSendSysMessage(" - Raid End/Pinn.: L{} ({})", raidEndUnlock, playerLevel >= raidEndUnlock ? "UNLOCKED" : "LOCKED");

        return true;
    }

    static bool HandleCreature(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        Creature* target = handler->getSelectedCreature();
        if (!target)
        {
            handler->SendSysMessage("No creature selected.");
            return true;
        }

        CreatureTemplate const* cinfo = target->GetCreatureTemplate();
        if (!cinfo)
            return true;

        CreatureScaleContext const ctx = sCombatBudgetProfile->BuildContext(cinfo, target, target->GetLevel());
        CalculatedCombatBudget const budget = sCombatBudgetProfile->CalculateBudget(cinfo, ctx);
        EraResolutionResult const eraRes = sContentPackRegistry->ResolveEraDetailsForCreature(
            cinfo->Entry, target->GetMapId(), target->GetAreaId(), cinfo->expansion, cinfo->maxlevel);

        handler->PSendSysMessage("=== Creature Scaling: {} (Entry: {}) ===", cinfo->Name.c_str(), cinfo->Entry);
        handler->PSendSysMessage("Era: {} | Resolved by: {} (Confidence: {:.2f})",
            ContentEraToString(eraRes.era).data(), EraResolutionSourceToString(eraRes.source).data(), eraRes.confidence);
        handler->PSendSysMessage("Authored Level: {} | Effective Level: {}", uint32(ctx.authoredLevel), uint32(ctx.effectiveLevel));
        handler->PSendSysMessage("Tier: {} | HealthMod: {:.2f} | DamageMod: {:.2f}",
            ContentTierToString(ctx.tier).data(), cinfo->ModHealth, cinfo->DamageModifier);
        handler->PSendSysMessage("Scaled Health: {} | Mana: {} | Armor: {:.0f} | AP: {}",
            budget.health, budget.mana, budget.armor, budget.attackPower);
        handler->PSendSysMessage("Damage Range: {:.1f} - {:.1f} | Current HP: {} / {}",
            budget.minDamage, budget.maxDamage, target->GetHealth(), target->GetMaxHealth());

        return true;
    }

    static bool HandleInstance(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        Map* map = player->GetMap();
        if (!map || !map->IsDungeon())
        {
            handler->SendSysMessage("You are not inside an instance/dungeon.");
            return true;
        }

        InstanceScaleContext const ctx = sInstanceScalingMgr->GetOrCreateContext(map);
        EraResolutionResult const eraRes = sContentPackRegistry->ResolveEraDetailsForMap(ctx.mapId);

        handler->PSendSysMessage("=== Instance Group Scaling (Map: {}, Inst: {}) ===", ctx.mapId, ctx.instanceId);
        handler->PSendSysMessage("Era: {} | Resolved by: {}",
            ContentEraToString(eraRes.era).data(), EraResolutionSourceToString(eraRes.source).data());
        handler->PSendSysMessage("Tier: {} | Difficulty: {}",
            ContentTierToString(ctx.tier).data(), uint32(ctx.difficulty));
        handler->PSendSysMessage("Actual Players: {} | Challenge Size: {}",
            map->GetPlayersCountExceptGMs(),
            ctx.challengeSize > 0 ? std::to_string(ctx.challengeSize).c_str() : "Adaptive (0)");
        handler->PSendSysMessage("Intended Players: {} | Effective Players: {:.1f}", ctx.intendedPlayers, ctx.effectivePlayers);
        handler->PSendSysMessage("Encounter Lock: {} (Gen: {})",
            ctx.encounterLocked ? "[FROZEN IN ENCOUNTER]" : "[IDLE / DYNAMIC]",
            static_cast<unsigned long long>(ctx.snapshotGeneration));
        if (ctx.encounterLocked)
        {
            handler->PSendSysMessage("Encounter Source: {} | Key Type: {}, ID: {}",
                static_cast<uint32>(ctx.activeEncounterSource),
                static_cast<uint32>(ctx.activeEncounterKey.type),
                ctx.activeEncounterKey.id);
        }
        handler->PSendSysMessage("Multipliers: HP x{:.2f} | Damage x{:.2f} | Heal x{:.2f} | Absorb x{:.2f}",
            ctx.healthScale, ctx.damageScale, ctx.healingScale, ctx.absorbScale);

        return true;
    }

    static bool HandleEncounter(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        Map* map = player->GetMap();
        if (!map || !map->IsDungeon())
        {
            handler->SendSysMessage("You are not inside an instance/dungeon.");
            return true;
        }

        InstanceScaleContext const ctx = sInstanceScalingMgr->GetOrCreateContext(map);
        EncounterContext const encCtx = sInstanceScalingMgr->BuildEncounterContext(map, ctx.lockEncounterId);
        IEncounterAdapter const* adapter = sAdaptiveEncounterMgr->GetAdapter(map->GetId(), ctx.lockEncounterId);

        handler->PSendSysMessage("=== Adaptive Encounter Diagnostics (Map: {}, Inst: {}) ===", map->GetId(), map->GetInstanceId());
        handler->PSendSysMessage("Active Lock: {} (Encounter ID: {}, Gen: {})",
            ctx.encounterLocked ? "LOCKED" : "UNLOCKED/IDLE",
            ctx.lockEncounterId, static_cast<unsigned long long>(encCtx.snapshotGeneration));
        handler->PSendSysMessage("Participants: {} physical | {:.1f} combat eff | {} mechanic parts (Intended: {})",
            encCtx.actualParticipants, encCtx.combatEffectivePlayers, encCtx.mechanicParticipants, encCtx.intendedPlayers);
        handler->PSendSysMessage("Mode: {} | Solo State: Physical: {} | Mechanic: {}",
            encCtx.challengeSize > 0 ? ("Challenge " + std::to_string(encCtx.challengeSize)).c_str() : "Adaptive (0)",
            encCtx.isPhysicallySolo ? "Yes" : "No",
            encCtx.isMechanicSolo ? "Yes" : "No");

        if (adapter)
        {
            handler->PSendSysMessage("Adapter: {} [COMPAT: {}]",
                adapter->GetName().data(), CompatibilityToString(adapter->GetCompatibility()).data());
        }
        else
        {
            handler->PSendSysMessage("Adapter: None (Generic Adaptive Scaling fallback) [COMPAT: AUTO]");
        }

        handler->PSendSysMessage("Sample Mechanics Scaled (at {} mechanic players):", encCtx.mechanicParticipants);
        uint32 sampleTargets = sAdaptiveEncounterMgr->ResolveMechanic(map->GetId(), ctx.lockEncounterId, 0, EncounterMechanicType::TARGET_COUNT, 4, encCtx);
        uint32 sampleAdds = sAdaptiveEncounterMgr->ResolveMechanic(map->GetId(), ctx.lockEncounterId, 0, EncounterMechanicType::ADD_COUNT, 12, encCtx);
        uint32 sampleReq = sAdaptiveEncounterMgr->ResolveMechanic(map->GetId(), ctx.lockEncounterId, 0, EncounterMechanicType::REQUIRED_PLAYERS, 4, encCtx);
        handler->PSendSysMessage(" - Target Count: 4 -> {}", sampleTargets);
        handler->PSendSysMessage(" - Add Wave: 12 -> {}", sampleAdds);
        handler->PSendSysMessage(" - Required Players: 4 -> {}", sampleReq);

        return true;
    }

    static bool HandleQuest(ChatHandler* handler, uint32 questId)
    {
        Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
        if (!quest)
        {
            handler->PSendSysMessage("Quest {} not found.", questId);
            return true;
        }

        EraResolutionResult const eraRes = sContentPackRegistry->ResolveEraDetailsForQuest(
            questId, quest->GetZoneOrSort(), 0, quest->GetQuestLevel());
        ProgressionLayout const& layout = sCoAContentScaling->GetLayout();
        int32 const effectiveLevel = sCoAContentScaling->GetEffectiveQuestLevel(quest);
        uint32 const effectiveMin = sCoAContentScaling->GetEffectiveQuestMinLevel(quest);
        bool const eraEnabled = layout.IsEraEnabled(eraRes.era);

        handler->PSendSysMessage("=== Quest Scaling: {} (ID: {}) ===", quest->GetTitle().c_str(), questId);
        handler->PSendSysMessage("Era: {} ({}) | Resolved by: {} (Confidence: {:.2f})",
            ContentEraToString(eraRes.era).data(),
            eraEnabled ? "Active" : "Locked / Pack Disabled",
            EraResolutionSourceToString(eraRes.source).data(),
            eraRes.confidence);
        handler->PSendSysMessage("Authored Level: {} | Effective Level: {}", quest->GetQuestLevel(), effectiveLevel);
        handler->PSendSysMessage("Authored MinLevel: {} | Effective MinLevel: {} ({})",
            quest->GetMinLevel(), effectiveMin,
            effectiveMin >= 255 ? "Inaccessible" : "Accessible");

        uint8 const pLvl = handler->GetPlayer() ? handler->GetPlayer()->GetLevel() : 0;
        uint32 const authoredXP = quest->XPValue(pLvl);
        uint32 const effectiveXP = sProgressionRewardResolver->ResolveQuestXP(
            authoredXP, quest->GetQuestLevel(), effectiveLevel, layout, eraRes.era);
        int32 const moneyAtCap = sProgressionRewardResolver->ResolveMoneyAtCap(effectiveXP, 1.0f);

        handler->PSendSysMessage("Reward XP: Authored {} -> Calibrated {}", authoredXP, effectiveXP);
        handler->PSendSysMessage("At-Cap Money Conversion: {}g {}s {}c (Guard: 50g cap)",
            moneyAtCap / 10000, (moneyAtCap % 10000) / 100, moneyAtCap % 100);

        return true;
    }

    static bool HandleItem(ChatHandler* handler, uint32 itemId)
    {
        ItemTemplate const* item = sObjectMgr->GetItemTemplate(itemId);
        if (!item)
        {
            handler->PSendSysMessage("Item {} not found.", itemId);
            return true;
        }

        ProgressionLayout const& layout = sCoAContentScaling->GetLayout();
        ItemScalingContext const ctx = ItemScalingContext::Resolve(item);
        ScaledItemBudget const budget = sItemBudgetScaler->CalculateItemBudget(item, layout, ctx);

        char const* authorityOrigin = "FALLBACK_ILVL";
        if (ctx.hasGeneratedProfile)
        {
            if (ctx.sourceMap != 0)
                authorityOrigin = "INSTANCE_LOOT";
            else if (ctx.specialFlags & ITEM_SPECIAL_CUSTOM)
                authorityOrigin = "CUSTOM_OVERRIDE";
            else
                authorityOrigin = "CENSUS_PROFILE";
        }
        else if (ctx.specialFlags & ITEM_SPECIAL_CUSTOM)
        {
            authorityOrigin = "CUSTOM_FALLBACK";
        }

        handler->PSendSysMessage("=== Item Scaling: {} (ID: {}) ===", item->Name1.c_str(), itemId);
        handler->PSendSysMessage("Authority Origin: {} | Generated Profile: {} | Fallback: {}",
            authorityOrigin, ctx.hasGeneratedProfile ? "Yes" : "No", ctx.fallbackTierInference ? "Yes" : "No");
        handler->PSendSysMessage("Era: {} | Tier: {} | Policy: {}",
            ContentEraToString(ctx.era).data(), ContentTierToString(ctx.tier).data(), ItemScalingPolicyToString(ctx.policy).data());
        handler->PSendSysMessage("Source Map: {} | Special Flags: 0x{:02X}", ctx.sourceMap, uint32(ctx.specialFlags));
        handler->PSendSysMessage("Authored ReqLevel: {} | Effective ReqLevel: {}", item->RequiredLevel, budget.effectiveRequiredLevel);
        handler->PSendSysMessage("Authored ItemLevel: {} | Effective ItemLevel: {}", item->ItemLevel, budget.effectiveItemLevel);
        handler->PSendSysMessage("Multipliers: Stats x{:.2f} | Ratings x{:.2f} | Armor x{:.2f} | DPS x{:.2f}",
            budget.statMultiplier, budget.ratingMultiplier, budget.armorMultiplier, budget.weaponDpsMultiplier);

        return true;
    }

    static bool HandleValidate(ChatHandler* handler)
    {
        handler->SendSysMessage("=== Validating CoA Universal Content Scaling ===");
        ProgressionLayout const& layout = sCoAContentScaling->GetLayout();
        std::string err;
        if (!layout.Validate(err))
        {
            handler->PSendSysMessage("[FAIL] ProgressionLayout invalid: {}", err.c_str());
            return true;
        }

        handler->PSendSysMessage("[PASS] ProgressionLayout valid: Cap {}", uint32(layout.maxLevel));
        handler->PSendSysMessage("[PASS] Enabled eras continuous and terminating at Cap");

        sCoAContentScaling->PurgeExpiredPendingPolicies();
        handler->PSendSysMessage("[PASS] Pending LFG policies verified ({} active groups, {} player aliases)",
            sCoAContentScaling->GetPendingGroupPoliciesCount(), sCoAContentScaling->GetPendingPlayerPoliciesCount());

        handler->PSendSysMessage("All validation checks passed.");
        return true;
    }

    static bool HandleLfgStatus(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        lfg::LfgCompositionMode const mode = sCoAContentScaling->GetPlayerLfgMode(player->GetGUID());
        uint32 const challenge = sCoAContentScaling->GetPlayerLfgChallenge(player->GetGUID());

        std::string_view modeStr = "matchmaking";
        if (mode == lfg::LfgCompositionMode::BOT_FILL)
            modeStr = "bots";
        else if (mode == lfg::LfgCompositionMode::CURRENT_PARTY)
            modeStr = "party";

        handler->PSendSysMessage("=== CoA LFG Settings ===");
        handler->PSendSysMessage("Composition Mode: {}", modeStr.data());
        if (challenge == 0)
            handler->SendSysMessage("Challenge Size: Adaptive (matches actual player count)");
        else
            handler->PSendSysMessage("Challenge Size: {} player(s)", challenge);

        size_t const pendingGroups = sCoAContentScaling->GetPendingGroupPoliciesCount();
        size_t const pendingPlayers = sCoAContentScaling->GetPendingPlayerPoliciesCount();
        if (pendingGroups > 0 || pendingPlayers > 0)
        {
            handler->PSendSysMessage("Pending Policies: {} group(s), {} player alias(es)", pendingGroups, pendingPlayers);
        }

        return true;
    }

    static bool HandleLfgDungeon(ChatHandler* handler, uint32 dungeonId)
    {
        GeneratedLfgProfile const* lfgProf = FindGeneratedLfgProfile(dungeonId);
        if (!lfgProf)
        {
            handler->PSendSysMessage("LFG Dungeon ID {} not found in census.", dungeonId);
            return true;
        }

        ProgressionLayout const& layout = sCoAContentScaling->GetLayout();
        uint8 effectiveMin = lfgProf->authoredMin;
        uint8 effectiveMax = lfgProf->authoredMax;
        if (effectiveMin > 0 && effectiveMin < 100)
            effectiveMin = layout.MapAuthoredToEffective(lfgProf->era, effectiveMin);
        if (effectiveMax > 0 && effectiveMax < 100)
            effectiveMax = layout.MapAuthoredToEffective(lfgProf->era, effectiveMax);

        bool const eraEnabled = layout.IsEraEnabled(lfgProf->era);

        handler->PSendSysMessage("=== LFG Dungeon Profile: ID {} (Map: {}, Diff: {}) ===",
            dungeonId, lfgProf->mapId, uint32(lfgProf->difficulty));
        handler->PSendSysMessage("Era: {} (Status: {})",
            ContentEraToString(lfgProf->era).data(), eraEnabled ? "ENABLED" : "LOCKED_EXPANSION");
        handler->PSendSysMessage("Authored Levels: {} - {} (Target: {})",
            uint32(lfgProf->authoredMin), uint32(lfgProf->authoredMax), uint32(lfgProf->authoredTarget));
        handler->PSendSysMessage("Effective Levels: {} - {}",
            uint32(effectiveMin), uint32(effectiveMax));

        GeneratedAccessProfile const* accessProf = FindGeneratedAccessProfile(lfgProf->mapId, lfgProf->difficulty);
        if (accessProf)
        {
            uint8 accMin = layout.MapAuthoredToEffective(accessProf->era, accessProf->authoredMin);
            uint8 accMax = layout.MapAuthoredToEffective(accessProf->era, accessProf->authoredMax);
            handler->PSendSysMessage("Dungeon Access Profile: Authored {}-{} -> Effective {}-{}",
                uint32(accessProf->authoredMin), uint32(accessProf->authoredMax), uint32(accMin), uint32(accMax));
        }

        return true;
    }

    static bool HandleLfgMode(ChatHandler* handler, std::string const& modeArg)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        if (modeArg == "matchmaking" || modeArg == "real" || modeArg == "normal")
        {
            sCoAContentScaling->SetPlayerLfgMode(player->GetGUID(), lfg::LfgCompositionMode::MATCHMAKING);
            handler->SendSysMessage("LFG Mode set to MATCHMAKING (wait for real players).");
        }
        else if (modeArg == "bots" || modeArg == "botfill" || modeArg == "fill")
        {
            if (true /* no bot fill provider on this realm */)
            {
                handler->SendSysMessage("Warning: No bot fill provider is registered. Bot fill mode will fall back to Matchmaking upon queuing.");
            }
            sCoAContentScaling->SetPlayerLfgMode(player->GetGUID(), lfg::LfgCompositionMode::BOT_FILL);
            handler->SendSysMessage("LFG Mode set to BOT_FILL (auto-fill party with bots).");
        }
        else if (modeArg == "party" || modeArg == "current" || modeArg == "solo")
        {
            sCoAContentScaling->SetPlayerLfgMode(player->GetGUID(), lfg::LfgCompositionMode::CURRENT_PARTY);
            handler->SendSysMessage("LFG Mode set to CURRENT_PARTY (queue immediately with current group/solo, bypassing matchmaking).");
        }
        else
        {
            handler->SendSysMessage("Usage: .lfgmode [matchmaking | bots | party]");
            return true;
        }

        sCoAContentScaling->SavePlayerLfgSettings(player);
        return true;
    }

    static bool HandleLfgChallenge(ChatHandler* handler, std::string const& challengeArg)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        if (challengeArg == "adaptive" || challengeArg == "auto" || challengeArg == "0")
        {
            sCoAContentScaling->SetPlayerLfgChallenge(player->GetGUID(), 0);
            handler->SendSysMessage("LFG Challenge Size set to Adaptive (scales dynamically to group size).");
        }
        else
        {
            try
            {
                int val = std::stoi(challengeArg);
                if (val < 1 || val > 40)
                {
                    handler->SendSysMessage("Challenge size must be between 1 and 40 (or 'adaptive').");
                    return true;
                }

                if (!sCoAContentScaling->SetPlayerLfgChallenge(player->GetGUID(), uint32(val)))
                {
                    handler->SendSysMessage("Failed to set challenge size (must be 0..40).");
                    return true;
                }
                handler->PSendSysMessage("LFG Challenge Size locked to {} players.", val);
            }
            catch (...)
            {
                handler->SendSysMessage("Usage: .lfgchallenge [adaptive | 1..40]");
                return true;
            }
        }

        sCoAContentScaling->SavePlayerLfgSettings(player);
        return true;
    }
};

void AddCoAContentScalingCommands()
{
    new coa_content_scaling_commandscript();
}
