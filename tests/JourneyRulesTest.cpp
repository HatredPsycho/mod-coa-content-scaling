/*
 * CoA Universal Content Scaling
 * JourneyRulesTest: the journey layout's arithmetic, its tables and the preset built on them.
 */

#include "CoAContentScalingConfig.h"
#include "ContentEraResolver.h"
#include "JourneyData.h"
#include "JourneyRules.h"
#include "ProgressionLayout.h"
#include "gtest/gtest.h"
#include <map>

namespace
{
    journey::Rules DefaultRules()
    {
        return journey::Rules(journey::Shape{});
    }
}

TEST(JourneyRulesTest, EachPartIsLaidOverItsOwnRange)
{
    journey::Rules const rules = DefaultRules();

    EXPECT_EQ(rules.Classic(1), 1);
    EXPECT_EQ(rules.Classic(30), 19);
    EXPECT_EQ(rules.Classic(55), 35);

    EXPECT_EQ(rules.Outland(58), 30);
    EXPECT_EQ(rules.Outland(67), 43);
    EXPECT_EQ(rules.Outland(70), 47);
    EXPECT_EQ(rules.Outland(20), rules.Classic(20));

    EXPECT_EQ(rules.Northrend(68), 40);
    EXPECT_EQ(rules.Northrend(77), 55);
    EXPECT_EQ(rules.Northrend(80), 60);
    EXPECT_EQ(rules.Northrend(83), 60);
    EXPECT_EQ(rules.Northrend(40), rules.Classic(40));
}

TEST(JourneyRulesTest, LevelsNeverInvertWithinAPart)
{
    journey::Rules const rules = DefaultRules();
    for (int level = 2; level <= 83; ++level)
    {
        EXPECT_LE(rules.Classic(level - 1), rules.Classic(level)) << level;
        if (level > 58)
            EXPECT_LE(rules.Outland(level - 1), rules.Outland(level)) << level;
        if (level > 68)
            EXPECT_LE(rules.Northrend(level - 1), rules.Northrend(level)) << level;
    }
}

TEST(JourneyRulesTest, ThePartsOverlapWhereTheyMeet)
{
    // Below its own first level an expansion's map holds old-world content, and the old world's last levels
    // reach past where Outland opens: the ranges overlap by design, so the steps at 58 and 68 go down.
    journey::Rules const rules = DefaultRules();
    EXPECT_EQ(rules.Outland(57), rules.Classic(57));
    EXPECT_GT(rules.Outland(57), rules.Outland(58));
    EXPECT_EQ(rules.Northrend(67), rules.Classic(67));
    EXPECT_GT(rules.Northrend(67), rules.Northrend(68));

    // What Outland and Northrend sent into the old world above 60 is laid over their ranges.
    EXPECT_EQ(rules.Compress(61, journey::ERA_CLASSIC), rules.Outland(61));
    EXPECT_LT(rules.Compress(61, journey::ERA_CLASSIC), rules.Compress(60, journey::ERA_CLASSIC));
}

TEST(JourneyRulesTest, RaidsAndHeroicsStandAtTheEndgameWithTheirBossesAbove)
{
    journey::Rules const rules = DefaultRules();

    EXPECT_EQ(rules.Top(70, 70), 60);
    EXPECT_EQ(rules.Top(73, 70), 63);
    EXPECT_EQ(rules.Top(80, 80), 60);
    EXPECT_EQ(rules.Top(83, 80), 63);
    EXPECT_EQ(rules.Top(60, 70), 60);
    EXPECT_EQ(rules.Top(90, 80), 63);

    EXPECT_EQ(rules.Compress(73, journey::ERA_TOP_OUTLAND), 63);
    EXPECT_EQ(rules.Compress(55, journey::ERA_TOP_OUTLAND), 55);
    EXPECT_EQ(rules.Compress(63, journey::ERA_KEEP), 63);
    EXPECT_EQ(rules.Compress(65, journey::ERA_CLASSIC), rules.Outland(65));
    EXPECT_EQ(rules.Compress(75, journey::ERA_CLASSIC), rules.Northrend(75));
}

