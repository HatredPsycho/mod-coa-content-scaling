/*
 * CoA Universal Content Scaling
 * LiftedItemScalingTest: a lifted copy of a cut item is cut from its own levels, never above authored strength.
 */

#include "ItemBudgetScaler.h"
#include "gtest/gtest.h"

namespace
{
    // Sharpened Twilight Scale on a cap-60 realm: authored item level 284 (required 80), cut to 78 (required 60).
    AppliedItemScaling CutTrinket()
    {
        AppliedItemScaling cut;
        cut.authoredRequiredLevel = 80;
        cut.effectiveRequiredLevel = 60;
        cut.authoredItemLevel = 284;
        cut.effectiveItemLevel = 78;
        cut.statMultiplier = 78.0f / 284.0f;
        cut.ratingMultiplier = cut.statMultiplier * (60.0f / 80.0f) * (60.0f / 80.0f);
        return cut;
    }
}

TEST(LiftedItemScaling, ACopyAtTheCutLevelsKeepsTheCut)
{
    AppliedItemScaling const cut = CutTrinket();
    AppliedItemScaling const copy = ItemBudgetScaler::LiftedScaling(cut, 78, 60);
    EXPECT_FLOAT_EQ(copy.statMultiplier, cut.statMultiplier);
    EXPECT_FLOAT_EQ(copy.ratingMultiplier, cut.ratingMultiplier);
}

TEST(LiftedItemScaling, ALiftedCopyIsCutLessButStillCut)
{
    AppliedItemScaling const cut = CutTrinket();
    AppliedItemScaling const copy = ItemBudgetScaler::LiftedScaling(cut, 98, 60);
    EXPECT_GT(copy.statMultiplier, cut.statMultiplier);
    EXPECT_LT(copy.statMultiplier, 1.0f);
    EXPECT_FLOAT_EQ(copy.statMultiplier, 98.0f / 284.0f);
    EXPECT_EQ(copy.effectiveItemLevel, 98u);
    EXPECT_EQ(copy.authoredItemLevel, 284u);
}

TEST(LiftedItemScaling, ACopyNeverExceedsAuthoredStrength)
{
    AppliedItemScaling const copy = ItemBudgetScaler::LiftedScaling(CutTrinket(), 300, 85);
    EXPECT_FLOAT_EQ(copy.statMultiplier, 1.0f);
    EXPECT_FLOAT_EQ(copy.ratingMultiplier, 1.0f);
}

TEST(LiftedItemScaling, AnItemWithoutAnAuthoredLevelKeepsItsRecord)
{
    AppliedItemScaling uncut;
    uncut.statMultiplier = 0.5f;
    AppliedItemScaling const copy = ItemBudgetScaler::LiftedScaling(uncut, 120, 60);
    EXPECT_FLOAT_EQ(copy.statMultiplier, 0.5f);
    EXPECT_EQ(copy.effectiveItemLevel, 0u);
}
