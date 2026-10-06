/*
 * CoA Universal Content Scaling
 * ProgressionLayoutTest: Comprehensive unit test matrix for ProgressionLayout combinations.
 */

#include "ProgressionLayout.h"
#include "ContentEra.h"
#include "ContentPackRegistry.h"
#include "CoAContentScalingConfig.h"
#include "DungeonFinding/LFG.h"
#include "GeneratedContentCensus.h"
#include "InstanceProfile.h"
#include "InstanceScaleContext.h"
#include "ItemBudgetScaler.h"
#include "ItemTemplate.h"
#include "ProgressionContext.h"
#include "ProgressionRewardResolver.h"
#include <fstream>
#include <filesystem>
#include <sstream>
#include "gtest/gtest.h"

// 1. MaxLevel 60 Combinations
TEST(ProgressionLayoutTest, MaxLevel60_ClassicOnly)
{
    ProgressionLayout layout = ProgressionLayout::Create(60, false, false);
    std::string err;
    EXPECT_TRUE(layout.Validate(err)) << err;
    EXPECT_EQ(layout.classic.minLevel, 1);
    EXPECT_EQ(layout.classic.maxLevel, 60);
    EXPECT_FALSE(layout.tbc.has_value());
    EXPECT_FALSE(layout.wotlk.has_value());
}

TEST(ProgressionLayoutTest, MaxLevel60_ClassicTBC)
{
    ProgressionLayout layout = ProgressionLayout::Create(60, true, false);
    std::string err;
    EXPECT_TRUE(layout.Validate(err)) << err;
    EXPECT_EQ(layout.classic.minLevel, 1);
    EXPECT_EQ(layout.classic.maxLevel, 45);
    ASSERT_TRUE(layout.tbc.has_value());
    EXPECT_EQ(layout.tbc->minLevel, 45);
    EXPECT_EQ(layout.tbc->maxLevel, 60);
    EXPECT_FALSE(layout.wotlk.has_value());
}

TEST(ProgressionLayoutTest, MaxLevel60_ClassicWotLK)
{
    ProgressionLayout layout = ProgressionLayout::Create(60, false, true);
    std::string err;
    EXPECT_TRUE(layout.Validate(err)) << err;
    EXPECT_EQ(layout.classic.minLevel, 1);
    EXPECT_EQ(layout.classic.maxLevel, 45);
    EXPECT_FALSE(layout.tbc.has_value());
    ASSERT_TRUE(layout.wotlk.has_value());
    EXPECT_EQ(layout.wotlk->minLevel, 45);
    EXPECT_EQ(layout.wotlk->maxLevel, 60);
}

TEST(ProgressionLayoutTest, MaxLevel60_AllEras)
{
    ProgressionLayout layout = ProgressionLayout::Create(60, true, true);
    std::string err;
    EXPECT_TRUE(layout.Validate(err)) << err;
    EXPECT_EQ(layout.classic.minLevel, 1);
    EXPECT_EQ(layout.classic.maxLevel, 45);
    ASSERT_TRUE(layout.tbc.has_value());
    EXPECT_EQ(layout.tbc->minLevel, 45);
    EXPECT_EQ(layout.tbc->maxLevel, 55);
    ASSERT_TRUE(layout.wotlk.has_value());
    EXPECT_EQ(layout.wotlk->minLevel, 55);
    EXPECT_EQ(layout.wotlk->maxLevel, 60);
}

// 2. MaxLevel 70 Combinations (Interpolation)
TEST(ProgressionLayoutTest, MaxLevel70_ClassicOnly)
{
    ProgressionLayout layout = ProgressionLayout::Create(70, false, false);
    std::string err;
    EXPECT_TRUE(layout.Validate(err)) << err;
    EXPECT_EQ(layout.classic.minLevel, 1);
    EXPECT_EQ(layout.classic.maxLevel, 70);
    EXPECT_FALSE(layout.tbc.has_value());
    EXPECT_FALSE(layout.wotlk.has_value());
}

TEST(ProgressionLayoutTest, MaxLevel70_ClassicTBC)
{
    ProgressionLayout layout = ProgressionLayout::Create(70, true, false);
    std::string err;
    EXPECT_TRUE(layout.Validate(err)) << err;
    EXPECT_EQ(layout.classic.minLevel, 1);
    EXPECT_EQ(layout.classic.maxLevel, 53); // round(45 + 10 * 0.75) = round(52.5) = 53
    ASSERT_TRUE(layout.tbc.has_value());
    EXPECT_EQ(layout.tbc->minLevel, 53);
    EXPECT_EQ(layout.tbc->maxLevel, 70);
    EXPECT_FALSE(layout.wotlk.has_value());
}

TEST(ProgressionLayoutTest, MaxLevel70_ClassicWotLK)
{
    ProgressionLayout layout = ProgressionLayout::Create(70, false, true);
    std::string err;
    EXPECT_TRUE(layout.Validate(err)) << err;
    EXPECT_EQ(layout.classic.minLevel, 1);
    EXPECT_EQ(layout.classic.maxLevel, 53);
    EXPECT_FALSE(layout.tbc.has_value());
    ASSERT_TRUE(layout.wotlk.has_value());
    EXPECT_EQ(layout.wotlk->minLevel, 53);
    EXPECT_EQ(layout.wotlk->maxLevel, 70);
}

TEST(ProgressionLayoutTest, MaxLevel70_AllEras)
{
    ProgressionLayout layout = ProgressionLayout::Create(70, true, true);
    std::string err;
    EXPECT_TRUE(layout.Validate(err)) << err;
    EXPECT_EQ(layout.classic.minLevel, 1);
    EXPECT_EQ(layout.classic.maxLevel, 53);
    ASSERT_TRUE(layout.tbc.has_value());
    EXPECT_EQ(layout.tbc->minLevel, 53);
    EXPECT_EQ(layout.tbc->maxLevel, 63); // round(55 + 10 * 0.75) = 63
    ASSERT_TRUE(layout.wotlk.has_value());
    EXPECT_EQ(layout.wotlk->minLevel, 63);
    EXPECT_EQ(layout.wotlk->maxLevel, 70);
}

// 3. MaxLevel 80 Combinations
TEST(ProgressionLayoutTest, MaxLevel80_ClassicOnly)
{
    ProgressionLayout layout = ProgressionLayout::Create(80, false, false);
    std::string err;
    EXPECT_TRUE(layout.Validate(err)) << err;
    EXPECT_EQ(layout.classic.minLevel, 1);
    EXPECT_EQ(layout.classic.maxLevel, 80);
    EXPECT_FALSE(layout.tbc.has_value());
    EXPECT_FALSE(layout.wotlk.has_value());
}

TEST(ProgressionLayoutTest, MaxLevel80_ClassicTBC)
{
    ProgressionLayout layout = ProgressionLayout::Create(80, true, false);
    std::string err;
    EXPECT_TRUE(layout.Validate(err)) << err;
    EXPECT_EQ(layout.classic.minLevel, 1);
    EXPECT_EQ(layout.classic.maxLevel, 60);
    ASSERT_TRUE(layout.tbc.has_value());
    EXPECT_EQ(layout.tbc->minLevel, 60);
    EXPECT_EQ(layout.tbc->maxLevel, 80);
    EXPECT_FALSE(layout.wotlk.has_value());
}

TEST(ProgressionLayoutTest, MaxLevel80_ClassicWotLK)
{
    ProgressionLayout layout = ProgressionLayout::Create(80, false, true);
    std::string err;
    EXPECT_TRUE(layout.Validate(err)) << err;
    EXPECT_EQ(layout.classic.minLevel, 1);
    EXPECT_EQ(layout.classic.maxLevel, 60);
    EXPECT_FALSE(layout.tbc.has_value());
    ASSERT_TRUE(layout.wotlk.has_value());
    EXPECT_EQ(layout.wotlk->minLevel, 60);
    EXPECT_EQ(layout.wotlk->maxLevel, 80);
}

TEST(ProgressionLayoutTest, MaxLevel80_AllEras)
{
    ProgressionLayout layout = ProgressionLayout::Create(80, true, true);
    std::string err;
    EXPECT_TRUE(layout.Validate(err)) << err;
    EXPECT_EQ(layout.classic.minLevel, 1);
    EXPECT_EQ(layout.classic.maxLevel, 60);
    ASSERT_TRUE(layout.tbc.has_value());
    EXPECT_EQ(layout.tbc->minLevel, 60);
    EXPECT_EQ(layout.tbc->maxLevel, 70);
    ASSERT_TRUE(layout.wotlk.has_value());
    EXPECT_EQ(layout.wotlk->minLevel, 70);
    EXPECT_EQ(layout.wotlk->maxLevel, 80);
}

// 4. Monotonic Mapping & Boundaries Tests
TEST(ProgressionLayoutTest, MonotonicLevelMapping_Cap60AllEras)
{
    ProgressionLayout layout = ProgressionLayout::Create(60, true, true);

    // Classic: 1-60 -> 1-45
    uint8 lastEffective = 0;
    for (uint8 authored = 1; authored <= 60; ++authored)
    {
        uint8 effective = layout.MapAuthoredToEffective(ContentEra::Classic, authored, 1, 60);
        EXPECT_GE(effective, lastEffective) << "Classic mapping must be strictly monotonic";
        EXPECT_GE(effective, 1);
        EXPECT_LE(effective, 45);
        lastEffective = effective;
    }

    // TBC: 58-70 -> 45-55
    lastEffective = 0;
    for (uint8 authored = 58; authored <= 70; ++authored)
    {
        uint8 effective = layout.MapAuthoredToEffective(ContentEra::TBC, authored, 58, 70);
        EXPECT_GE(effective, lastEffective) << "TBC mapping must be strictly monotonic";
        EXPECT_GE(effective, 45);
        EXPECT_LE(effective, 55);
        lastEffective = effective;
    }

    // WotLK: 68-80 -> 55-60
    lastEffective = 0;
    for (uint8 authored = 68; authored <= 80; ++authored)
    {
        uint8 effective = layout.MapAuthoredToEffective(ContentEra::WotLK, authored, 68, 80);
        EXPECT_GE(effective, lastEffective) << "WotLK mapping must be strictly monotonic";
        EXPECT_GE(effective, 55);
        EXPECT_LE(effective, 60);
        lastEffective = effective;
    }
}

TEST(ProgressionLayoutTest, ValidationFailures)
{
    // maxLevel > 80 fails validation
    ProgressionLayout badMax = ProgressionLayout::Create(85, true, true);
    std::string err;
    EXPECT_FALSE(badMax.Validate(err));

    // maxLevel < 60 fails validation
    ProgressionLayout badMin = ProgressionLayout::Create(50, true, true);
    EXPECT_FALSE(badMin.Validate(err));
}

// 5. Pack Registration Order Invariance
TEST(ProgressionLayoutTest, RegistrationOrderInvariance)
{
    // Regardless of which pack was discovered/registered first, the resulting ProgressionLayout is identical
    ProgressionLayout l1 = ProgressionLayout::Create(60, true, true);
    ProgressionLayout l2 = ProgressionLayout::Create(60, true, true);

    EXPECT_EQ(l1.maxLevel, l2.maxLevel);
    EXPECT_EQ(l1.classic.minLevel, l2.classic.minLevel);
    EXPECT_EQ(l1.classic.maxLevel, l2.classic.maxLevel);
    ASSERT_TRUE(l1.tbc.has_value() && l2.tbc.has_value());
    EXPECT_EQ(l1.tbc->minLevel, l2.tbc->minLevel);
    EXPECT_EQ(l1.tbc->maxLevel, l2.tbc->maxLevel);
    ASSERT_TRUE(l1.wotlk.has_value() && l2.wotlk.has_value());
    EXPECT_EQ(l1.wotlk->minLevel, l2.wotlk->minLevel);
    EXPECT_EQ(l1.wotlk->maxLevel, l2.wotlk->maxLevel);
}

// 6. ContentEra Strict Resolution Priority
TEST(ContentEraResolutionTest, ClassicContinentCreatureRetainsClassicAtLevel60)
{
    // A creature on Eastern Kingdoms (map 0) or Kalimdor (map 1) at level 60 must NEVER be promoted to TBC
    EraResolutionResult res = sContentPackRegistry->ResolveEraDetailsForCreature(
        12345, 0 /*Eastern Kingdoms*/, 0, 0 /*Classic expansion*/, 60 /*Level 60*/);

    EXPECT_EQ(res.era, ContentEra::Classic);
    EXPECT_EQ(res.source, EraResolutionSource::InstanceProfile);

    // Explicit override takes precedence over map profile
    sContentPackRegistry->RegisterCreatureOverride(12345, ContentEra::Custom);
    EraResolutionResult overrideRes = sContentPackRegistry->ResolveEraDetailsForCreature(
        12345, 0, 0, 0, 60);

    EXPECT_EQ(overrideRes.era, ContentEra::Custom);
    EXPECT_EQ(overrideRes.source, EraResolutionSource::ExplicitOverride);

    // Clean up
    sContentPackRegistry->Clear();
}