TEST(JourneyRulesTest, RegulatorsAreHeldInsideTheirLimits)
{
    journey::Shape shape;
    shape.endgame = 75;
    shape.bossLevels = 9;
    journey::Rules const high(shape);
    EXPECT_EQ(high.GetShape().endgame, 60);
    EXPECT_EQ(high.GetShape().bossLevels, 3);

    shape.endgame = 40;
    shape.bossLevels = -2;
    journey::Rules const low(shape);
    EXPECT_EQ(low.GetShape().endgame, 50);
    EXPECT_EQ(low.GetShape().bossLevels, 0);
    EXPECT_EQ(low.Northrend(80), 50);
}

TEST(JourneyRulesTest, MapsBelongToTheirPartOfTheJourney)
{
    EXPECT_EQ(journey::EraOf(409, 60, false), journey::ERA_KEEP);
    EXPECT_EQ(journey::EraOf(543, 61, false), journey::ERA_OUTLAND);
    EXPECT_EQ(journey::EraOf(543, 70, true), journey::ERA_TOP_OUTLAND);
    EXPECT_EQ(journey::EraOf(564, 70, false), journey::ERA_TOP_OUTLAND);
    EXPECT_EQ(journey::EraOf(574, 70, false), journey::ERA_NORTHREND);
    EXPECT_EQ(journey::EraOf(631, 80, false), journey::ERA_TOP_NORTHREND);
    EXPECT_EQ(journey::EraOf(530, 5, false), journey::ERA_CLASSIC);
    EXPECT_EQ(journey::EraOf(530, 60, false), journey::ERA_OUTLAND);
    EXPECT_EQ(journey::EraOf(571, 70, false), journey::ERA_NORTHREND);
    EXPECT_EQ(journey::EraOf(0, 30, false), journey::ERA_CLASSIC);

    EXPECT_EQ(journey::EraByLevel(60), journey::ERA_CLASSIC);
    EXPECT_EQ(journey::EraByLevel(70), journey::ERA_OUTLAND);
    EXPECT_EQ(journey::EraByLevel(75), journey::ERA_NORTHREND);
    EXPECT_EQ(journey::PartOf(journey::ERA_TOP_NORTHREND), 3);
}

TEST(JourneyRulesTest, ZoneEntriesFollowTheirPartAndHandSetLevelsWin)
{
    journey::Shape shape;
    shape.zoneEntries[3483] = 32;
    journey::Rules const rules(shape);

    EXPECT_EQ(rules.ZoneEntry(14, 0, 1), 1);
    EXPECT_EQ(rules.ZoneEntry(1377, 0, 55), 35);
    EXPECT_EQ(rules.ZoneEntry(3483, 1, 58), 32);
    EXPECT_EQ(rules.ZoneEntry(3483, 1, 58, false), 30);
    EXPECT_EQ(rules.ZoneEntry(3537, 2, 68), 40);
    EXPECT_EQ(rules.ZoneEntry(4080, 1, 70), 55);
}

TEST(JourneyRulesTest, InstanceEntriesFollowTheirContent)
{
    journey::Shape shape;
    shape.instanceEntries[547] = 33;
    journey::Rules const rules(shape);

    EXPECT_EQ(rules.InstanceEntry(36, journey::INSTANCE_DUNGEON, 10, 18), 9);
    EXPECT_EQ(rules.InstanceEntry(543, journey::INSTANCE_DUNGEON, 55, 61), 30);
    EXPECT_EQ(rules.InstanceEntry(547, journey::INSTANCE_DUNGEON, 55, 63), 33);
    EXPECT_EQ(rules.InstanceEntry(543, journey::INSTANCE_HEROIC, 70, 72), 60);
    EXPECT_EQ(rules.InstanceEntry(564, journey::INSTANCE_RAID, 70, 72), 60);
    EXPECT_EQ(rules.InstanceEntry(409, journey::INSTANCE_RAID, 50, 62), 50);
    EXPECT_EQ(rules.InstanceEntry(574, journey::INSTANCE_DUNGEON, 65, 71), 40);
}

TEST(JourneyRulesTest, APartLeftOutKeepsItsLevels)
{
    journey::Shape shape;
    shape.outland = false;
    journey::Rules const rules(shape);

    EXPECT_EQ(rules.Outland(64), 64);
    EXPECT_EQ(rules.ZoneEntry(3483, 1, 58), 58);
    EXPECT_EQ(rules.InstanceEntry(543, journey::INSTANCE_DUNGEON, 55, 61), 55);
    EXPECT_EQ(rules.Compress(73, journey::ERA_TOP_OUTLAND), 73);
    EXPECT_EQ(rules.PlanItem(3, 64, 105).era, journey::ITEM_KEEP);
}

