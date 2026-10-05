/*
 * CoA Universal Content Scaling
 * CombatBudgetProfile: Authoritative combat budget calculator reconciling authored expansion
 * multipliers with target CoA effective base stats and tier modifiers.
 */

#ifndef COA_COMBAT_BUDGET_PROFILE_H
#define COA_COMBAT_BUDGET_PROFILE_H

#include "ContentTier.h"
#include "CreatureScaleContext.h"
#include "Define.h"

class Creature;

// Scaling a creature's health leaves the player damage requirement it was given at its old size. Cut
// below half, the creature asks for more damage than it has health, and the kill pays nothing.
void RescaleLootDamageRequirement(Creature* creature, uint32 previousMaxHealth);
struct CreatureTemplate;

struct CalculatedCombatBudget
{
    uint32 health{100};
    uint32 mana{0};
    float minDamage{10.0f};
    float maxDamage{15.0f};
    float armor{100.0f};
    uint32 attackPower{50};
};

class CombatBudgetProfile
{
public:
    static CombatBudgetProfile* Instance();

    // Resolves tier modifier for health, damage, armor
    static float GetTierHealthMultiplier(ContentTier tier);
    static float GetTierDamageMultiplier(ContentTier tier);
    static float GetTierArmorMultiplier(ContentTier tier);

    // Calculates complete combat budget for a creature context
    CalculatedCombatBudget CalculateBudget(CreatureTemplate const* cinfo,
                                          CreatureScaleContext const& context) const;

    // Builds scale context for a creature
    CreatureScaleContext BuildContext(CreatureTemplate const* cinfo, Creature const* creature,
                                     uint8 effectiveLevel) const;

private:
    CombatBudgetProfile() = default;
};

#define sCombatBudgetProfile CombatBudgetProfile::Instance()

#endif // COA_COMBAT_BUDGET_PROFILE_H