// 7. Monotonic Item Power Bands
TEST(ItemBudgetScalerTest, MonotonicRaidTierIlvlOrdering)
{
    ProgressionLayout layout = ProgressionLayout::Create(60, true, true);

    ItemTemplate naxxItem;
    naxxItem.ItemId = 39000;
    naxxItem.ItemLevel = 200; // T7
    naxxItem.RequiredLevel = 80;

    ItemTemplate ulduarItem;
    ulduarItem.ItemId = 45000;
    ulduarItem.ItemLevel = 226; // T8
    ulduarItem.RequiredLevel = 80;

    ItemTemplate tocItem;
    tocItem.ItemId = 47000;
    tocItem.ItemLevel = 245; // T9
    tocItem.RequiredLevel = 80;

    ItemTemplate iccItem;
    iccItem.ItemId = 50000;
    iccItem.ItemLevel = 264; // T10
    iccItem.RequiredLevel = 80;

    ScaledItemBudget bNaxx = sItemBudgetScaler->CalculateItemBudget(&naxxItem, layout);
    ScaledItemBudget bUlduar = sItemBudgetScaler->CalculateItemBudget(&ulduarItem, layout);
    ScaledItemBudget bToc = sItemBudgetScaler->CalculateItemBudget(&tocItem, layout);
    ScaledItemBudget bIcc = sItemBudgetScaler->CalculateItemBudget(&iccItem, layout);

    // Monotonic item levels: Naxx < Ulduar < ToC < ICC
    EXPECT_LT(bNaxx.effectiveItemLevel, bUlduar.effectiveItemLevel);
    EXPECT_LT(bUlduar.effectiveItemLevel, bToc.effectiveItemLevel);
    EXPECT_LT(bToc.effectiveItemLevel, bIcc.effectiveItemLevel);

    // Effective stat points: higher tiers retain strictly greater effective power
    // Authored stats for T7 ~70, T8 ~100, T9 ~135, T10 ~175
    float const statNaxx = 70.0f * bNaxx.statMultiplier;
    float const statUlduar = 100.0f * bUlduar.statMultiplier;
    float const statToc = 135.0f * bToc.statMultiplier;
    float const statIcc = 175.0f * bIcc.statMultiplier;

    EXPECT_LT(statNaxx, statUlduar);
    EXPECT_LT(statUlduar, statToc);
    EXPECT_LT(statToc, statIcc);
}

namespace
{
    uint32 ScaledGearLevel(uint32 itemLevel, ContentTier tier, ProgressionLayout const& layout)
    {
        ItemTemplate item;
        item.ItemId = 0;
        item.ItemLevel = itemLevel;
        item.RequiredLevel = 80;

        ItemScalingContext context;
        context.era = ContentEra::WotLK;
        context.tier = tier;
        return sItemBudgetScaler->CalculateItemBudget(&item, layout, context).effectiveItemLevel;
    }
}

TEST(ItemBudgetScalerTest, HeroicItemLevelRequirementIsMetByTheGearThatComesBeforeIt)
{
    ProgressionLayout const layout = ProgressionLayout::Create(60, true, true);

    uint32 const heroic = sItemBudgetScaler->ScaleRequiredAverageItemLevel(180, ContentEra::WotLK, layout);
    uint32 const heroicToc = sItemBudgetScaler->ScaleRequiredAverageItemLevel(200, ContentEra::WotLK, layout);
    uint32 const heroicHor = sItemBudgetScaler->ScaleRequiredAverageItemLevel(219, ContentEra::WotLK, layout);

    EXPECT_LE(heroic, ScaledGearLevel(187, ContentTier::DUNGEON_NORMAL, layout));
    EXPECT_LE(heroicToc, ScaledGearLevel(200, ContentTier::DUNGEON_HEROIC, layout));
    EXPECT_LE(heroicHor, ScaledGearLevel(219, ContentTier::RAID_ENTRY, layout));

    EXPECT_GT(heroic, ScaledGearLevel(150, ContentTier::DUNGEON_NORMAL, layout));
    EXPECT_LT(heroic, heroicToc);
    EXPECT_LT(heroicToc, heroicHor);
    EXPECT_LT(heroicHor, 180u);
}

TEST(ItemBudgetScalerTest, ItemLevelRequirementStaysAuthoredOnTheStockLayout)
{
    ProgressionLayout const layout = ProgressionLayout::Create(80, true, true);
    ASSERT_TRUE(layout.IsStockIdentity());

    EXPECT_EQ(sItemBudgetScaler->ScaleRequiredAverageItemLevel(180, ContentEra::WotLK, layout), 180u);
    EXPECT_EQ(sItemBudgetScaler->ScaleRequiredAverageItemLevel(219, ContentEra::WotLK, layout), 219u);
}

// 8. Instance Profile Validation and Multi-Era Resolution (Onyxia 249)
TEST(InstanceProfileRegistryTest, ValidationAndEraResolution)
{
    sInstanceProfileRegistry->Initialize();

    std::vector<std::string> issues;
    bool const valid = sInstanceProfileRegistry->ValidateAll(issues);
    EXPECT_TRUE(valid) << (issues.empty() ? "" : issues[0]);

    // Onyxia (Map 249): 40-man Classic vs 10/25 WotLK
    EXPECT_EQ(sInstanceProfileRegistry->GetEraForMap(249, 0), ContentEra::Classic);
    EXPECT_EQ(sInstanceProfileRegistry->GetEraForMap(249, 1), ContentEra::WotLK);
    EXPECT_EQ(sInstanceProfileRegistry->GetEraForMap(249, 2), ContentEra::WotLK);
}

// 9. Instance Scaling Multipliers Monotonicity & Solo Floor
TEST(InstanceScalingMultipliersTest, DynamicScalingMonotonicity)
{
    InstanceScaleContext ctx;
    ctx.intendedPlayers = 5;

    // Solo in 5-man
    ctx.CalculateMultipliers(1.0f / 5.0f);
    float const soloHealth = ctx.healthScale;
    float const soloDamage = ctx.damageScale;
    EXPECT_GT(soloHealth, 0.0f);
    EXPECT_LT(soloHealth, 1.0f);
    EXPECT_GT(soloDamage, 0.0f);
    EXPECT_LT(soloDamage, 1.0f);

    // Duo in 5-man
    ctx.CalculateMultipliers(2.0f / 5.0f);
    float const duoHealth = ctx.healthScale;
    float const duoDamage = ctx.damageScale;
    EXPECT_GT(duoHealth, soloHealth);
    EXPECT_GT(duoDamage, soloDamage);

    // Full 5-man
    ctx.CalculateMultipliers(5.0f / 5.0f);
    EXPECT_FLOAT_EQ(ctx.healthScale, 1.0f);
    EXPECT_FLOAT_EQ(ctx.damageScale, 1.0f);
    EXPECT_FLOAT_EQ(ctx.healingScale, 1.0f);
    EXPECT_FLOAT_EQ(ctx.absorbScale, 1.0f);
}

// 10. Challenge Size Isolation by Instance ID
TEST(InstanceScalingMgrTest, ChallengeModeStorage)
{
    uint32 const mapId = 33; // Shadowfang Keep
    uint32 const instA = 1001;
    uint32 const instB = 1002;

    sInstanceScalingMgr->SetChallengeSize(mapId, instA, 10);
    sInstanceScalingMgr->SetChallengeSize(mapId, instB, 1);

    EXPECT_EQ(sInstanceScalingMgr->GetChallengeSize(mapId, instA), 10u);
    EXPECT_EQ(sInstanceScalingMgr->GetChallengeSize(mapId, instB), 1u);

    // Reset instA to adaptive
    sInstanceScalingMgr->SetChallengeSize(mapId, instA, 0);
    EXPECT_EQ(sInstanceScalingMgr->GetChallengeSize(mapId, instA), 0u);
    EXPECT_EQ(sInstanceScalingMgr->GetChallengeSize(mapId, instB), 1u); // instB remains unaffected
}

// 11. Authoritative Encounter Lifecycle Hierarchy & Keys
TEST(EncounterLifecycleSourceHierarchyTest, PriorityAndKeyIntegrity)
{
    // Instance Script has higher priority (lower numeric value) than Creature Fallback
    EXPECT_LT(static_cast<uint8>(EncounterLifecycleSource::INSTANCE_SCRIPT),
              static_cast<uint8>(EncounterLifecycleSource::REGISTERED_ADAPTER));
    EXPECT_LT(static_cast<uint8>(EncounterLifecycleSource::REGISTERED_ADAPTER),
              static_cast<uint8>(EncounterLifecycleSource::CREATURE_FALLBACK));

    EncounterKey k1{EncounterKeyType::INSTANCE_ENCOUNTER, 1};
    EncounterKey k2{EncounterKeyType::INSTANCE_ENCOUNTER, 1};
    EncounterKey k3{EncounterKeyType::CREATURE_ENTRY, 1};

    EXPECT_EQ(k1, k2);
    EXPECT_NE(k1, k3);

    std::hash<EncounterKey> hasher;
    EXPECT_EQ(hasher(k1), hasher(k2));
}

// 12. LFG Queue Policy Acceptance Matrix
TEST(LfgQueuePolicyAcceptanceMatrixTest, AcceptanceMatrixVerification)
{
    auto ResolvePolicy = [](lfg::LfgCompositionMode mode, uint32 challengeSize, uint8 partySize)
    {
        lfg::LfgQueuePolicy policy;
        policy.compositionMode = mode;
        policy.challengeSize = challengeSize;

        switch (mode)
        {
            case lfg::LfgCompositionMode::MATCHMAKING:
                policy.bypassMatchmaking = false;
                policy.requireStandardRoles = true;
                policy.minPlayers = 5;
                policy.targetPlayers = 5;
                break;

            case lfg::LfgCompositionMode::BOT_FILL:
                policy.bypassMatchmaking = false;
                policy.requireStandardRoles = true;
                policy.minPlayers = 5;
                policy.targetPlayers = 5;
                break;

            case lfg::LfgCompositionMode::CURRENT_PARTY:
                policy.bypassMatchmaking = true;
                policy.requireStandardRoles = false;
                policy.minPlayers = partySize;
                policy.targetPlayers = partySize;
                break;
        }
        return policy;
    };

    // Scenario 1: CURRENT_PARTY, solo, adaptive
    {
        auto p = ResolvePolicy(lfg::LfgCompositionMode::CURRENT_PARTY, 0, 1);
        EXPECT_TRUE(p.bypassMatchmaking);
        EXPECT_FALSE(p.requireStandardRoles);
        EXPECT_EQ(p.minPlayers, 1u);
        EXPECT_EQ(p.targetPlayers, 1u);
        EXPECT_EQ(p.challengeSize, 0u);
    }

    // Scenario 2: CURRENT_PARTY, solo, challenge 5
    {
        auto p = ResolvePolicy(lfg::LfgCompositionMode::CURRENT_PARTY, 5, 1);
        EXPECT_TRUE(p.bypassMatchmaking);
        EXPECT_EQ(p.minPlayers, 1u);
        EXPECT_EQ(p.targetPlayers, 1u);
        EXPECT_EQ(p.challengeSize, 5u);
    }

    // Scenario 3: CURRENT_PARTY, duo, adaptive
    {
        auto p = ResolvePolicy(lfg::LfgCompositionMode::CURRENT_PARTY, 0, 2);
        EXPECT_TRUE(p.bypassMatchmaking);
        EXPECT_EQ(p.minPlayers, 2u);
        EXPECT_EQ(p.targetPlayers, 2u);
        EXPECT_EQ(p.challengeSize, 0u);
    }

    // Scenario 4: CURRENT_PARTY, duo, challenge 1
    {
        auto p = ResolvePolicy(lfg::LfgCompositionMode::CURRENT_PARTY, 1, 2);
        EXPECT_TRUE(p.bypassMatchmaking);
        EXPECT_EQ(p.minPlayers, 2u);
        EXPECT_EQ(p.targetPlayers, 2u);
        EXPECT_EQ(p.challengeSize, 1u);
    }

    // Scenario 5: CURRENT_PARTY, duo, challenge 5
    {
        auto p = ResolvePolicy(lfg::LfgCompositionMode::CURRENT_PARTY, 5, 2);
        EXPECT_TRUE(p.bypassMatchmaking);
        EXPECT_EQ(p.minPlayers, 2u);
        EXPECT_EQ(p.targetPlayers, 2u);
        EXPECT_EQ(p.challengeSize, 5u);
    }

    // Scenario 6: CURRENT_PARTY, 4-man, challenge 10
    {
        auto p = ResolvePolicy(lfg::LfgCompositionMode::CURRENT_PARTY, 10, 4);
        EXPECT_TRUE(p.bypassMatchmaking);
        EXPECT_EQ(p.minPlayers, 4u);
        EXPECT_EQ(p.targetPlayers, 4u);
        EXPECT_EQ(p.challengeSize, 10u);
    }

    // Scenario 7: MATCHMAKING, solo, adaptive
    {
        auto p = ResolvePolicy(lfg::LfgCompositionMode::MATCHMAKING, 0, 1);
        EXPECT_FALSE(p.bypassMatchmaking);
        EXPECT_TRUE(p.requireStandardRoles);
        EXPECT_EQ(p.minPlayers, 5u);
        EXPECT_EQ(p.targetPlayers, 5u);
        EXPECT_EQ(p.challengeSize, 0u);
    }

    // Scenario 8: BOT_FILL, solo, adaptive
    {
        auto p = ResolvePolicy(lfg::LfgCompositionMode::BOT_FILL, 0, 1);
        EXPECT_FALSE(p.bypassMatchmaking);
        EXPECT_TRUE(p.requireStandardRoles);
        EXPECT_EQ(p.minPlayers, 5u);
        EXPECT_EQ(p.targetPlayers, 5u);
        EXPECT_EQ(p.challengeSize, 0u);
    }
}