TEST(JourneyRulesTest, ItemsFollowTheLevelTheyWereMadeFor)
{
    journey::Rules const rules = DefaultRules();

    journey::ItemPlan const green = rules.PlanItem(2, 30, 35);
    EXPECT_EQ(green.era, journey::ITEM_CLASSIC);
    EXPECT_EQ(green.requiredLevel, 19);
    EXPECT_EQ(green.itemLevel, 24);

    journey::ItemPlan const classicRaid = rules.PlanItem(4, 60, 76);
    EXPECT_EQ(classicRaid.era, journey::ITEM_KEEP);
    EXPECT_EQ(classicRaid.itemLevel, 76);

    journey::ItemPlan const blackTemple = rules.PlanItem(4, 70, 141);
    EXPECT_EQ(blackTemple.era, journey::ITEM_TBC);
    EXPECT_TRUE(blackTemple.endgame);
    EXPECT_EQ(blackTemple.requiredLevel, 60);
    EXPECT_EQ(blackTemple.itemLevel, 95);

    journey::ItemPlan const icecrown = rules.PlanItem(4, 80, 264);
    EXPECT_EQ(icecrown.era, journey::ITEM_WOTLK);
    EXPECT_TRUE(icecrown.endgame);
    EXPECT_GT(icecrown.itemLevel, blackTemple.itemLevel);
    EXPECT_LE(icecrown.itemLevel, 160);

    journey::ItemPlan const boreanGreen = rules.PlanItem(2, 68, 130);
    EXPECT_EQ(boreanGreen.era, journey::ITEM_WOTLK);
    EXPECT_FALSE(boreanGreen.endgame);
    EXPECT_EQ(boreanGreen.requiredLevel, 40);
    EXPECT_LT(boreanGreen.itemLevel, 130);
}

TEST(JourneyRulesTest, MoneyShrinksWithTheSquareOfTheLevel)
{
    EXPECT_EQ(journey::ScaleMoney(10000, 60, 30), 2500u);
    EXPECT_EQ(journey::ScaleMoney(10000, 30, 60), 10000u);
    EXPECT_EQ(journey::ScaleMoney(1, 80, 10), 1u);
    EXPECT_EQ(journey::ScaleMoney(0, 60, 30), 0u);
}

TEST(JourneyRulesTest, TheWindowPlacesACreatureByItsPlaceInItsZone)
{
    EXPECT_EQ(journey::WindowOffset(20, 20, 0, false, 2, 2), -2);
    EXPECT_EQ(journey::WindowOffset(25, 20, 0, false, 2, 2), 0);
    EXPECT_EQ(journey::WindowOffset(25, 20, 0, true, 2, 2), 1);
    EXPECT_EQ(journey::WindowOffset(30, 20, 0, false, 2, 2), 2);
    EXPECT_EQ(journey::WindowOffset(40, 20, 0, true, 2, 2), 2);
    EXPECT_EQ(journey::WindowOffset(61, 58, 1, false, 2, 2), 0);
}

TEST(JourneyDataTest, TablesHoldEveryZoneAndInstanceOfTheJourney)
{
    std::map<uint32_t, journey::ZoneData> zones;
    for (journey::ZoneData const& zone : journey::Zones)
        zones.emplace(zone.zone, zone);

    EXPECT_EQ(zones.size(), 62u);
    ASSERT_TRUE(zones.count(3430));
    EXPECT_EQ(zones.at(3430).map, 530u);
    EXPECT_EQ(zones.at(3430).part, 0);
    ASSERT_TRUE(zones.count(3483));
    EXPECT_EQ(zones.at(3483).part, 1);
    EXPECT_EQ(zones.at(3483).originalEntry, 58);
    ASSERT_TRUE(zones.count(3537));
    EXPECT_EQ(zones.at(3537).part, 2);

    size_t instances = 0;
    for (journey::InstanceData const& instance : journey::Instances)
    {
        ++instances;
        EXPECT_GE(instance.contentLevel, instance.originalEntry) << instance.name;
    }
    EXPECT_EQ(instances, 121u);
}

