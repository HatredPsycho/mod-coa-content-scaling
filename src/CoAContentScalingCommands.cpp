/*
 * CoA Universal Content Scaling
 * CoAContentScalingCommands: Diagnostic and administration commands (.coascale).
 */

#include "AdaptiveEncounterAPI.h"
#include "Chat.h"
#include "CoAContentScaling.h"
#include "CombatBudgetProfile.h"
#include "CommandScript.h"
#include "ContentEraResolver.h"
#include "Creature.h"
#include "InstanceProfile.h"
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
        static ChatCommandTable const coaScaleCommandTable =
        {
            { "status",      HandleStatus,      SEC_ADMINISTRATOR, Console::Yes },
            { "eras",        HandleEras,        SEC_ADMINISTRATOR, Console::Yes },
            { "layout",      HandleLayout,      SEC_ADMINISTRATOR, Console::Yes },
            { "progression", HandleProgression, SEC_PLAYER,        Console::No },
            { "creature",    HandleCreature,    SEC_ADMINISTRATOR, Console::No },
            { "instance",    HandleInstance,    SEC_ADMINISTRATOR, Console::No },
            { "encounter",   HandleEncounter,   SEC_ADMINISTRATOR, Console::No },
            { "quest",       HandleQuest,       SEC_ADMINISTRATOR, Console::Yes },
            { "item",        HandleItem,        SEC_ADMINISTRATOR, Console::Yes },
            { "validate",    HandleValidate,    SEC_ADMINISTRATOR, Console::Yes },
        };

        static ChatCommandTable const commandTable =
        {
            { "coascale", coaScaleCommandTable },
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
        handler->PSendSysMessage("Creature hook calls: {} | scaled: {}",
            sCoAContentScaling->GetCreatureHookCalls(), sCoAContentScaling->GetCreatureScaleApplied());
        handler->PSendSysMessage("Last scaled: entry {} -> max health {}",
            sCoAContentScaling->GetLastScaledEntry(), sCoAContentScaling->GetLastScaledHealth());
        return true;
    }

    static bool HandleEras(ChatHandler* handler)
    {
        handler->PSendSysMessage("=== CoA Content Era Sources ===");
        handler->PSendSysMessage("Map expansions: {} | Area maps: {} | Classic areas on Outland: {}",
            sContentEraResolver->GetMapExpansionCount(), sContentEraResolver->GetAreaMapCount(),
            sContentEraResolver->GetClassicAreaCount());
        handler->PSendSysMessage("Zone eras from the journey tables: {}", sContentEraResolver->GetZoneEraCount());
        handler->PSendSysMessage("PvE instance profiles: {}", sInstanceProfileRegistry->GetProfileCount());
        return true;
    }

    static bool HandleLayout(ChatHandler* handler)
    {
        ProgressionLayout const& layout = sCoAContentScaling->GetLayout();
        handler->PSendSysMessage("=== CoA Progression Layout ===");
        handler->PSendSysMessage("MaxPlayerLevel: {}", uint32(layout.maxLevel));
        if (layout.journey)
        {
            journey::Shape const& shape = layout.journeyRules.GetShape();
            handler->PSendSysMessage("Preset: Journey | Endgame: {} (+{} for bosses)", shape.endgame, shape.bossLevels);
        }
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

        CreatureScaleContext ctx;
        CalculatedCombatBudget const budget = sCoAContentScaling->CalculateCreatureBudget(cinfo, target, &ctx);

        CreatureScaleContext fullGroupCtx = ctx;
        fullGroupCtx.groupHealthScale = 1.0f;
        fullGroupCtx.groupDamageScale = 1.0f;
        CalculatedCombatBudget const fullGroup = sCombatBudgetProfile->CalculateBudget(cinfo, fullGroupCtx);
        EraResolutionResult const eraRes = sContentEraResolver->ResolveEraDetailsForCreature(
            cinfo->Entry, target->GetMapId(), target->GetAreaId(), cinfo->expansion, cinfo->maxlevel);

        handler->PSendSysMessage("=== Creature Scaling: {} (Entry: {}) ===", cinfo->Name.c_str(), cinfo->Entry);
        handler->PSendSysMessage("Era: {} | Resolved by: {} (Confidence: {:.2f})",
            ContentEraToString(eraRes.era).data(), EraResolutionSourceToString(eraRes.source).data(), eraRes.confidence);
        handler->PSendSysMessage("Authored Level: {} | Current Level: {}", uint32(ctx.authoredLevel), uint32(ctx.effectiveLevel));
        handler->PSendSysMessage("Tier: {} | HealthMod: {:.2f} | DamageMod: {:.2f}",
            ContentTierToString(ctx.tier).data(), cinfo->ModHealth, cinfo->DamageModifier);
        handler->PSendSysMessage("Group Scale: health x{:.4f} | damage x{:.4f}",
            ctx.groupHealthScale, ctx.groupDamageScale);
        handler->PSendSysMessage("Scaled Health: {} | Mana: {} | Armor: {:.0f} | AP: {}",
            budget.health, budget.mana, budget.armor, budget.attackPower);
        handler->PSendSysMessage("At full group: health {} (this is not what the creature carries)",
            fullGroup.health);
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
        EraResolutionResult const eraRes = sContentEraResolver->ResolveEraDetailsForMap(ctx.mapId);

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

        EraResolutionResult const eraRes = sContentEraResolver->ResolveEraDetailsForQuest(
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

        ItemScalingContext const ctx = ItemScalingContext::Resolve(item);

        char const* const authorityOrigin = (ctx.specialFlags & ITEM_SPECIAL_CUSTOM) ? "CUSTOM_RANGE" : "FALLBACK_ILVL";

        handler->PSendSysMessage("=== Item Scaling: {} (ID: {}) ===", item->Name1.c_str(), itemId);
        handler->PSendSysMessage("Authority Origin: {} | Fallback: {}",
            authorityOrigin, ctx.fallbackTierInference ? "Yes" : "No");
        handler->PSendSysMessage("Era: {} | Tier: {} | Policy: {}",
            ContentEraToString(ctx.era).data(), ContentTierToString(ctx.tier).data(), ItemScalingPolicyToString(ctx.policy).data());
        handler->PSendSysMessage("Source Map: {} | Special Flags: 0x{:02X}", ctx.sourceMap, uint32(ctx.specialFlags));

        // The rewrite is in place, so once it has run the template no longer holds what the item was
        // authored as and recalculating from it would report a cut of x1.00 for every rewritten item.
        if (AppliedItemScaling const* applied = sItemBudgetScaler->FindAppliedScaling(itemId))
        {
            handler->PSendSysMessage("Rewritten at startup.");
            handler->PSendSysMessage("Authored ReqLevel: {} | Effective ReqLevel: {}",
                applied->authoredRequiredLevel, applied->effectiveRequiredLevel);
            handler->PSendSysMessage("Authored ItemLevel: {} | Effective ItemLevel: {}",
                applied->authoredItemLevel, applied->effectiveItemLevel);
            handler->PSendSysMessage("Multipliers: Stats x{:.2f} | Ratings x{:.2f} (armor, weapon dps and spell power follow stats)",
                applied->statMultiplier, applied->ratingMultiplier);
            return true;
        }

        ProgressionLayout const& layout = sCoAContentScaling->GetLayout();
        ScaledItemBudget const budget = sItemBudgetScaler->CalculateItemBudget(item, layout, ctx);

        handler->PSendSysMessage(sItemBudgetScaler->AreItemsScaled() ?
            "Left as authored at startup; the figures below are what it would be cut to now." :
            "Items have not been rewritten yet; the figures below are what it would be cut to.");
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

        handler->PSendSysMessage("All validation checks passed.");
        return true;
    }
};

void AddCoAContentScalingCommands()
{
    new coa_content_scaling_commandscript();
}