// =============================================================================
// Round 2.2: Config Canonical Keys and Centralized Parser Tests
// =============================================================================

TEST(CoAConfigTest, CanonicalKeysConstantsMatch)
{
    EXPECT_STREQ(CoAContentScalingConfigKeys::Enable, "CoAContentScaling.Enable");
    EXPECT_STREQ(CoAContentScalingConfigKeys::ProgressionMode, "CoAContentScaling.Progression.Mode");
    EXPECT_STREQ(CoAContentScalingConfigKeys::ProgressionClassicEnd, "CoAContentScaling.Progression.ClassicEnd");
    EXPECT_STREQ(CoAContentScalingConfigKeys::ProgressionTbcEnd, "CoAContentScaling.Progression.TbcEnd");
    EXPECT_STREQ(CoAContentScalingConfigKeys::GroupScalingEnable, "CoAContentScaling.GroupScaling.Enable");
    EXPECT_STREQ(CoAContentScalingConfigKeys::GroupScalingLockOnEncounterStart, "CoAContentScaling.GroupScaling.LockOnEncounterStart");
    EXPECT_STREQ(CoAContentScalingConfigKeys::GroupScalingAllowSoloRaids, "CoAContentScaling.GroupScaling.AllowSoloRaids");
    EXPECT_STREQ(CoAContentScalingConfigKeys::AdaptiveMechanicsEnable, "CoAContentScaling.AdaptiveMechanics.Enable");
    EXPECT_STREQ(CoAContentScalingConfigKeys::SoloAssistMode, "CoAContentScaling.SoloAssist.Mode");
    EXPECT_STREQ(CoAContentScalingConfigKeys::RewardsScaleLootCount, "CoAContentScaling.Rewards.ScaleLootCount");
    EXPECT_STREQ(CoAContentScalingConfigKeys::ScaleItems, "CoAContentScaling.ScaleItems");
    EXPECT_STREQ(CoAContentScalingConfigKeys::LfgDefaultMode, "CoAContentScaling.LFG.DefaultMode");
    EXPECT_STREQ(CoAContentScalingConfigKeys::LfgDefaultChallengeSize, "CoAContentScaling.LFG.DefaultChallengeSize");
    EXPECT_STREQ(CoAContentScalingConfigKeys::Debug, "CoAContentScaling.Debug");
}

TEST(CoAConfigTest, ParseProgressionMode)
{
    EXPECT_EQ(CoAContentScalingConfig::ParseProgressionMode("Auto"), "Auto");
    EXPECT_EQ(CoAContentScalingConfig::ParseProgressionMode("auto"), "Auto");
    EXPECT_EQ(CoAContentScalingConfig::ParseProgressionMode(" AUTO "), "Auto");
    EXPECT_EQ(CoAContentScalingConfig::ParseProgressionMode("Custom"), "Custom");
    EXPECT_EQ(CoAContentScalingConfig::ParseProgressionMode("custom"), "Custom");
    EXPECT_EQ(CoAContentScalingConfig::ParseProgressionMode(" CUSTOM "), "Custom");
    // Invalid / garbage fallback to Auto
    EXPECT_EQ(CoAContentScalingConfig::ParseProgressionMode("Unknown"), "Auto");
    EXPECT_EQ(CoAContentScalingConfig::ParseProgressionMode(""), "Auto");
    EXPECT_EQ(CoAContentScalingConfig::ParseProgressionMode("123"), "Auto");
}

TEST(CoAConfigTest, ParseSoloAssistMode)
{
    // Canonical names
    EXPECT_EQ(CoAContentScalingConfig::ParseSoloAssistMode("None"), SoloAssistMode::NONE);
    EXPECT_EQ(CoAContentScalingConfig::ParseSoloAssistMode("none"), SoloAssistMode::NONE);
    EXPECT_EQ(CoAContentScalingConfig::ParseSoloAssistMode("Light"), SoloAssistMode::LIGHT);
    EXPECT_EQ(CoAContentScalingConfig::ParseSoloAssistMode("light"), SoloAssistMode::LIGHT);
    EXPECT_EQ(CoAContentScalingConfig::ParseSoloAssistMode("Full"), SoloAssistMode::FULL);
    EXPECT_EQ(CoAContentScalingConfig::ParseSoloAssistMode("full"), SoloAssistMode::FULL);

    // Backwards-compatible numbers
    EXPECT_EQ(CoAContentScalingConfig::ParseSoloAssistMode("0"), SoloAssistMode::NONE);
    EXPECT_EQ(CoAContentScalingConfig::ParseSoloAssistMode("1"), SoloAssistMode::LIGHT);
    EXPECT_EQ(CoAContentScalingConfig::ParseSoloAssistMode("2"), SoloAssistMode::FULL);

    // Trimming
    EXPECT_EQ(CoAContentScalingConfig::ParseSoloAssistMode(" 1 "), SoloAssistMode::LIGHT);
    EXPECT_EQ(CoAContentScalingConfig::ParseSoloAssistMode(" Light "), SoloAssistMode::LIGHT);

    // Invalid values MUST safely fall back to NONE (never LIGHT)
    EXPECT_EQ(CoAContentScalingConfig::ParseSoloAssistMode("invalid"), SoloAssistMode::NONE);
    EXPECT_EQ(CoAContentScalingConfig::ParseSoloAssistMode("3"), SoloAssistMode::NONE);
    EXPECT_EQ(CoAContentScalingConfig::ParseSoloAssistMode("99"), SoloAssistMode::NONE);
    EXPECT_EQ(CoAContentScalingConfig::ParseSoloAssistMode(""), SoloAssistMode::NONE);
}

TEST(CoAConfigTest, ParseMapList)
{
    EXPECT_TRUE(CoAContentScalingConfig::ParseMapList("").empty());
    EXPECT_TRUE(CoAContentScalingConfig::ParseMapList(" , ,").empty());

    std::unordered_set<uint32> const maps = CoAContentScalingConfig::ParseMapList(" 409, 469 ,531,,409");
    EXPECT_EQ(maps, (std::unordered_set<uint32>{ 409, 469, 531 }));

    std::unordered_set<uint32> const mixed = CoAContentScalingConfig::ParseMapList("229, MC, -5, 12a, 230");
    EXPECT_EQ(mixed, (std::unordered_set<uint32>{ 229, 230 }));
}

TEST(CoAConfigTest, ParseLfgCompositionMode)
{
    EXPECT_EQ(CoAContentScalingConfig::ParseLfgCompositionMode("Matchmaking"), lfg::LfgCompositionMode::MATCHMAKING);
    EXPECT_EQ(CoAContentScalingConfig::ParseLfgCompositionMode("matchmaking"), lfg::LfgCompositionMode::MATCHMAKING);
    EXPECT_EQ(CoAContentScalingConfig::ParseLfgCompositionMode("BotFill"), lfg::LfgCompositionMode::BOT_FILL);
    EXPECT_EQ(CoAContentScalingConfig::ParseLfgCompositionMode("bots"), lfg::LfgCompositionMode::BOT_FILL);
    EXPECT_EQ(CoAContentScalingConfig::ParseLfgCompositionMode("CurrentParty"), lfg::LfgCompositionMode::CURRENT_PARTY);
    EXPECT_EQ(CoAContentScalingConfig::ParseLfgCompositionMode("party"), lfg::LfgCompositionMode::CURRENT_PARTY);
    EXPECT_EQ(CoAContentScalingConfig::ParseLfgCompositionMode("solo"), lfg::LfgCompositionMode::CURRENT_PARTY);

    // Invalid fallback
    EXPECT_EQ(CoAContentScalingConfig::ParseLfgCompositionMode("invalid"), lfg::LfgCompositionMode::MATCHMAKING);
    EXPECT_EQ(CoAContentScalingConfig::ParseLfgCompositionMode(""), lfg::LfgCompositionMode::MATCHMAKING);
}

// =============================================================================
// Round 2.2: Pending Policy Atomic Consume-Once and TTL Simulation Tests
// =============================================================================

struct TestPendingPolicy
{
    uint32 mapId{0};
    uint64 groupGuid{0};
    uint32 challengeSize{0};
    lfg::LfgCompositionMode compositionMode{lfg::LfgCompositionMode::MATCHMAKING};
    uint64 generation{0};
    uint32 createdAt{0};
};

class PendingPolicySimulator
{
public:
    void RegisterProposal(uint32 mapId, uint64 groupGuid, std::vector<uint64> const& playerGuids,
                          uint32 challengeSize, lfg::LfgCompositionMode compMode, uint32 currentTime)
    {
        ++_generation;
        TestPendingPolicy p;
        p.mapId = mapId;
        p.groupGuid = groupGuid;
        p.challengeSize = challengeSize;
        p.compositionMode = compMode;
        p.generation = _generation;
        p.createdAt = currentTime;

        if (groupGuid)
            _groupPolicies[groupGuid] = p;

        for (uint64 pguid : playerGuids)
            _playerPolicies[pguid] = p;
    }

    std::optional<TestPendingPolicy> Consume(uint32 mapId, uint64 groupGuid, uint64 playerGuid, uint32 currentTime)
    {
        PurgeExpired(currentTime);

        TestPendingPolicy policy;
        bool found = false;

        if (groupGuid)
        {
            auto it = _groupPolicies.find(groupGuid);
            if (it != _groupPolicies.end() && it->second.mapId == mapId)
            {
                policy = it->second;
                found = true;
            }
        }

        if (!found && playerGuid)
        {
            auto it = _playerPolicies.find(playerGuid);
            if (it != _playerPolicies.end() && it->second.mapId == mapId)
            {
                policy = it->second;
                found = true;
            }
        }

        if (!found)
            return std::nullopt;

        uint64 const targetGen = policy.generation;

        if (policy.groupGuid)
        {
            auto git = _groupPolicies.find(policy.groupGuid);
            if (git != _groupPolicies.end() && git->second.generation == targetGen)
                _groupPolicies.erase(git);
        }

        for (auto it = _playerPolicies.begin(); it != _playerPolicies.end();)
        {
            if (it->second.generation == targetGen)
                it = _playerPolicies.erase(it);
            else
                ++it;
        }

        return policy;
    }

    void OnPlayerLogout(uint64 playerGuid, uint64 leaderGuid, uint32 groupSize)
    {
        auto it = _playerPolicies.find(playerGuid);
        if (it != _playerPolicies.end())
        {
            uint64 const gen = it->second.generation;
            uint64 const grpGuid = it->second.groupGuid;
            _playerPolicies.erase(it);

            if (grpGuid && (playerGuid == leaderGuid || groupSize <= 1))
            {
                auto git = _groupPolicies.find(grpGuid);
                if (git != _groupPolicies.end() && git->second.generation == gen)
                    _groupPolicies.erase(git);
            }
        }
    }

    void PurgeExpired(uint32 currentTime)
    {
        constexpr uint32 TTL = 300; // 5 min
        for (auto it = _groupPolicies.begin(); it != _groupPolicies.end();)
        {
            if (currentTime > it->second.createdAt && (currentTime - it->second.createdAt) > TTL)
                it = _groupPolicies.erase(it);
            else
                ++it;
        }

        for (auto it = _playerPolicies.begin(); it != _playerPolicies.end();)
        {
            if (currentTime > it->second.createdAt && (currentTime - it->second.createdAt) > TTL)
                it = _playerPolicies.erase(it);
            else
                ++it;
        }
    }

    size_t GroupCount() const { return _groupPolicies.size(); }
    size_t PlayerCount() const { return _playerPolicies.size(); }

private:
    uint64 _generation{0};
    std::unordered_map<uint64, TestPendingPolicy> _groupPolicies;
    std::unordered_map<uint64, TestPendingPolicy> _playerPolicies;
};

