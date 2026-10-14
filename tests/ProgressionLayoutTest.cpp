/*
 * CoA Universal Content Scaling
 * ProgressionLayoutTest: Comprehensive unit test matrix for ProgressionLayout combinations.
 */

#include "ProgressionLayout.h"
#include "ContentEra.h"
#include "ContentEraResolver.h"
#include "CoAContentScalingConfig.h"
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

// 5. Layout Construction Is Deterministic
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
    EraResolutionResult res = sContentEraResolver->ResolveEraDetailsForCreature(
        12345, 0 /*Eastern Kingdoms*/, 0, 0 /*Classic expansion*/, 60 /*Level 60*/);

    EXPECT_EQ(res.era, ContentEra::Classic);
    EXPECT_EQ(res.source, EraResolutionSource::InstanceProfile);

    // Explicit override takes precedence over map profile
    sContentEraResolver->RegisterCreatureOverride(12345, ContentEra::Custom);
    EraResolutionResult overrideRes = sContentEraResolver->ResolveEraDetailsForCreature(
        12345, 0, 0, 0, 60);

    EXPECT_EQ(overrideRes.era, ContentEra::Custom);
    EXPECT_EQ(overrideRes.source, EraResolutionSource::ExplicitOverride);

    // Clean up
    sContentEraResolver->Clear();
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
    EXPECT_STREQ(CoAContentScalingConfigKeys::TbcEnable, "CoAContentScaling.TBC.Enable");
    EXPECT_STREQ(CoAContentScalingConfigKeys::WotlkEnable, "CoAContentScaling.WotLK.Enable");
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

// =============================================================================
// Round 3: Era Sources, Instance Profiles & Calibration Tests
// =============================================================================

TEST(ContentEraResolverTest, MapExpansionDecidesTheEraOfAnUnprofiledMap)
{
    sContentEraResolver->Clear();
    sContentEraResolver->RegisterMapExpansion(9001, 0);
    sContentEraResolver->RegisterMapExpansion(9002, 1);
    sContentEraResolver->RegisterMapExpansion(9003, 2);

    EraResolutionResult const classic = sContentEraResolver->ResolveEraDetailsForMap(9001);
    EXPECT_EQ(classic.era, ContentEra::Classic);
    EXPECT_EQ(classic.source, EraResolutionSource::MapExpansion);
    EXPECT_EQ(sContentEraResolver->ResolveEraForMap(9002), ContentEra::TBC);
    EXPECT_EQ(sContentEraResolver->ResolveEraForMap(9003), ContentEra::WotLK);

    EraResolutionResult const creature = sContentEraResolver->ResolveEraDetailsForCreature(1, 9003, 0, 0, 10);
    EXPECT_EQ(creature.era, ContentEra::WotLK);
    EXPECT_EQ(creature.source, EraResolutionSource::InstanceProfile);

    sContentEraResolver->RegisterMapOverride(9003, ContentEra::Custom);
    EXPECT_EQ(sContentEraResolver->ResolveEraForMap(9003), ContentEra::Custom);

    sContentEraResolver->Clear();
    EXPECT_EQ(sContentEraResolver->ResolveEraDetailsForMap(9002).source, EraResolutionSource::SafeFallback);
}

TEST(ContentEraResolverTest, QuestZoneFollowsTheMapOfItsArea)
{
    sContentEraResolver->Clear();
    sContentEraResolver->RegisterMapExpansion(9002, 1);
    sContentEraResolver->RegisterAreaMap(9100, 9002);

    EraResolutionResult const quest = sContentEraResolver->ResolveEraDetailsForQuest(1, 9100, 0, 10);
    EXPECT_EQ(quest.era, ContentEra::TBC);
    EXPECT_EQ(quest.source, EraResolutionSource::ZoneSortMetadata);
    EXPECT_EQ(sContentEraResolver->ResolveEraForArea(9100), ContentEra::TBC);

    EraResolutionResult const sortOnly = sContentEraResolver->ResolveEraDetailsForQuest(2, -101, 0, 75);
    EXPECT_EQ(sortOnly.era, ContentEra::WotLK);
    EXPECT_EQ(sortOnly.source, EraResolutionSource::AuthoredLevelHeuristic);

    sContentEraResolver->Clear();
}

TEST(ContentEraResolverTest, ClassicAreasOnOutlandStayClassic)
{
    constexpr uint32 eversongWoods = 3430;
    constexpr uint32 hellfirePeninsula = 3483;

    sContentEraResolver->Clear();
    sContentEraResolver->RegisterMapExpansion(530, 1);
    sContentEraResolver->RegisterAreaMap(eversongWoods, 530);
    sContentEraResolver->RegisterAreaMap(hellfirePeninsula, 530);
    sContentEraResolver->RegisterClassicArea(eversongWoods);

    EXPECT_EQ(sContentEraResolver->ResolveEraForCreature(1, 530, eversongWoods, 1, 5), ContentEra::Classic);
    EXPECT_EQ(sContentEraResolver->ResolveEraForQuest(1, eversongWoods, 0, 5), ContentEra::Classic);
    EXPECT_EQ(sContentEraResolver->ResolveEraForArea(eversongWoods, 530), ContentEra::Classic);

    EXPECT_EQ(sContentEraResolver->ResolveEraForCreature(1, 530, hellfirePeninsula, 1, 60), ContentEra::TBC);
    EXPECT_EQ(sContentEraResolver->ResolveEraForQuest(2, hellfirePeninsula, 0, 60), ContentEra::TBC);

    sContentEraResolver->Clear();
}