TEST(JourneyDataTest, LaidOverTheDefaultShapeEveryEntryStaysInsideTheJourney)
{
    journey::Rules const rules = DefaultRules();
    int const endgame = rules.GetShape().endgame;

    for (journey::ZoneData const& zone : journey::Zones)
    {
        int const entry = rules.ZoneEntry(zone.zone, zone.part, zone.originalEntry);
        EXPECT_GE(entry, 1) << zone.key;
        EXPECT_LE(entry, endgame - 5) << zone.key;

        for (journey::ZoneData const& other : journey::Zones)
            if (other.part == zone.part && other.originalEntry < zone.originalEntry &&
                zone.zone != 4080 && other.zone != 4080)
                EXPECT_LE(rules.ZoneEntry(other.zone, other.part, other.originalEntry), entry)
                    << other.key << " opens above " << zone.key;
    }

    for (journey::InstanceData const& instance : journey::Instances)
    {
        int const entry = rules.InstanceEntry(instance.map, journey::InstanceKind(instance.kind),
            instance.originalEntry, instance.contentLevel);
        EXPECT_GE(entry, 1) << instance.name;
        EXPECT_LE(entry, endgame) << instance.name;
        if (instance.kind == journey::INSTANCE_DUNGEON && journey::IsInstanceOfOutland(instance.map))
            EXPECT_GE(entry, rules.GetShape().outlandRange.from) << instance.name;
        if (instance.kind == journey::INSTANCE_DUNGEON && journey::IsInstanceOfNorthrend(instance.map))
            EXPECT_GE(entry, rules.GetShape().northrendRange.from) << instance.name;
    }
}

TEST(JourneyLayoutTest, ThePresetLaysEachEraOverItsJourneyRange)
{
    ProgressionLayout const layout = ProgressionLayout::CreateJourney(60, true, true, journey::Shape{});
    std::string err;
    ASSERT_TRUE(layout.Validate(err)) << err;

    EXPECT_TRUE(layout.journey);
    EXPECT_FALSE(layout.IsStockIdentity());
    EXPECT_EQ(layout.GetEraRange(ContentEra::Classic).maxLevel, 35);
    EXPECT_EQ(layout.GetEraRange(ContentEra::TBC).minLevel, 30);
    EXPECT_EQ(layout.GetEraRange(ContentEra::TBC).maxLevel, 43);
    EXPECT_EQ(layout.GetEraRange(ContentEra::WotLK).minLevel, 40);
    EXPECT_EQ(layout.GetEraRange(ContentEra::WotLK).maxLevel, 55);

    EXPECT_EQ(layout.MapAuthoredToEffective(ContentEra::Classic, 55), 35);
    EXPECT_EQ(layout.MapAuthoredToEffective(ContentEra::Classic, 65), layout.journeyRules.Outland(65));
    EXPECT_EQ(layout.MapAuthoredToEffective(ContentEra::TBC, 58), 30);
    EXPECT_EQ(layout.MapAuthoredToEffective(ContentEra::WotLK, 77), 55);
    EXPECT_EQ(layout.MapAuthoredToEffective(ContentEra::WotLK, 80), 60);
    EXPECT_EQ(layout.MapAuthoredToEffective(ContentEra::Custom, 70), 60);

    // A starting zone that shipped with an expansion is read as the Classic content it is.
    EXPECT_EQ(layout.MapAuthoredToEffective(layout.ResolveContentEra(ContentEra::TBC, 5), 5),
              layout.MapAuthoredToEffective(ContentEra::Classic, 5));

    // Monotonic within each era's own span: Classic up to 60, the expansions from their first level.
    for (uint8 level = 2; level <= 80; ++level)
    {
        if (level <= 60)
            EXPECT_LE(layout.MapAuthoredToEffective(ContentEra::Classic, level - 1),
                      layout.MapAuthoredToEffective(ContentEra::Classic, level)) << uint32(level);
        if (level > 58)
            EXPECT_LE(layout.MapAuthoredToEffective(ContentEra::TBC, level - 1),
                      layout.MapAuthoredToEffective(ContentEra::TBC, level)) << uint32(level);
        if (level > 68)
            EXPECT_LE(layout.MapAuthoredToEffective(ContentEra::WotLK, level - 1),
                      layout.MapAuthoredToEffective(ContentEra::WotLK, level)) << uint32(level);
    }
}