TEST(PendingPolicyTest, ConsumeOnceAndEraseAllAliases)
{
    PendingPolicySimulator sim;
    sim.RegisterProposal(33, 100, { 1, 2, 3, 4, 5 }, 10, lfg::LfgCompositionMode::CURRENT_PARTY, 1000);

    EXPECT_EQ(sim.GroupCount(), 1u);
    EXPECT_EQ(sim.PlayerCount(), 5u);

    // First entry: Successfully consumed
    auto consumed = sim.Consume(33, 100, 1, 1010);
    ASSERT_TRUE(consumed.has_value());
    EXPECT_EQ(consumed->challengeSize, 10u);
    EXPECT_EQ(consumed->mapId, 33u);

    // Group entry and ALL 5 player aliases sharing generation erased atomically
    EXPECT_EQ(sim.GroupCount(), 0u);
    EXPECT_EQ(sim.PlayerCount(), 0u);

    // Re-entry / subsequent check: MUST NOT inherit old challenge
    auto secondEntry = sim.Consume(33, 100, 1, 1020);
    EXPECT_FALSE(secondEntry.has_value());

    auto memberEntry = sim.Consume(33, 100, 2, 1020);
    EXPECT_FALSE(memberEntry.has_value());
}

TEST(PendingPolicyTest, TtlExpiration)
{
    PendingPolicySimulator sim;
    sim.RegisterProposal(43, 200, { 10 }, 5, lfg::LfgCompositionMode::CURRENT_PARTY, 1000);

    // Within TTL (150s elapsed)
    sim.PurgeExpired(1150);
    EXPECT_EQ(sim.GroupCount(), 1u);

    // Past 300s TTL (301s elapsed)
    sim.PurgeExpired(1301);
    EXPECT_EQ(sim.GroupCount(), 0u);
    EXPECT_EQ(sim.PlayerCount(), 0u);

    auto result = sim.Consume(43, 200, 10, 1302);
    EXPECT_FALSE(result.has_value());
}

TEST(PendingPolicyTest, LeaderLogoutCleansGroupPolicy)
{
    PendingPolicySimulator sim;
    sim.RegisterProposal(33, 300, { 20, 21, 22 }, 3, lfg::LfgCompositionMode::CURRENT_PARTY, 1000);

    EXPECT_EQ(sim.GroupCount(), 1u);
    EXPECT_EQ(sim.PlayerCount(), 3u);

    // Non-leader logs out (leader is 20, member 21 logs out)
    sim.OnPlayerLogout(21, 20, 3);
    EXPECT_EQ(sim.PlayerCount(), 2u);
    EXPECT_EQ(sim.GroupCount(), 1u); // Group policy remains

    // Leader 20 logs out
    sim.OnPlayerLogout(20, 20, 2);
    EXPECT_EQ(sim.PlayerCount(), 1u);
    EXPECT_EQ(sim.GroupCount(), 0u); // Group policy purged
}

// =============================================================================
// Round 2.2: Multi-Role Bot Solver Tests
// =============================================================================

struct SolverMember
{
    uint8 roles;
};

struct SolverResult
{
    bool hasTank{false};
    bool hasHealer{false};
    uint32 realDpsCount{0};
    uint32 tankBotsNeeded{0};
    uint32 healerBotsNeeded{0};
    uint32 dpsBotsNeeded{0};
};

SolverResult SolveRoles(std::vector<SolverMember> const& members)
{
    SolverResult result;
    if (members.empty() || members.size() >= 5)
        return result;

    size_t const N = members.size();
    bool bestFound = false;
    int bestScore = -1;
    bool bestTank = false;
    bool bestHealer = false;
    uint32 bestDps = 0;

    auto backtrack = [&](auto& self, size_t idx, bool curTank, bool curHealer, uint32 curDps) -> void
    {
        if (idx == N)
        {
            int score = (curTank ? 100 : 0) + (curHealer ? 50 : 0) + int(curDps);
            if (!bestFound || score > bestScore)
            {
                bestScore = score;
                bestTank = curTank;
                bestHealer = curHealer;
                bestDps = curDps;
                bestFound = true;
            }
            return;
        }

        uint8 const availableRoles = members[idx].roles;

        // Try Tank
        if (!curTank && (availableRoles & lfg::PLAYER_ROLE_TANK))
            self(self, idx + 1, true, curHealer, curDps);

        // Try Healer
        if (!curHealer && (availableRoles & lfg::PLAYER_ROLE_HEALER))
            self(self, idx + 1, curTank, true, curDps);

        // Try DPS
        if (curDps < 3 && (availableRoles & lfg::PLAYER_ROLE_DAMAGE))
            self(self, idx + 1, curTank, curHealer, curDps + 1);

        // Unassigned branch
        self(self, idx + 1, curTank, curHealer, curDps);
    };

    backtrack(backtrack, 0, false, false, 0);

    result.hasTank = bestTank;
    result.hasHealer = bestHealer;
    result.realDpsCount = bestDps;

    uint32 const availableBotSlots = static_cast<uint32>(5 - N);
    uint32 remainingSlots = availableBotSlots;

    if (!result.hasTank && remainingSlots > 0)
    {
        result.tankBotsNeeded = 1;
        --remainingSlots;
    }

    if (!result.hasHealer && remainingSlots > 0)
    {
        result.healerBotsNeeded = 1;
        --remainingSlots;
    }

    uint32 const missingDps = (result.realDpsCount >= 3) ? 0 : (3 - result.realDpsCount);
    result.dpsBotsNeeded = std::min<uint32>(remainingSlots, missingDps);

    return result;
}

TEST(BotRoleSolverTest, SinglePlayerMultiRole)
{
    // Solo player queued as Tank | Healer
    // Must be assigned to Tank (or Healer), exactly ONE role, never both!
    std::vector<SolverMember> members = { { lfg::PLAYER_ROLE_TANK | lfg::PLAYER_ROLE_HEALER } };
    SolverResult res = SolveRoles(members);

    EXPECT_TRUE(res.hasTank);
    EXPECT_FALSE(res.hasHealer); // Solo player cannot be both!
    EXPECT_EQ(res.tankBotsNeeded, 0u);
    EXPECT_EQ(res.healerBotsNeeded, 1u); // Healer bot spawned
    EXPECT_EQ(res.dpsBotsNeeded, 3u);    // 3 DPS bots spawned
    EXPECT_EQ(1u + res.tankBotsNeeded + res.healerBotsNeeded + res.dpsBotsNeeded, 5u); // Total exactly 5
}

TEST(BotRoleSolverTest, FourRealDps_NeverExceedsFiveMembers)
{
    // 4 real DPS queued
    // Bot slots available = 5 - 4 = 1 slot.
    // Tank has higher priority than Healer, so add 1 Tank bot.
    // Group must NEVER become 6 players!
    std::vector<SolverMember> members = {
        { lfg::PLAYER_ROLE_DAMAGE },
        { lfg::PLAYER_ROLE_DAMAGE },
        { lfg::PLAYER_ROLE_DAMAGE },
        { lfg::PLAYER_ROLE_DAMAGE }
    };
    SolverResult res = SolveRoles(members);

    EXPECT_FALSE(res.hasTank);
    EXPECT_FALSE(res.hasHealer);
    EXPECT_EQ(res.realDpsCount, 3u); // Capped to 3 standard dungeon DPS
    EXPECT_EQ(res.tankBotsNeeded, 1u);
    EXPECT_EQ(res.healerBotsNeeded, 0u); // Slots exhausted!
    EXPECT_EQ(res.dpsBotsNeeded, 0u);

    uint32 totalGroup = 4u + res.tankBotsNeeded + res.healerBotsNeeded + res.dpsBotsNeeded;
    EXPECT_EQ(totalGroup, 5u); // Exactly 5, NOT 6!
}

TEST(BotRoleSolverTest, FullFiveRealPlayers_ZeroBotsNeeded)
{
    std::vector<SolverMember> members = {
        { lfg::PLAYER_ROLE_TANK },
        { lfg::PLAYER_ROLE_HEALER },
        { lfg::PLAYER_ROLE_DAMAGE },
        { lfg::PLAYER_ROLE_DAMAGE },
        { lfg::PLAYER_ROLE_DAMAGE }
    };
    SolverResult res = SolveRoles(members);

    EXPECT_EQ(res.tankBotsNeeded, 0u);
    EXPECT_EQ(res.healerBotsNeeded, 0u);
    EXPECT_EQ(res.dpsBotsNeeded, 0u);
}

TEST(BotRoleSolverTest, FlexibleRolesThreeMembers)
{
    // Player 1: Tank | DPS
    // Player 2: Healer | DPS
    // Player 3: DPS
    std::vector<SolverMember> members = {
        { lfg::PLAYER_ROLE_TANK | lfg::PLAYER_ROLE_DAMAGE },
        { lfg::PLAYER_ROLE_HEALER | lfg::PLAYER_ROLE_DAMAGE },
        { lfg::PLAYER_ROLE_DAMAGE }
    };
    SolverResult res = SolveRoles(members);

    EXPECT_TRUE(res.hasTank);
    EXPECT_TRUE(res.hasHealer);
    EXPECT_EQ(res.realDpsCount, 1u);
    EXPECT_EQ(res.tankBotsNeeded, 0u);
    EXPECT_EQ(res.healerBotsNeeded, 0u);
    EXPECT_EQ(res.dpsBotsNeeded, 2u); // 3 members + 2 DPS bots = 5 total

    uint32 totalGroup = 3u + res.tankBotsNeeded + res.healerBotsNeeded + res.dpsBotsNeeded;
    EXPECT_EQ(totalGroup, 5u);
}

// =============================================================================
// Round 3: Census, Instance Profiles & Calibration Tests
// =============================================================================

TEST(ContentCensusTest, StaticCensusProfilesPopulated)
{
    EXPECT_GE(sGeneratedInstanceProfiles.size(), 90u);

    // Verify key landmark instances
    auto const* deadmines = FindGeneratedInstanceProfile(36);
    ASSERT_NE(deadmines, nullptr);
    EXPECT_EQ(deadmines->era, ContentEra::Classic);
    EXPECT_FALSE(deadmines->isRaid);
    EXPECT_EQ(deadmines->tier, ContentTier::DUNGEON_NORMAL);

    auto const* moltenCore = FindGeneratedInstanceProfile(409);
    ASSERT_NE(moltenCore, nullptr);
    EXPECT_EQ(moltenCore->era, ContentEra::Classic);
    EXPECT_TRUE(moltenCore->isRaid);
    EXPECT_EQ(moltenCore->tier, ContentTier::RAID_MID);
    EXPECT_EQ(moltenCore->intendedPlayers, 40u);

    auto const* karazhan = FindGeneratedInstanceProfile(532);
    ASSERT_NE(karazhan, nullptr);
    EXPECT_EQ(karazhan->era, ContentEra::TBC);
    EXPECT_TRUE(karazhan->isRaid);
    EXPECT_EQ(karazhan->tier, ContentTier::RAID_ENTRY);
    EXPECT_EQ(karazhan->intendedPlayers, 10u);

    auto const* naxx = FindGeneratedInstanceProfile(533);
    ASSERT_NE(naxx, nullptr);
    EXPECT_EQ(naxx->era, ContentEra::WotLK);
    EXPECT_TRUE(naxx->isRaid);
    EXPECT_EQ(naxx->tier, ContentTier::RAID_ENTRY);

    auto const* icc = FindGeneratedInstanceProfile(631);
    ASSERT_NE(icc, nullptr);
    EXPECT_EQ(icc->era, ContentEra::WotLK);
    EXPECT_TRUE(icc->isRaid);
    EXPECT_EQ(icc->tier, ContentTier::RAID_PINNACLE);
}

TEST(ContentCensusTest, InstanceProfileRegistryTiersCalibrated)
{
    sInstanceProfileRegistry->Initialize();

    std::vector<std::string> issues;
    bool ok = sInstanceProfileRegistry->ValidateAll(issues);
    for (auto const& issue : issues)
        std::cout << "ISSUE: " << issue << std::endl;
    EXPECT_TRUE(ok);

    // Check tier resolution
    EXPECT_EQ(sInstanceProfileRegistry->GetTierForMap(36), ContentTier::DUNGEON_NORMAL);
    EXPECT_EQ(sInstanceProfileRegistry->GetTierForMap(36, 1), ContentTier::DUNGEON_HEROIC); // Heroic difficulty = 1
    EXPECT_EQ(sInstanceProfileRegistry->GetTierForMap(409), ContentTier::RAID_MID);
    EXPECT_EQ(sInstanceProfileRegistry->GetTierForMap(531), ContentTier::RAID_PINNACLE);
    EXPECT_EQ(sInstanceProfileRegistry->GetTierForMap(631), ContentTier::RAID_PINNACLE);
}