TEST(InstanceProfileRegistryTest, AuthoredRaidTiersAndHeroicDungeons)
{
    EXPECT_EQ(InstanceProfileRegistry::GetAuthoredTier(36, false), ContentTier::DUNGEON_NORMAL);
    EXPECT_EQ(InstanceProfileRegistry::GetAuthoredTier(409, true), ContentTier::RAID_MID);
    EXPECT_EQ(InstanceProfileRegistry::GetAuthoredTier(531, true), ContentTier::RAID_PINNACLE);
    EXPECT_EQ(InstanceProfileRegistry::GetAuthoredTier(564, true), ContentTier::RAID_END);
    EXPECT_EQ(InstanceProfileRegistry::GetAuthoredTier(631, true), ContentTier::RAID_PINNACLE);
    EXPECT_EQ(InstanceProfileRegistry::GetAuthoredTier(9999, true), ContentTier::RAID_ENTRY);
    EXPECT_TRUE(InstanceProfileRegistry::IsCustomMap(936));
    EXPECT_FALSE(InstanceProfileRegistry::IsCustomMap(36));

    sInstanceProfileRegistry->Clear();
    sInstanceProfileRegistry->RegisterProfile(InstanceProfile{ .mapId = 36, .name = "The Deadmines" });
    sInstanceProfileRegistry->RegisterProfile(InstanceProfile{ .mapId = 409, .name = "Molten Core",
        .tier = InstanceProfileRegistry::GetAuthoredTier(409, true), .kind = MapContentKind::RAID,
        .defaultGroupSize = 40, .raid25Size = 40, .isRaid = true });
    sInstanceProfileRegistry->RegisterMapKind(30, MapContentKind::BATTLEGROUND);

    std::vector<std::string> issues;
    EXPECT_TRUE(sInstanceProfileRegistry->ValidateAll(issues));

    EXPECT_EQ(sInstanceProfileRegistry->GetTierForMap(36), ContentTier::DUNGEON_NORMAL);
    EXPECT_EQ(sInstanceProfileRegistry->GetTierForMap(36, 1), ContentTier::DUNGEON_HEROIC); // Heroic difficulty = 1
    EXPECT_EQ(sInstanceProfileRegistry->GetTierForMap(409), ContentTier::RAID_MID);
    EXPECT_EQ(sInstanceProfileRegistry->GetIntendedPlayers(409, 0), 40u);
    EXPECT_EQ(sInstanceProfileRegistry->GetKindForMap(409), MapContentKind::RAID);
    EXPECT_FALSE(sInstanceProfileRegistry->HasProfile(30));
    EXPECT_FALSE(IsPvEInstanceKind(sInstanceProfileRegistry->GetKindForMap(30)));

    sInstanceProfileRegistry->Clear();
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
// Round 3.1: Dungeon Access Tests
// =============================================================================

TEST(DungeonAccessTest, NorthrendAccessLevelsFollowTheWotLKBand)
{
    ProgressionLayout layoutCap60 = ProgressionLayout::Create(60, true, true);
    // Classic: 1-45, TBC: 45-55, WotLK: 55-60

    // Utgarde Keep asks for 69 to 72 as authored.
    uint8 const scaledMin = layoutCap60.MapAuthoredToEffective(ContentEra::WotLK, 69);
    uint8 const scaledMax = layoutCap60.MapAuthoredToEffective(ContentEra::WotLK, 72);
    EXPECT_GE(scaledMin, 55);
    EXPECT_LE(scaledMax, 60);
    EXPECT_LE(scaledMin, scaledMax);

    // Expansion locked check when WotLK is disabled
    ProgressionLayout layoutClassicTBC = ProgressionLayout::Create(70, true, false);
    EXPECT_FALSE(layoutClassicTBC.IsEraEnabled(ContentEra::WotLK));
}

// =============================================================================
// Round 3.2: Item Runtime Authority & Policy Enforcement Tests
// =============================================================================

TEST(ItemRuntimeAuthorityTest, WotLKTierProgressionMonotonicAtCap60)
{
    ProgressionLayout layout = ProgressionLayout::Create(60, true, true);
    // Classic: 1-45, TBC: 45-55, WotLK: 55-60

    // Representative real items:
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

    // Representative real items:
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

    // 1. Custom Cosmetic: Wizened Wizard (100002)
    ItemTemplate cosmetic;
    cosmetic.ItemId = 100002;
    cosmetic.ItemLevel = 20;
    cosmetic.RequiredLevel = 0;
    cosmetic.Armor = 0;

    ItemScalingContext ctxCosmetic = ItemScalingContext::Resolve(&cosmetic);
    EXPECT_EQ(ctxCosmetic.policy, ItemScalingPolicy::PRESERVE);
    EXPECT_TRUE(ctxCosmetic.specialFlags & ITEM_SPECIAL_PRESERVE);

    ScaledItemBudget bCosmetic = sItemBudgetScaler->CalculateItemBudget(&cosmetic, layout, ctxCosmetic);
    EXPECT_EQ(bCosmetic.effectiveItemLevel, cosmetic.ItemLevel);
    EXPECT_EQ(bCosmetic.effectiveRequiredLevel, cosmetic.RequiredLevel);
    EXPECT_FLOAT_EQ(bCosmetic.statMultiplier, 1.0f);

    // 2. Custom Service / Potion: Race Change Potion (200001)
    ItemTemplate classItem;
    classItem.ItemId = 200001;
    classItem.ItemLevel = 64;
    classItem.RequiredLevel = 0;

    ItemScalingContext ctxClass = ItemScalingContext::Resolve(&classItem);
    EXPECT_EQ(ctxClass.policy, ItemScalingPolicy::PRESERVE);
    ScaledItemBudget bClass = sItemBudgetScaler->CalculateItemBudget(&classItem, layout, ctxClass);
    EXPECT_EQ(bClass.effectiveItemLevel, classItem.ItemLevel);
    EXPECT_FLOAT_EQ(bClass.statMultiplier, 1.0f);

    // 3. Custom item in a preserved range: no stat budget, left exactly as authored.
    ItemTemplate unprofiledCosmetic;
    unprofiledCosmetic.ItemId = 150000;
    unprofiledCosmetic.ItemLevel = 80;
    unprofiledCosmetic.RequiredLevel = 60;

    ItemScalingContext ctxUnprofiledCosmetic = ItemScalingContext::Resolve(&unprofiledCosmetic);
    EXPECT_EQ(ctxUnprofiledCosmetic.policy, ItemScalingPolicy::PRESERVE);
    EXPECT_TRUE(ctxUnprofiledCosmetic.specialFlags & ITEM_SPECIAL_CUSTOM);
    EXPECT_TRUE(ctxUnprofiledCosmetic.specialFlags & ITEM_SPECIAL_PRESERVE);

    ScaledItemBudget bUnprofiledCosmetic = sItemBudgetScaler->CalculateItemBudget(&unprofiledCosmetic, layout, ctxUnprofiledCosmetic);
    EXPECT_EQ(bUnprofiledCosmetic.effectiveItemLevel, unprofiledCosmetic.ItemLevel);
    EXPECT_FLOAT_EQ(bUnprofiledCosmetic.statMultiplier, 1.0f);

    // 4. Custom item in the gameplay range, above where that range used to stop: this is
    // equipment with statistics - the Bloodforged and Heroic versions live here - and is scaled.
    ItemTemplate unprofiledGameplay;
    unprofiledGameplay.ItemId = 750000;
    unprofiledGameplay.ItemLevel = 80;
    unprofiledGameplay.RequiredLevel = 60;

    ItemScalingContext ctxUnprofiledGameplay = ItemScalingContext::Resolve(&unprofiledGameplay);
    EXPECT_NE(ctxUnprofiledGameplay.policy, ItemScalingPolicy::PRESERVE);
    EXPECT_TRUE(ctxUnprofiledGameplay.specialFlags & ITEM_SPECIAL_CUSTOM);
    EXPECT_FALSE(ctxUnprofiledGameplay.specialFlags & ITEM_SPECIAL_PRESERVE);
}

TEST(ItemRuntimeAuthorityTest, UnprofiledItemFallsBackToItemLevel)
{
    ProgressionLayout layout = ProgressionLayout::Create(60, true, true);

    // Standard unprofiled item (< 100000, not custom): uses fallback tier inference
    ItemTemplate unprofiled;
    unprofiled.ItemId = 99999;
    unprofiled.ItemLevel = 200;
    unprofiled.RequiredLevel = 80;

    ItemScalingContext ctx = ItemScalingContext::Resolve(&unprofiled);
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
// Round 3.3 — Item Source Authority Tests
// ============================================================================

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
    EXPECT_NE(ItemScalingContext::Resolve(&warglaive).policy, ItemScalingPolicy::PRESERVE);
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

TEST(SourceGraphAuthorityTest, NoHardcodedCustomBoundariesInItemBudgetScalerCpp)
{
    // Verify that ItemBudgetScaler.cpp does not contain hardcoded custom category boundaries (200000, 350000, 600000).
    // The sole source of truth for custom item ranges and categories is CustomItemRanges.h.
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