TEST(JourneyLayoutTest, TheExpansionSwitchesDecideWhatIsLaidOver)
{
    ProgressionLayout const classicOnly = ProgressionLayout::CreateJourney(60, false, false, journey::Shape{});
    std::string err;
    ASSERT_TRUE(classicOnly.Validate(err)) << err;
    EXPECT_FALSE(classicOnly.IsEraEnabled(ContentEra::TBC));
    EXPECT_FALSE(classicOnly.IsEraEnabled(ContentEra::WotLK));
    EXPECT_FALSE(classicOnly.journeyRules.GetShape().outland);

    ProgressionLayout const northrendOnly = ProgressionLayout::CreateJourney(60, false, true, journey::Shape{});
    ASSERT_TRUE(northrendOnly.Validate(err)) << err;
    EXPECT_TRUE(northrendOnly.IsEraEnabled(ContentEra::WotLK));
    EXPECT_EQ(northrendOnly.MapAuthoredToEffective(ContentEra::WotLK, 68), 40);
}

TEST(JourneyLayoutTest, AShapeThatDoesNotFitIsRejected)
{
    std::string err;

    journey::Shape aboveEndgame;
    aboveEndgame.classic = { 1, 62 };
    EXPECT_FALSE(ProgressionLayout::CreateJourney(60, true, true, aboveEndgame).Validate(err));

    journey::Shape notFromOne;
    notFromOne.classic = { 5, 35 };
    EXPECT_FALSE(ProgressionLayout::CreateJourney(60, true, true, notFromOne).Validate(err));

    journey::Shape northrendFirst;
    northrendFirst.northrendRange = { 20, 55 };
    EXPECT_FALSE(ProgressionLayout::CreateJourney(60, true, true, northrendFirst).Validate(err));

    journey::Shape lowEndgame;
    lowEndgame.endgame = 50;
    lowEndgame.northrendRange = { 40, 50 };
    EXPECT_TRUE(ProgressionLayout::CreateJourney(60, true, true, lowEndgame).Validate(err)) << err;
    EXPECT_FALSE(ProgressionLayout::CreateJourney(80, true, true, journey::Shape{}).IsStockIdentity());
}

TEST(JourneyConfigTest, RangesAndTheModeAreRead)
{
    using CoAContentScalingConfig::ParseLevelRange;
    journey::Range const fallback{ 1, 35 };

    journey::Range const read = ParseLevelRange("key", "30-43", fallback);
    EXPECT_EQ(read.from, 30);
    EXPECT_EQ(read.to, 43);

    journey::Range const spaced = ParseLevelRange("key", " 40 - 55 ", fallback);
    EXPECT_EQ(spaced.from, 40);
    EXPECT_EQ(spaced.to, 55);

    for (char const* bad : { "", "abc", "43-30", "0-10", "1-35x", "1-300" })
    {
        journey::Range const kept = ParseLevelRange("key", bad, fallback);
        EXPECT_EQ(kept.from, fallback.from) << bad;
        EXPECT_EQ(kept.to, fallback.to) << bad;
    }

    EXPECT_EQ(CoAContentScalingConfig::ParseProgressionMode("journey"), "Journey");
    EXPECT_EQ(CoAContentScalingConfig::ParseProgressionMode("Journey"), "Journey");
}

TEST(JourneyEraTest, TheZoneTableNamesTheEraOfAZone)
{
    sContentEraResolver->Clear();
    sContentEraResolver->LoadJourneyZones();
    sContentEraResolver->RegisterAreaZone(3431, 3430);

    EraResolutionResult const hellfire = sContentEraResolver->ResolveEraDetailsForQuest(1, 3483, 0, 60);
    EXPECT_EQ(hellfire.era, ContentEra::TBC);
    EXPECT_EQ(hellfire.source, EraResolutionSource::ZoneTable);

    EXPECT_EQ(sContentEraResolver->ResolveEraForQuest(2, 3537, 0, 70), ContentEra::WotLK);
    EXPECT_EQ(sContentEraResolver->ResolveEraForQuest(3, 3430, 0, 5), ContentEra::Classic);
    EXPECT_EQ(sContentEraResolver->ResolveEraForArea(3431), ContentEra::Classic);
    EXPECT_EQ(sContentEraResolver->ResolveEraForArea(4080), ContentEra::TBC);
    EXPECT_EQ(sContentEraResolver->GetZoneEraCount(), 62u);

    sContentEraResolver->RegisterAreaOverride(3483, ContentEra::Classic);
    EXPECT_EQ(sContentEraResolver->ResolveEraForQuest(4, 3483, 0, 60), ContentEra::Classic);

    sContentEraResolver->Clear();
}