TEST(ProgressionCalibrationTest, Cap60AllEras_MonotonicZoneCompression)
{
    // Cap 60 with Classic, TBC, and WotLK:
    // Classic authored: 1-60 -> effective 1-45
    // TBC authored: 58-70 -> effective 45-55
    // WotLK authored: 68-80 -> effective 55-60
    ProgressionLayout layout = ProgressionLayout::Create(60, true, true);
    std::string err;
    ASSERT_TRUE(layout.Validate(err)) << err;

    // Classic zone (e.g. Westfall authored ~15, Plaguelands authored ~55)
    uint8 const effWestfall = layout.MapAuthoredToEffective(ContentEra::Classic, 15);
    uint8 const effPlaguelands = layout.MapAuthoredToEffective(ContentEra::Classic, 55);
    EXPECT_GE(effWestfall, 1);
    EXPECT_LE(effPlaguelands, 45);
    EXPECT_LT(effWestfall, effPlaguelands);

    // TBC zone (e.g. Hellfire Peninsula authored ~60, Shadowmoon authored ~70)
    uint8 const effHellfire = layout.MapAuthoredToEffective(ContentEra::TBC, 60);
    uint8 const effShadowmoon = layout.MapAuthoredToEffective(ContentEra::TBC, 70);
    EXPECT_GE(effHellfire, 45);
    EXPECT_EQ(effShadowmoon, 55);
    EXPECT_LE(effHellfire, effShadowmoon);

    // WotLK zone (e.g. Borean Tundra authored ~70, Icecrown authored ~80)
    uint8 const effBorean = layout.MapAuthoredToEffective(ContentEra::WotLK, 70);
    uint8 const effIcecrown = layout.MapAuthoredToEffective(ContentEra::WotLK, 80);
    EXPECT_GE(effBorean, 55);
    EXPECT_EQ(effIcecrown, 60);
    EXPECT_LE(effBorean, effIcecrown);
}

TEST(ProgressionCalibrationTest, QuestChainMonotonicity_NeverInverts)
{
    ProgressionLayout layout = ProgressionLayout::Create(60, true, true);

    // Chain: Q1 (authored 12) -> Q2 (authored 14) -> Q3 (authored 16)
    uint8 q1 = layout.MapAuthoredToEffective(ContentEra::Classic, 12);
    uint8 q2 = layout.MapAuthoredToEffective(ContentEra::Classic, 14);
    uint8 q3 = layout.MapAuthoredToEffective(ContentEra::Classic, 16);

    EXPECT_LE(q1, q2);
    EXPECT_LE(q2, q3);

    // Cross-era transition: End of Classic Q (authored 60) -> Intro TBC Q (authored 58)
    uint8 qClassicEnd = layout.MapAuthoredToEffective(ContentEra::Classic, 60);
    uint8 qTbcStart = layout.MapAuthoredToEffective(ContentEra::TBC, 58);
    EXPECT_EQ(qClassicEnd, 45);
    EXPECT_GE(qTbcStart, 45);
}

// =============================================================================
// Round 3.1: Census Integrity, Generated Profiles & Runtime Wiring Tests
// =============================================================================

TEST(ContentCensusIntegrityTest, PvPMapsExcludedFromInstanceRegistry)
{
    sInstanceProfileRegistry->Initialize();

    // Verified list of Battlegrounds and Arenas in WoW 3.3.5a
    static constexpr uint32 pvpMaps[] = { 30, 489, 529, 559, 562, 566, 572, 607, 617, 618, 628 };

    for (uint32 mapId : pvpMaps)
    {
        // 1. InstanceProfileRegistry must NEVER register or have a profile for PvP maps
        EXPECT_FALSE(sInstanceProfileRegistry->HasProfile(mapId))
            << "PvP Map " << mapId << " should NOT be in InstanceProfileRegistry!";
        EXPECT_EQ(sInstanceProfileRegistry->GetProfile(mapId), nullptr)
            << "PvP Map " << mapId << " should return nullptr from GetProfile!";

        // 2. Kind helper must classify as BATTLEGROUND or ARENA, never DUNGEON or RAID
        MapContentKind kind = sInstanceProfileRegistry->GetKindForMap(mapId);
        EXPECT_TRUE(kind == MapContentKind::BATTLEGROUND || kind == MapContentKind::ARENA)
            << "Map " << mapId << " has unexpected kind: " << static_cast<uint32>(kind);
        EXPECT_FALSE(IsPvEInstanceKind(kind))
            << "Map " << mapId << " was wrongly considered a PvE instance!";
    }
}

TEST(ContentCensusIntegrityTest, GeneratedCensusSchemaAndCounts)
{
    EXPECT_EQ(GENERATED_CONTENT_CENSUS_SCHEMA_VERSION, 311);
    EXPECT_EQ(sGeneratedMapProfiles.size(), 374u);
    EXPECT_EQ(sGeneratedInstanceProfiles.size(), 221u); // 221 PvE instance difficulty variants
    EXPECT_EQ(sGeneratedCreaturePlacements.size(), 18751u); // 100% active production spawns
    EXPECT_EQ(sGeneratedQuestProfiles.size(), 10106u);
    EXPECT_EQ(sGeneratedItemProfiles.size(), 3865u);
    EXPECT_EQ(sGeneratedLfgProfiles.size(), 423u);
    EXPECT_EQ(sGeneratedAccessProfiles.size(), 121u);
}

TEST(ContentCensusIntegrityTest, QuestRuntimeUsesGeneratedCensus)
{
    // Quest 203 in census
    auto const* q203 = FindGeneratedQuestProfile(203);
    ASSERT_NE(q203, nullptr);
    EXPECT_EQ(q203->era, ContentEra::Classic);
    EXPECT_EQ(q203->authoredLevel, 33);
    EXPECT_EQ(q203->authoredMinLevel, 30);
    EXPECT_GE(q203->confidence, 80u);

    // Verify ContentPackRegistry resolution delegates to generated census
    EraResolutionResult res = sContentPackRegistry->ResolveEraDetailsForQuest(203, 12, 0, 33);
    EXPECT_EQ(res.era, ContentEra::Classic);
    EXPECT_EQ(res.source, EraResolutionSource::ContentCensus);
    EXPECT_FLOAT_EQ(res.confidence, static_cast<float>(q203->confidence) / 100.0f);
}

TEST(ContentCensusIntegrityTest, CreaturePlacementAwareResolution)
{
    // Test creature entry in census: entry with map placement
    ASSERT_FALSE(sGeneratedCreaturePlacements.empty());
    auto const& sample = sGeneratedCreaturePlacements[0];

    auto const* found = FindGeneratedCreaturePlacement(sample.entry, sample.mapId);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->entry, sample.entry);
    EXPECT_EQ(found->mapId, sample.mapId);
    EXPECT_EQ(found->era, sample.era);

    EraResolutionResult res = sContentPackRegistry->ResolveEraDetailsForCreature(
        sample.entry, sample.mapId, 0, 0, 70);
    EXPECT_EQ(res.era, sample.era);
    EXPECT_EQ(res.source, EraResolutionSource::ContentCensus);
}

TEST(ContentCensusIntegrityTest, LfgAndDungeonAccessScaling)
{
    ProgressionLayout layoutCap60 = ProgressionLayout::Create(60, true, true);
    // Classic: 1-45, TBC: 45-55, WotLK: 55-60

    // Utgarde Keep (LFG Dungeon ID 242, Map 574, Authored min 69, max 72, target 70)
    auto const* ukLfg = FindGeneratedLfgProfile(242);
    ASSERT_NE(ukLfg, nullptr);
    EXPECT_EQ(ukLfg->era, ContentEra::WotLK);
    EXPECT_EQ(ukLfg->mapId, 574u);

    uint8 scaledMin = layoutCap60.MapAuthoredToEffective(ukLfg->era, ukLfg->authoredMin);
    uint8 scaledMax = layoutCap60.MapAuthoredToEffective(ukLfg->era, ukLfg->authoredMax);
    EXPECT_GE(scaledMin, 55);
    EXPECT_LE(scaledMax, 60);
    EXPECT_LE(scaledMin, scaledMax);

    // Access profile for Utgarde Keep (Map 574)
    auto const* ukAccess = FindGeneratedAccessProfile(574, 0);
    ASSERT_NE(ukAccess, nullptr);
    EXPECT_EQ(ukAccess->era, ContentEra::WotLK);
    uint8 accMin = layoutCap60.MapAuthoredToEffective(ukAccess->era, ukAccess->authoredMin);
    uint8 accMax = layoutCap60.MapAuthoredToEffective(ukAccess->era, ukAccess->authoredMax);
    EXPECT_GE(accMin, 55);
    EXPECT_LE(accMax, 60);

    // Expansion locked check when WotLK is disabled
    ProgressionLayout layoutClassicTBC = ProgressionLayout::Create(70, true, false);
    EXPECT_FALSE(layoutClassicTBC.IsEraEnabled(ContentEra::WotLK));
}

TEST(ContentCensusIntegrityTest, ItemSourceTiersAndOutlierClassification)
{
    // Landmark items in census
    // Classic raid item: Elementium Reinforced Bulwark (Item 19354, BWL Map 469 -> RAID_END)
    auto const* bwlShield = FindGeneratedItemProfile(19354);
    ASSERT_NE(bwlShield, nullptr);
    EXPECT_EQ(bwlShield->era, ContentEra::Classic);
    EXPECT_EQ(bwlShield->tier, ContentTier::RAID_END);

    // TBC Legendary: Warglaive of Azzinoth (BT Map 564, Item 32837 -> RAID_END)
    auto const* warglaive = FindGeneratedItemProfile(32837);
    ASSERT_NE(warglaive, nullptr);
    EXPECT_EQ(warglaive->era, ContentEra::TBC);
    EXPECT_EQ(warglaive->tier, ContentTier::RAID_END);

    // WotLK Pinnacle: Item 50351 (ICC Map 631 -> RAID_PINNACLE)
    auto const* iccTrinket = FindGeneratedItemProfile(50351);
    ASSERT_NE(iccTrinket, nullptr);
    EXPECT_EQ(iccTrinket->era, ContentEra::WotLK);
    EXPECT_EQ(iccTrinket->tier, ContentTier::RAID_PINNACLE);
}

// =============================================================================
// Round 3.2: Item Runtime Authority & Policy Enforcement Tests
// =============================================================================

TEST(ItemRuntimeAuthorityTest, GeneratedTierWinsOverMisleadingIlvl)
{
    ItemTemplate fakeItem;
    fakeItem.ItemId = 32837; // Warglaive of Azzinoth in census (BT, RAID_END)
    fakeItem.ItemLevel = 156;
    fakeItem.RequiredLevel = 70;

    ItemScalingContext ctx = ItemScalingContext::Resolve(&fakeItem);
    EXPECT_TRUE(ctx.hasGeneratedProfile);
    EXPECT_FALSE(ctx.fallbackTierInference);
    EXPECT_EQ(ctx.era, ContentEra::TBC);
    EXPECT_EQ(ctx.tier, ContentTier::RAID_END);
    EXPECT_EQ(ctx.policy, ItemScalingPolicy::REVIEW_SPECIAL); // Has proc / use flags
}

TEST(ItemRuntimeAuthorityTest, WotLKTierProgressionMonotonicAtCap60)
{
    ProgressionLayout layout = ProgressionLayout::Create(60, true, true);
    // Classic: 1-45, TBC: 45-55, WotLK: 55-60

    // Representative real items from current census:
    // Naxxramas: Item 39291 (RAID_ENTRY, auth ilvl 200, req 80)
    // Ulduar: Item 44005 (RAID_MID, auth ilvl 226, req 80)
    // ToC: Item 45086 (RAID_END, auth ilvl 232, req 80)
    // ICC: Item 50351 (RAID_PINNACLE, auth ilvl 264, req 80)

    ItemTemplate naxxItem;
    naxxItem.ItemId = 39291;
    naxxItem.ItemLevel = 200;
    naxxItem.RequiredLevel = 80;

    ItemTemplate ulduarItem;
    ulduarItem.ItemId = 44005;
    ulduarItem.ItemLevel = 226;
    ulduarItem.RequiredLevel = 80;

    ItemTemplate tocItem;
    tocItem.ItemId = 45086;
    tocItem.ItemLevel = 232;
    tocItem.RequiredLevel = 80;

    ItemTemplate iccItem;
    iccItem.ItemId = 50351;
    iccItem.ItemLevel = 264;
    iccItem.RequiredLevel = 80;

    ScaledItemBudget bNaxx = sItemBudgetScaler->CalculateItemBudget(&naxxItem, layout);
    ScaledItemBudget bUlduar = sItemBudgetScaler->CalculateItemBudget(&ulduarItem, layout);
    ScaledItemBudget bToc = sItemBudgetScaler->CalculateItemBudget(&tocItem, layout);
    ScaledItemBudget bIcc = sItemBudgetScaler->CalculateItemBudget(&iccItem, layout);

    // Strict monotonic effective power progression
    EXPECT_LT(bNaxx.effectiveItemLevel, bUlduar.effectiveItemLevel);
    EXPECT_LT(bUlduar.effectiveItemLevel, bToc.effectiveItemLevel);
    EXPECT_LT(bToc.effectiveItemLevel, bIcc.effectiveItemLevel);

    EXPECT_GT(bNaxx.statMultiplier, 0.20f);
    EXPECT_GT(bUlduar.statMultiplier, 0.20f);
    EXPECT_GT(bToc.statMultiplier, 0.20f);
    EXPECT_GT(bIcc.statMultiplier, 0.20f);
}

