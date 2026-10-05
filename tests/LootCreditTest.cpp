/*
 * CoA Universal Content Scaling
 * LootCreditTest: a scaled creature keeps a player damage requirement its players can meet.
 * Taken from upstream (Corfirean/mod-coa-content-scaling 60bba0bf).
 */

#include "CombatBudgetProfile.h"
#include "TestCreature.h"
#include "WorldMock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace testing;

class ScaledLootCreditTest : public Test
{
protected:
    void SetUp() override
    {
        previousWorld = std::move(sWorld);
        auto* world = new NiceMock<WorldMock>();
        ON_CALL(*world, getIntConfig(_)).WillByDefault(Return(0));
        ON_CALL(*world, getFloatConfig(_)).WillByDefault(Return(1.0f));
        ON_CALL(*world, getBoolConfig(_)).WillByDefault(Return(false));
        sWorld.reset(world);
    }

    void TearDown() override { sWorld = std::move(previousWorld); }
    std::unique_ptr<IWorld> previousWorld;
};

TEST_F(ScaledLootCreditTest, DownscaledCreatureCanRewardItsPlayerKiller)
{
    TestCreature creature;
    creature.ForceInitValues(1, 23953);
    creature.SetMaxHealth(80000);
    creature.SetHealth(80000);
    creature.ResetPlayerDamageReq();
    creature.SetMaxHealth(8000);
    creature.SetHealth(8000);
    RescaleLootDamageRequirement(&creature, 80000);
    EXPECT_EQ(creature.GetPlayerDamageReq(), 4000u);
    EXPECT_FALSE(creature.IsDamageEnoughForLootingAndReward());
    creature.LowerPlayerDamageReq(8000, true, 55);
    EXPECT_TRUE(creature.IsDamageEnoughForLootingAndReward());
}

TEST_F(ScaledLootCreditTest, EncounterRescalingPreservesExistingCredit)
{
    TestCreature creature;
    creature.ForceInitValues(2, 23953);
    creature.SetMaxHealth(80000);
    creature.SetHealth(80000);
    creature.ResetPlayerDamageReq();
    creature.LowerPlayerDamageReq(10000, true, 55);
    creature.SetMaxHealth(8000);
    RescaleLootDamageRequirement(&creature, 80000);
    EXPECT_EQ(creature.GetPlayerDamageReq(), 3000u);
    EXPECT_EQ(creature.GetHighestPlayerAttackerLevel(), 55);
    creature.LowerPlayerDamageReq(3000);
    EXPECT_TRUE(creature.IsDamageEnoughForLootingAndReward());
}

TEST_F(ScaledLootCreditTest, ScalingDoesNotManufacturePlayerCredit)
{
    TestCreature creature;
    creature.ForceInitValues(3, 23953);
    creature.SetMaxHealth(80000);
    creature.SetHealth(80000);
    creature.ResetPlayerDamageReq();
    creature.SetMaxHealth(8000);
    RescaleLootDamageRequirement(&creature, 80000);
    creature.LowerPlayerDamageReq(8000, false);
    EXPECT_FALSE(creature.IsDamageEnoughForLootingAndReward());
}

TEST_F(ScaledLootCreditTest, UpscaledCreatureKeepsItsRequirement)
{
    TestCreature creature;
    creature.ForceInitValues(4, 23953);
    creature.SetMaxHealth(8000);
    creature.SetHealth(8000);
    creature.ResetPlayerDamageReq();
    creature.SetMaxHealth(80000);
    RescaleLootDamageRequirement(&creature, 8000);
    EXPECT_EQ(creature.GetPlayerDamageReq(), 4000u);
}