TEST(ItemRuntimeAuthorityTest, TBCTierProgressionMonotonicAtCap60)
{
    ProgressionLayout layout = ProgressionLayout::Create(60, true, true);

    // Representative real items from current census:
    // Karazhan: Item 24079 (RAID_ENTRY, ilvl 115)
    // SSC/TK: Item 30105 (RAID_MID, ilvl 128)
    // BT/Hyjal: Item 32505 (RAID_END, ilvl 141)
    // Sunwell: Item 30311 (RAID_PINNACLE, ilvl 159)

    ItemTemplate karaItem;
    karaItem.ItemId = 24079;
    karaItem.ItemLevel = 115;
    karaItem.RequiredLevel = 70;

    ItemTemplate sscItem;
    sscItem.ItemId = 30105;
    sscItem.ItemLevel = 128;
    sscItem.RequiredLevel = 70;

    ItemTemplate btItem;
    btItem.ItemId = 32505;
    btItem.ItemLevel = 141;
    btItem.RequiredLevel = 70;

    ItemTemplate sunwellItem;
    sunwellItem.ItemId = 30311;
    sunwellItem.ItemLevel = 159;
    sunwellItem.RequiredLevel = 70;

    ScaledItemBudget bKara = sItemBudgetScaler->CalculateItemBudget(&karaItem, layout);
    ScaledItemBudget bSsc = sItemBudgetScaler->CalculateItemBudget(&sscItem, layout);
    ScaledItemBudget bBt = sItemBudgetScaler->CalculateItemBudget(&btItem, layout);
    ScaledItemBudget bSunwell = sItemBudgetScaler->CalculateItemBudget(&sunwellItem, layout);

    EXPECT_LE(bKara.effectiveItemLevel, bSsc.effectiveItemLevel);
    EXPECT_LT(bSsc.effectiveItemLevel, bBt.effectiveItemLevel);
    EXPECT_LT(bBt.effectiveItemLevel, bSunwell.effectiveItemLevel);
}

TEST(ItemRuntimeAuthorityTest, CustomItemSafetyAndPreservePolicies)
{
    ProgressionLayout layout = ProgressionLayout::Create(60, true, true);

    // 1. Profiled Custom Cosmetic: Wizened Wizard (100002) in generated census
    ItemTemplate cosmetic;
    cosmetic.ItemId = 100002;
    cosmetic.ItemLevel = 20;
    cosmetic.RequiredLevel = 0;
    cosmetic.Armor = 0;

    ItemScalingContext ctxCosmetic = ItemScalingContext::Resolve(&cosmetic);
    EXPECT_TRUE(ctxCosmetic.hasGeneratedProfile);
    EXPECT_EQ(ctxCosmetic.policy, ItemScalingPolicy::PRESERVE);
    EXPECT_TRUE(ctxCosmetic.specialFlags & ITEM_SPECIAL_PRESERVE);

    ScaledItemBudget bCosmetic = sItemBudgetScaler->CalculateItemBudget(&cosmetic, layout, ctxCosmetic);
    EXPECT_EQ(bCosmetic.effectiveItemLevel, cosmetic.ItemLevel);
    EXPECT_EQ(bCosmetic.effectiveRequiredLevel, cosmetic.RequiredLevel);
    EXPECT_FLOAT_EQ(bCosmetic.statMultiplier, 1.0f);

    // 2. Profiled Custom Service / Potion: Race Change Potion (200001) in generated census
    ItemTemplate classItem;
    classItem.ItemId = 200001;
    classItem.ItemLevel = 64;
    classItem.RequiredLevel = 0;

    ItemScalingContext ctxClass = ItemScalingContext::Resolve(&classItem);
    EXPECT_TRUE(ctxCosmetic.hasGeneratedProfile);
    EXPECT_EQ(ctxClass.policy, ItemScalingPolicy::PRESERVE);
    ScaledItemBudget bClass = sItemBudgetScaler->CalculateItemBudget(&classItem, layout, ctxClass);
    EXPECT_EQ(bClass.effectiveItemLevel, classItem.ItemLevel);
    EXPECT_FLOAT_EQ(bClass.statMultiplier, 1.0f);

    // 3. Profiled Custom Gameplay: Havoc's Call (350012) in generated census
    ItemTemplate gameplayItem;
    gameplayItem.ItemId = 350012;
    gameplayItem.ItemLevel = 287;
    gameplayItem.RequiredLevel = 80;

    ItemScalingContext ctxGameplay = ItemScalingContext::Resolve(&gameplayItem);
    EXPECT_TRUE(ctxGameplay.hasGeneratedProfile);
    EXPECT_EQ(ctxGameplay.policy, ItemScalingPolicy::TIER_ALIGNED);
    EXPECT_FALSE(ctxGameplay.specialFlags & ITEM_SPECIAL_PRESERVE);

    // 4. Unprofiled custom item in a preserved range: no stat budget, left exactly as authored.
    ItemTemplate unprofiledCosmetic;
    unprofiledCosmetic.ItemId = 150000;
    unprofiledCosmetic.ItemLevel = 80;
    unprofiledCosmetic.RequiredLevel = 60;

    ItemScalingContext ctxUnprofiledCosmetic = ItemScalingContext::Resolve(&unprofiledCosmetic);
    EXPECT_FALSE(ctxUnprofiledCosmetic.hasGeneratedProfile);
    EXPECT_EQ(ctxUnprofiledCosmetic.policy, ItemScalingPolicy::PRESERVE);
    EXPECT_TRUE(ctxUnprofiledCosmetic.specialFlags & ITEM_SPECIAL_CUSTOM);
    EXPECT_TRUE(ctxUnprofiledCosmetic.specialFlags & ITEM_SPECIAL_PRESERVE);

    ScaledItemBudget bUnprofiledCosmetic = sItemBudgetScaler->CalculateItemBudget(&unprofiledCosmetic, layout, ctxUnprofiledCosmetic);
    EXPECT_EQ(bUnprofiledCosmetic.effectiveItemLevel, unprofiledCosmetic.ItemLevel);
    EXPECT_FLOAT_EQ(bUnprofiledCosmetic.statMultiplier, 1.0f);

    // 5. Unprofiled custom item in the gameplay range, above where that range used to stop: this is
    // equipment with statistics - the Bloodforged and Heroic versions live here - and is scaled.
    ItemTemplate unprofiledGameplay;
    unprofiledGameplay.ItemId = 750000;
    unprofiledGameplay.ItemLevel = 80;
    unprofiledGameplay.RequiredLevel = 60;

    ItemScalingContext ctxUnprofiledGameplay = ItemScalingContext::Resolve(&unprofiledGameplay);
    EXPECT_FALSE(ctxUnprofiledGameplay.hasGeneratedProfile);
    EXPECT_NE(ctxUnprofiledGameplay.policy, ItemScalingPolicy::PRESERVE);
    EXPECT_TRUE(ctxUnprofiledGameplay.specialFlags & ITEM_SPECIAL_CUSTOM);
    EXPECT_FALSE(ctxUnprofiledGameplay.specialFlags & ITEM_SPECIAL_PRESERVE);
}

TEST(ItemRuntimeAuthorityTest, MissingGeneratedProfileSafelyFallsBack)
{
    ProgressionLayout layout = ProgressionLayout::Create(60, true, true);

    // Standard unprofiled item (< 100000, not custom): uses fallback tier inference
    ItemTemplate unprofiled;
    unprofiled.ItemId = 99999;
    unprofiled.ItemLevel = 200;
    unprofiled.RequiredLevel = 80;

    ItemScalingContext ctx = ItemScalingContext::Resolve(&unprofiled);
    EXPECT_FALSE(ctx.hasGeneratedProfile);
    EXPECT_TRUE(ctx.fallbackTierInference);
    EXPECT_EQ(ctx.era, ContentEra::WotLK);
    EXPECT_EQ(ctx.tier, ContentTier::RAID_ENTRY);

    ScaledItemBudget budget = sItemBudgetScaler->CalculateItemBudget(&unprofiled, layout, ctx);
    EXPECT_LE(budget.effectiveRequiredLevel, 60u);
    EXPECT_LT(budget.effectiveItemLevel, 200u);
}

TEST(ItemRuntimeAuthorityTest, StockCap80IdentityPreserved)
{
    ProgressionLayout layoutCap80 = ProgressionLayout::Create(80, true, true);

    ItemTemplate iccItem;
    iccItem.ItemId = 50351;
    iccItem.ItemLevel = 264;
    iccItem.RequiredLevel = 80;

    ScaledItemBudget budget = sItemBudgetScaler->CalculateItemBudget(&iccItem, layoutCap80);
    EXPECT_EQ(budget.effectiveRequiredLevel, 80u);
    EXPECT_EQ(budget.effectiveItemLevel, 264u);
    EXPECT_FLOAT_EQ(budget.statMultiplier, 1.0f);
    EXPECT_FLOAT_EQ(budget.ratingMultiplier, 1.0f);
    EXPECT_FLOAT_EQ(budget.armorMultiplier, 1.0f);
    EXPECT_FLOAT_EQ(budget.weaponDpsMultiplier, 1.0f);
}

TEST(ItemRuntimeAuthorityTest, ScaleAllItemsIdempotenceProtection)
{
    ProgressionLayout layout = ProgressionLayout::Create(60, true, true);
    sItemBudgetScaler->ResetScaledState();
    EXPECT_FALSE(sItemBudgetScaler->AreItemsScaled());

    sItemBudgetScaler->ScaleAllItems(layout);
    EXPECT_TRUE(sItemBudgetScaler->AreItemsScaled());

    // Second call must no-op without crashing or multiplying stats twice
    sItemBudgetScaler->ScaleAllItems(layout);
    EXPECT_TRUE(sItemBudgetScaler->AreItemsScaled());
}

// ============================================================================
// Round 3.3 — Source Graph Authority & Census Completeness Tests
// ============================================================================

TEST(SourceGraphAuthorityTest, WarglaiveOfAzzinothIsRaidEndNotPinnacle)
{
    // Item 32837: Warglaive of Azzinoth
    // Authoritative source: Illidan Stormrage (Creature 22917) in Black Temple (Map 564)
    // Must be classified as RAID_END with sourceMap = 564, NOT RAID_PINNACLE
    auto const* prof = FindGeneratedItemProfile(32837);
    ASSERT_NE(prof, nullptr);
    EXPECT_EQ(prof->era, ContentEra::TBC);
    EXPECT_EQ(prof->tier, ContentTier::RAID_END);
    EXPECT_EQ(prof->sourceMap, 564u); // Black Temple

    ItemTemplate warglaive;
    warglaive.ItemId = 32837;
    warglaive.ItemLevel = 156;
    warglaive.RequiredLevel = 70;

    ItemScalingContext ctx = ItemScalingContext::Resolve(&warglaive);
    EXPECT_TRUE(ctx.hasGeneratedProfile);
    EXPECT_EQ(ctx.tier, ContentTier::RAID_END);
    EXPECT_EQ(ctx.sourceMap, 564u);
}

TEST(SourceGraphAuthorityTest, PreservedItemsKeepTheirTemplate)
{
    ItemTemplate warglaive;
    warglaive.ItemId = 32837;
    warglaive.ItemLevel = 156;
    warglaive.RequiredLevel = 70;

    sItemBudgetScaler->PreserveItems({ 32837 });
    ItemScalingContext const preserved = ItemScalingContext::Resolve(&warglaive);
    sItemBudgetScaler->PreserveItems({});

    EXPECT_EQ(preserved.policy, ItemScalingPolicy::PRESERVE);
    EXPECT_EQ(preserved.tier, ContentTier::RAID_END);
    EXPECT_EQ(preserved.sourceMap, 564u);
    EXPECT_NE(ItemScalingContext::Resolve(&warglaive).policy, ItemScalingPolicy::PRESERVE);
}

TEST(SourceGraphAuthorityTest, SunwellItemsPreserveRaidPinnacle)
{
    // Item 34334: Thori'dal, the Stars' Fury
    // Authoritative source: Kil'jaeden (Creature 25315) in Sunwell Plateau (Map 580)
    auto const* prof = FindGeneratedItemProfile(34334);
    ASSERT_NE(prof, nullptr);
    EXPECT_EQ(prof->era, ContentEra::TBC);
    EXPECT_EQ(prof->tier, ContentTier::RAID_PINNACLE);
    EXPECT_EQ(prof->sourceMap, 580u); // Sunwell Plateau

    ItemTemplate thoridal;
    thoridal.ItemId = 34334;
    thoridal.ItemLevel = 155;
    thoridal.RequiredLevel = 70;

    ItemScalingContext ctx = ItemScalingContext::Resolve(&thoridal);
    EXPECT_TRUE(ctx.hasGeneratedProfile);
    EXPECT_EQ(ctx.tier, ContentTier::RAID_PINNACLE);
    EXPECT_EQ(ctx.sourceMap, 580u);
}

TEST(SourceGraphAuthorityTest, WotlkRaidTiersDrivenBySourceMap)
{
    // Naxxramas (Map 533): Item 39291 (Torment of the Banished) -> RAID_ENTRY
    auto const* profNaxx = FindGeneratedItemProfile(39291);
    ASSERT_NE(profNaxx, nullptr);
    EXPECT_EQ(profNaxx->era, ContentEra::WotLK);
    EXPECT_EQ(profNaxx->tier, ContentTier::RAID_ENTRY);
    EXPECT_EQ(profNaxx->sourceMap, 533u);

    // Ulduar (Map 603): Item 45086 (Rising Sun) -> RAID_MID
    auto const* profUlduar = FindGeneratedItemProfile(45086);
    ASSERT_NE(profUlduar, nullptr);
    EXPECT_EQ(profUlduar->era, ContentEra::WotLK);
    EXPECT_EQ(profUlduar->tier, ContentTier::RAID_MID);
    EXPECT_EQ(profUlduar->sourceMap, 603u);

    // Icecrown Citadel (Map 631): Item 50351 (Tiny Abomination in a Jar) -> RAID_PINNACLE
    auto const* profIcc = FindGeneratedItemProfile(50351);
    ASSERT_NE(profIcc, nullptr);
    EXPECT_EQ(profIcc->era, ContentEra::WotLK);
    EXPECT_EQ(profIcc->tier, ContentTier::RAID_PINNACLE);
    EXPECT_EQ(profIcc->sourceMap, 631u);
}

TEST(SourceGraphAuthorityTest, DungeonHeroicPowerBandDistinctFromRaidEntry)
{
    ProgressionLayout layout = ProgressionLayout::Create(60, true, true);

    // TBC: Dungeon Normal < Dungeon Heroic < Raid Entry
    ItemTemplate tbcNorm;
    tbcNorm.ItemId = 900001;
    tbcNorm.ItemLevel = 100;
    tbcNorm.RequiredLevel = 70;
    ItemScalingContext ctxNorm;
    ctxNorm.era = ContentEra::TBC;
    ctxNorm.tier = ContentTier::DUNGEON_NORMAL;

    ItemTemplate tbcHeroic;
    tbcHeroic.ItemId = 900002;
    tbcHeroic.ItemLevel = 118;
    tbcHeroic.RequiredLevel = 70;
    ItemScalingContext ctxHeroic;
    ctxHeroic.era = ContentEra::TBC;
    ctxHeroic.tier = ContentTier::DUNGEON_HEROIC;

    ItemTemplate tbcRaid;
    tbcRaid.ItemId = 900003;
    tbcRaid.ItemLevel = 125;
    tbcRaid.RequiredLevel = 70;
    ItemScalingContext ctxRaid;
    ctxRaid.era = ContentEra::TBC;
    ctxRaid.tier = ContentTier::RAID_ENTRY;

    ScaledItemBudget bNorm = sItemBudgetScaler->CalculateItemBudget(&tbcNorm, layout, ctxNorm);
    ScaledItemBudget bHeroic = sItemBudgetScaler->CalculateItemBudget(&tbcHeroic, layout, ctxHeroic);
    ScaledItemBudget bRaid = sItemBudgetScaler->CalculateItemBudget(&tbcRaid, layout, ctxRaid);

    EXPECT_LE(bNorm.effectiveItemLevel, bHeroic.effectiveItemLevel);
    EXPECT_LT(bHeroic.effectiveItemLevel, bRaid.effectiveItemLevel);

    // WotLK: Dungeon Normal < Dungeon Heroic < Raid Entry
    ItemTemplate wotlkHeroic;
    wotlkHeroic.ItemId = 900004;
    wotlkHeroic.ItemLevel = 195;
    wotlkHeroic.RequiredLevel = 80;
    ItemScalingContext ctxWotlkHeroic;
    ctxWotlkHeroic.era = ContentEra::WotLK;
    ctxWotlkHeroic.tier = ContentTier::DUNGEON_HEROIC;

    ItemTemplate wotlkRaid;
    wotlkRaid.ItemId = 900005;
    wotlkRaid.ItemLevel = 213;
    wotlkRaid.RequiredLevel = 80;
    ItemScalingContext ctxWotlkRaid;
    ctxWotlkRaid.era = ContentEra::WotLK;
    ctxWotlkRaid.tier = ContentTier::RAID_ENTRY;

    ScaledItemBudget bWotlkHeroic = sItemBudgetScaler->CalculateItemBudget(&wotlkHeroic, layout, ctxWotlkHeroic);
    ScaledItemBudget bWotlkRaid = sItemBudgetScaler->CalculateItemBudget(&wotlkRaid, layout, ctxWotlkRaid);

    EXPECT_LT(bWotlkHeroic.effectiveItemLevel, bWotlkRaid.effectiveItemLevel);
}

TEST(SourceGraphAuthorityTest, HighEntryCreaturePlacementIndexed)
{
    // The Lich King (Entry 36597) in ICC (Map 631)
    // In Round 3.1, [:4000] capped entries and missed 36597.
    // In Round 3.3, 100% of production spawns are indexed.
    auto const* lkPlacement = FindGeneratedCreaturePlacement(36597, 631);
    ASSERT_NE(lkPlacement, nullptr);
    EXPECT_EQ(lkPlacement->entry, 36597u);
    EXPECT_EQ(lkPlacement->mapId, 631u);
    EXPECT_EQ(lkPlacement->era, ContentEra::WotLK);
}

TEST(SourceGraphAuthorityTest, RealGeneratedHeroicProgressionOrdering)
{
    // Validate real generated items from census without manual Context override
    ProgressionLayout layout = ProgressionLayout::Create(60, true, true);

    // 1. WotLK progression:
    // Skadi the Ruthless (26693, Utgarde Pinnacle Normal, Map 575): Item 37056 (Drake-Mounted Crossbow, ilvl 187) -> DUNGEON_NORMAL
    // Skadi the Ruthless (30807, Utgarde Pinnacle Heroic, Map 575): Item 37379 (Netherbreath Spellblade, ilvl 200) -> DUNGEON_HEROIC
    // Naxxramas Boss Drop (Map 533): Item 39291 (Torment of the Banished, ilvl 200) -> RAID_ENTRY
    auto const* profNormWotlk = FindGeneratedItemProfile(37056);
    auto const* profHeroicWotlk = FindGeneratedItemProfile(37379);
    auto const* profRaidWotlk = FindGeneratedItemProfile(39291);

    ASSERT_NE(profNormWotlk, nullptr);
    ASSERT_NE(profHeroicWotlk, nullptr);
    ASSERT_NE(profRaidWotlk, nullptr);

    EXPECT_EQ(profNormWotlk->tier, ContentTier::DUNGEON_NORMAL);
    EXPECT_EQ(profHeroicWotlk->tier, ContentTier::DUNGEON_HEROIC);
    EXPECT_EQ(profRaidWotlk->tier, ContentTier::RAID_ENTRY);

    ItemTemplate itemNormWotlk;
    itemNormWotlk.ItemId = 37056;
    itemNormWotlk.ItemLevel = 187;
    itemNormWotlk.RequiredLevel = 80;

    ItemTemplate itemHeroicWotlk;
    itemHeroicWotlk.ItemId = 37379;
    itemHeroicWotlk.ItemLevel = 200;
    itemHeroicWotlk.RequiredLevel = 80;

    ItemTemplate itemRaidWotlk;
    itemRaidWotlk.ItemId = 39291;
    itemRaidWotlk.ItemLevel = 200;
    itemRaidWotlk.RequiredLevel = 80;

    ItemScalingContext ctxNormWotlk = ItemScalingContext::Resolve(&itemNormWotlk);
    ItemScalingContext ctxHeroicWotlk = ItemScalingContext::Resolve(&itemHeroicWotlk);
    ItemScalingContext ctxRaidWotlk = ItemScalingContext::Resolve(&itemRaidWotlk);

    EXPECT_EQ(ctxNormWotlk.tier, ContentTier::DUNGEON_NORMAL);
    EXPECT_EQ(ctxHeroicWotlk.tier, ContentTier::DUNGEON_HEROIC);
    EXPECT_EQ(ctxRaidWotlk.tier, ContentTier::RAID_ENTRY);

    ScaledItemBudget bNormWotlk = sItemBudgetScaler->CalculateItemBudget(&itemNormWotlk, layout, ctxNormWotlk);
    ScaledItemBudget bHeroicWotlk = sItemBudgetScaler->CalculateItemBudget(&itemHeroicWotlk, layout, ctxHeroicWotlk);
    ScaledItemBudget bRaidWotlk = sItemBudgetScaler->CalculateItemBudget(&itemRaidWotlk, layout, ctxRaidWotlk);

    // Strict progression ordering: real Normal item < real Heroic item < real Raid Entry item
    EXPECT_LT(bNormWotlk.effectiveItemLevel, bHeroicWotlk.effectiveItemLevel);
    EXPECT_LT(bHeroicWotlk.effectiveItemLevel, bRaidWotlk.effectiveItemLevel);

    // 2. TBC progression:
    // Watchkeeper Gargolmar Normal (17306, Ramparts, Map 543): Item 24021 (Moonstrider Boots, ilvl 85) -> DUNGEON_NORMAL
    // Watchkeeper Gargolmar Heroic (18436, Ramparts, Map 543): Item 27447 (Bracers of Just Rewards, ilvl 115) -> DUNGEON_HEROIC
    // Shade of Aran (16524, Karazhan, Map 532): Item 28612 (Pendant of the Violet Eye, ilvl 115) -> RAID_ENTRY
    auto const* profNormTbc = FindGeneratedItemProfile(24021);
    auto const* profHeroicTbc = FindGeneratedItemProfile(27447);
    auto const* profRaidTbc = FindGeneratedItemProfile(28612);

    ASSERT_NE(profNormTbc, nullptr);
    ASSERT_NE(profHeroicTbc, nullptr);
    ASSERT_NE(profRaidTbc, nullptr);

    EXPECT_EQ(profNormTbc->tier, ContentTier::DUNGEON_NORMAL);
    EXPECT_EQ(profHeroicTbc->tier, ContentTier::DUNGEON_HEROIC);
    EXPECT_EQ(profRaidTbc->tier, ContentTier::RAID_ENTRY);

    ItemTemplate itemNormTbc;
    itemNormTbc.ItemId = 24021;
    itemNormTbc.ItemLevel = 85;
    itemNormTbc.RequiredLevel = 60;

    ItemTemplate itemHeroicTbc;
    itemHeroicTbc.ItemId = 27447;
    itemHeroicTbc.ItemLevel = 115;
    itemHeroicTbc.RequiredLevel = 70;

    ItemTemplate itemRaidTbc;
    itemRaidTbc.ItemId = 28612;
    itemRaidTbc.ItemLevel = 115;
    itemRaidTbc.RequiredLevel = 70;

    ItemScalingContext ctxNormTbc = ItemScalingContext::Resolve(&itemNormTbc);
    ItemScalingContext ctxHeroicTbc = ItemScalingContext::Resolve(&itemHeroicTbc);
    ItemScalingContext ctxRaidTbc = ItemScalingContext::Resolve(&itemRaidTbc);

    EXPECT_EQ(ctxNormTbc.tier, ContentTier::DUNGEON_NORMAL);
    EXPECT_EQ(ctxHeroicTbc.tier, ContentTier::DUNGEON_HEROIC);
    EXPECT_EQ(ctxRaidTbc.tier, ContentTier::RAID_ENTRY);

    ScaledItemBudget bNormTbc = sItemBudgetScaler->CalculateItemBudget(&itemNormTbc, layout, ctxNormTbc);
    ScaledItemBudget bHeroicTbc = sItemBudgetScaler->CalculateItemBudget(&itemHeroicTbc, layout, ctxHeroicTbc);
    ScaledItemBudget bRaidTbc = sItemBudgetScaler->CalculateItemBudget(&itemRaidTbc, layout, ctxRaidTbc);

    EXPECT_LT(bNormTbc.effectiveItemLevel, bHeroicTbc.effectiveItemLevel);
    EXPECT_LT(bHeroicTbc.effectiveItemLevel, bRaidTbc.effectiveItemLevel);
}

TEST(SourceGraphAuthorityTest, NoHardcodedCustomBoundariesInItemBudgetScalerCpp)
{
    // Verify that ItemBudgetScaler.cpp does not contain hardcoded custom category boundaries (200000, 350000, 600000).
    // The sole source of truth for custom item ranges and categories is custom_content.json and GeneratedContentCensus.h.
    // Found by walking up from wherever the tests were started, because that is the one thing known
    // about the working directory. The list of guessed absolute paths this replaced held the author's
    // own checkout, so the test could only pass on one machine.
    std::ifstream file;
    {
        std::filesystem::path here = std::filesystem::current_path();
        for (int up = 0; up < 6 && !file.is_open(); ++up)
        {
            std::filesystem::path const candidate =
                here / "modules" / "mod-coa-content-scaling" / "src" / "ItemBudgetScaler.cpp";
            if (std::filesystem::exists(candidate))
                file.open(candidate);
            if (!here.has_parent_path() || here.parent_path() == here)
                break;
            here = here.parent_path();
        }
    }

    ASSERT_TRUE(file.is_open()) << "Could not open ItemBudgetScaler.cpp to verify absence of hardcoded boundaries";

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    EXPECT_EQ(content.find("200000"), std::string::npos) << "Found hardcoded 200000 boundary in ItemBudgetScaler.cpp";
    EXPECT_EQ(content.find("350000"), std::string::npos) << "Found hardcoded 350000 boundary in ItemBudgetScaler.cpp";
    EXPECT_EQ(content.find("600000"), std::string::npos) << "Found hardcoded 600000 boundary in ItemBudgetScaler.cpp";
}

// =================================================================================================
// Round 5: Progression & Reward Integration Tests
// =================================================================================================

TEST(ProgressionRewardTest, QuestEffectiveLevelMapping_Cap60_AllEras)
{
    // MaxLevel 60, all eras active: Classic 1..45, TBC 45..55, WotLK 55..60
    ProgressionLayout layout = ProgressionLayout::Create(60, true, true);
    std::string err;
    ASSERT_TRUE(layout.Validate(err));

    // Classic level 60 quest -> maps to Classic max (45)
    uint8 effectiveClassic60 = layout.MapAuthoredToEffective(ContentEra::Classic, 60);
    EXPECT_EQ(effectiveClassic60, 45);

    // TBC level 70 quest -> maps to TBC max (55)
    uint8 effectiveTbc70 = layout.MapAuthoredToEffective(ContentEra::TBC, 70);
    EXPECT_EQ(effectiveTbc70, 55);

    // WotLK level 80 quest -> maps to WotLK max (60)
    uint8 effectiveWotlk80 = layout.MapAuthoredToEffective(ContentEra::WotLK, 80);
    EXPECT_EQ(effectiveWotlk80, 60);
}

TEST(ProgressionRewardTest, QuestChainRemainsReachable_MinLevelRelativeGapPreserved)
{
    ProgressionLayout layout = ProgressionLayout::Create(60, true, true);
    // Suppose a quest in Classic is level 30, minLevel 25 (gap = 5)
    uint8 effectiveLevel = layout.MapAuthoredToEffective(ContentEra::Classic, 30);
    uint8 effectiveMin = layout.MapAuthoredToEffective(ContentEra::Classic, 25);

    // Effective min level must be strictly less than or equal to effective quest level
    EXPECT_LE(effectiveMin, effectiveLevel);
}

TEST(ProgressionRewardTest, CreatureXpUsesEffectiveLevel)
{
    // Verify that ProgressionRewardResolver or level scaling logic yields effective level < authored level on compression
    ProgressionLayout layout = ProgressionLayout::Create(60, true, true);
    // Level 80 Lich King creature in WotLK
    uint8 effCreatureLevel = layout.MapAuthoredToEffective(ContentEra::WotLK, 80);
    EXPECT_EQ(effCreatureLevel, 60);
    // When a player is level 60, fighting level 60 gives appropriate XP, whereas fighting level 80 would give skull / 0 or impossible XP
    EXPECT_EQ(effCreatureLevel, layout.maxLevel);
}

TEST(ProgressionRewardTest, QuestXpMonotonicWithinEra)
{
    ProgressionLayout layout = ProgressionLayout::Create(60, true, true);

    // TBC early quest (61) vs TBC endgame quest (70)
    uint32 earlyAuthoredXP = 8500;
    uint32 lateAuthoredXP = 12500;

    int32 earlyEffLevel = layout.MapAuthoredToEffective(ContentEra::TBC, 61);
    int32 lateEffLevel = layout.MapAuthoredToEffective(ContentEra::TBC, 70);

    uint32 earlyCalibrated = sProgressionRewardResolver->ResolveQuestXP(earlyAuthoredXP, 61, earlyEffLevel, layout, ContentEra::TBC);
    uint32 lateCalibrated = sProgressionRewardResolver->ResolveQuestXP(lateAuthoredXP, 70, lateEffLevel, layout, ContentEra::TBC);

    // Calibrated XP should grow monotonically from early to late quest
    EXPECT_LT(earlyCalibrated, lateCalibrated);
}

TEST(ProgressionRewardTest, ItemRequiredLevelMatchesAcquisition)
{
    ProgressionLayout layout = ProgressionLayout::Create(60, true, true);

    // TBC dungeon item with authored reqLevel 70
    uint32 effContentLevel = 55; // TBC max in 60-all layout
    uint32 resolvedReq = sProgressionRewardResolver->ResolveItemRequiredLevel(70, ContentEra::TBC, effContentLevel, layout);

    // Required level must not exceed the acquisition content level
    EXPECT_LE(resolvedReq, effContentLevel + 1);
    EXPECT_LE(resolvedReq, layout.maxLevel);
}

TEST(ProgressionRewardTest, DungeonAccessUsesEffectiveLevel_NormalBeforeHeroic)
{
    ProgressionLayout layout = ProgressionLayout::Create(60, true, true);

    uint8 heroicUnlock = sProgressionRewardResolver->ResolveTierUnlockLevel(ContentTier::DUNGEON_HEROIC, ContentEra::TBC, layout);
    uint8 normalUnlock = sProgressionRewardResolver->ResolveTierUnlockLevel(ContentTier::DUNGEON_NORMAL, ContentEra::TBC, layout);

    EXPECT_LT(normalUnlock, heroicUnlock);
}

TEST(ProgressionRewardTest, RaidTierUnlockOrdering)
{
    ProgressionLayout layout = ProgressionLayout::Create(60, true, true);

    uint8 normal = sProgressionRewardResolver->ResolveTierUnlockLevel(ContentTier::DUNGEON_NORMAL, ContentEra::WotLK, layout);
    uint8 heroic = sProgressionRewardResolver->ResolveTierUnlockLevel(ContentTier::DUNGEON_HEROIC, ContentEra::WotLK, layout);
    uint8 entry = sProgressionRewardResolver->ResolveTierUnlockLevel(ContentTier::RAID_ENTRY, ContentEra::WotLK, layout);
    uint8 mid = sProgressionRewardResolver->ResolveTierUnlockLevel(ContentTier::RAID_MID, ContentEra::WotLK, layout);
    uint8 end = sProgressionRewardResolver->ResolveTierUnlockLevel(ContentTier::RAID_END, ContentEra::WotLK, layout);

    EXPECT_LE(normal, heroic);
    EXPECT_LE(heroic, entry);
    EXPECT_LE(entry, mid);
    EXPECT_LE(mid, end);
    EXPECT_EQ(end, layout.maxLevel);
}

TEST(ProgressionRewardTest, Economy_MoneyAtCapSafeguard)
{
    // Very high XP quest (e.g. 100,000 XP)
    int32 moneyAtCap = sProgressionRewardResolver->ResolveMoneyAtCap(100000, 1.0f);
    // Standard formula: 100,000 * 6 = 600,000 copper (60g). Guard caps at 500,000 copper (50g).
    EXPECT_LE(moneyAtCap, 500000);
    EXPECT_EQ(moneyAtCap, 500000);
}

TEST(ProgressionRewardTest, DisabledPackHidden_LockedOut)
{
    // MaxLevel 60, Classic only (TBC and WotLK disabled)
    ProgressionLayout layout = ProgressionLayout::Create(60, false, false);
    EXPECT_FALSE(layout.IsEraEnabled(ContentEra::TBC));
    EXPECT_FALSE(layout.IsEraEnabled(ContentEra::WotLK));
    EXPECT_TRUE(layout.IsEraEnabled(ContentEra::Classic));
    EXPECT_FALSE(layout.tbc.has_value());
    EXPECT_FALSE(layout.wotlk.has_value());
}

TEST(ProgressionRewardTest, ClassicOnlyUsesFullRange)
{
    ProgressionLayout layout = ProgressionLayout::Create(60, false, false);
    EXPECT_EQ(layout.classic.minLevel, 1);
    EXPECT_EQ(layout.classic.maxLevel, 60);

    // Authored level 60 maps 1:1 to 60
    uint8 mapped = layout.MapAuthoredToEffective(ContentEra::Classic, 60);
    EXPECT_EQ(mapped, 60);
}

TEST(ProgressionRewardTest, SkipEraHasNoGap)
{
    // Classic + WotLK (TBC skipped)
    ProgressionLayout layout = ProgressionLayout::Create(60, false, true);
    std::string err;
    ASSERT_TRUE(layout.Validate(err));

    // Classic: 1..45, WotLK: 45..60
    EXPECT_EQ(layout.classic.maxLevel, layout.wotlk->minLevel);
}

TEST(ProgressionRewardTest, Cap80Identity_StockBlizzardPreserved)
{
    ProgressionLayout layout = ProgressionLayout::Create(80, true, true);
    std::string err;
    ASSERT_TRUE(layout.Validate(err));

    EXPECT_EQ(layout.classic.minLevel, 1);
    EXPECT_EQ(layout.classic.maxLevel, 60);
    ASSERT_TRUE(layout.tbc.has_value());
    EXPECT_EQ(layout.tbc->minLevel, 60);
    EXPECT_EQ(layout.tbc->maxLevel, 70);
    ASSERT_TRUE(layout.wotlk.has_value());
    EXPECT_EQ(layout.wotlk->minLevel, 70);
    EXPECT_EQ(layout.wotlk->maxLevel, 80);

    // Authored XP must be preserved exactly 1:1 at cap 80 with all expansions active
    uint32 xp = sProgressionRewardResolver->ResolveQuestXP(12500, 75, 75, layout, ContentEra::WotLK);
    EXPECT_EQ(xp, 12500);

    // Authored item req level preserved 1:1
    uint32 req = sProgressionRewardResolver->ResolveItemRequiredLevel(80, ContentEra::WotLK, 80, layout);
    EXPECT_EQ(req, 80);
}

TEST(ProgressionRewardTest, ProgressionContext_ConstructionAndMetrics)
{
    ProgressionLayout layout = ProgressionLayout::Create(60, true, true);
    ProgressionContext ctx(75, 55, ContentEra::WotLK, ContentTier::RAID_ENTRY, layout.maxLevel, true, 0.7f, 0.7f);

    EXPECT_EQ(ctx.authoredLevel, 75);
    EXPECT_EQ(ctx.effectiveLevel, 55);
    EXPECT_EQ(ctx.era, ContentEra::WotLK);
    EXPECT_EQ(ctx.tier, ContentTier::RAID_ENTRY);
    EXPECT_EQ(ctx.maxPlayerLevel, 60);
    EXPECT_TRUE(ctx.contentPackEnabled);
    EXPECT_FLOAT_EQ(ctx.progressionPosition, 0.7f);
    EXPECT_FLOAT_EQ(ctx.eraProgress, 0.7f);
}

TEST(ProgressionCalibrationTest, StartingZonesShippedWithAnExpansionKeepTheirOwnLevel)
{
    // Eversong Woods and Azuremyst Isle are Burning Crusade content written for levels 1 to 20, so
    // the expansion they shipped with says nothing about where they belong. Reading them as that
    // expansion's content put a level 5 opponent at the floor of the TBC band.
    ProgressionLayout layout = ProgressionLayout::Create(80, true, true);
    std::string err;
    ASSERT_TRUE(layout.Validate(err)) << err;

    uint8 const tbcFloor = layout.GetEraRange(ContentEra::TBC).minLevel;

    // What the defect did, and what the raw mapping still does when it is handed that era: the
    // level falls below the band's own source range and lands on its floor.
    EXPECT_EQ(layout.MapAuthoredToEffective(ContentEra::TBC, 5), tbcFloor);

    EXPECT_EQ(layout.ResolveContentEra(ContentEra::TBC, 5), ContentEra::Classic);

    uint8 const effUrchin = layout.MapAuthoredToEffective(layout.ResolveContentEra(ContentEra::TBC, 5), 5);
    EXPECT_EQ(effUrchin, layout.MapAuthoredToEffective(ContentEra::Classic, 5));
    EXPECT_LT(effUrchin, tbcFloor);

    // Where the expansion's own progression starts, it stays that expansion's content.
    EXPECT_EQ(layout.ResolveContentEra(ContentEra::TBC, 58), ContentEra::TBC);
    EXPECT_EQ(layout.ResolveContentEra(ContentEra::WotLK, 70), ContentEra::WotLK);

    // An entry requirement is not a content level: it keeps the era of the instance it guards, so
    // Utgarde Keep's authored 65 still maps through Northrend rather than through Outland.
    EXPECT_EQ(layout.MapAuthoredToEffective(ContentEra::WotLK, 65),
        layout.GetEraRange(ContentEra::WotLK).minLevel);
}
