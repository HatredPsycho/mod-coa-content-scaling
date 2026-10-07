/*
 * CoA Universal Content Scaling
 * ItemBudgetScaler: Normalization of item stats, ratings, armor, weapon DPS and required levels.
 */

#ifndef COA_ITEM_BUDGET_SCALER_H
#define COA_ITEM_BUDGET_SCALER_H

#include "ContentEra.h"
#include "ContentTier.h"
#include "Define.h"
#include "ProgressionLayout.h"
#include "SharedDefines.h"
#include "SpellAuraDefines.h"
#include <map>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

struct ItemTemplate;

enum class ItemModCategory : uint8
{
    PrimaryStat     = 0,
    SecondaryRating = 1,
    SpellPower      = 2,
    AttackPower     = 3,
    BlockValue      = 4,
    Resistance      = 5,
    Other           = 6
};

[[nodiscard]] constexpr ItemModCategory GetItemModCategory(uint32 statType)
{
    switch (statType)
    {
        case 3:  // ITEM_MOD_AGILITY
        case 4:  // ITEM_MOD_STRENGTH
        case 5:  // ITEM_MOD_INTELLECT
        case 6:  // ITEM_MOD_SPIRIT
        case 7:  // ITEM_MOD_STAMINA
            return ItemModCategory::PrimaryStat;

        case 12: // ITEM_MOD_DEFENSE_SKILL_RATING
        case 13: // ITEM_MOD_DODGE_RATING
        case 14: // ITEM_MOD_PARRY_RATING
        case 15: // ITEM_MOD_BLOCK_RATING
        case 16: // ITEM_MOD_HIT_MELEE_RATING
        case 17: // ITEM_MOD_HIT_RANGED_RATING
        case 18: // ITEM_MOD_HIT_SPELL_RATING
        case 19: // ITEM_MOD_CRIT_MELEE_RATING
        case 20: // ITEM_MOD_CRIT_RANGED_RATING
        case 21: // ITEM_MOD_CRIT_SPELL_RATING
        case 28: // ITEM_MOD_HASTE_MELEE_RATING
        case 29: // ITEM_MOD_HASTE_RANGED_RATING
        case 30: // ITEM_MOD_HASTE_SPELL_RATING
        case 31: // ITEM_MOD_HIT_RATING
        case 32: // ITEM_MOD_CRIT_RATING
        case 35: // ITEM_MOD_RESILIENCE_RATING
        case 36: // ITEM_MOD_HASTE_RATING
        case 37: // ITEM_MOD_EXPERTISE_RATING
        case 44: // ITEM_MOD_ARMOR_PENETRATION_RATING
            return ItemModCategory::SecondaryRating;

        case 38: // ITEM_MOD_ATTACK_POWER
        case 39: // ITEM_MOD_RANGED_ATTACK_POWER
        case 40: // ITEM_MOD_FERAL_ATTACK_POWER
            return ItemModCategory::AttackPower;

        case 45: // ITEM_MOD_SPELL_POWER
        case 41: // ITEM_MOD_SPELL_HEALING_DONE
        case 42: // ITEM_MOD_SPELL_DAMAGE_DONE
        case 43: // ITEM_MOD_MANA_REGENERATION
            return ItemModCategory::SpellPower;

        case 48: // ITEM_MOD_BLOCK_VALUE
            return ItemModCategory::BlockValue;

        case 22: // ITEM_MOD_HIT_TAKEN_MELEE_RATING
        case 23: // ITEM_MOD_HIT_TAKEN_RANGED_RATING
        case 24: // ITEM_MOD_HIT_TAKEN_SPELL_RATING
        case 25: // ITEM_MOD_CRIT_TAKEN_MELEE_RATING
        case 26: // ITEM_MOD_CRIT_TAKEN_RANGED_RATING
        case 27: // ITEM_MOD_CRIT_TAKEN_SPELL_RATING
            return ItemModCategory::SecondaryRating;

        default:
            return ItemModCategory::Other;
    }
}

enum GeneratedItemSpecialFlags : uint8
{
    ITEM_SPECIAL_NONE     = 0,
    ITEM_SPECIAL_PROC     = 1 << 0, // tr1/tr2 in (1, 2)
    ITEM_SPECIAL_USE      = 1 << 1, // tr1/tr2 == 0 with sp > 0
    ITEM_SPECIAL_SET      = 1 << 2, // ItemSet > 0
    ITEM_SPECIAL_SOCKET   = 1 << 3, // Sockets present
    ITEM_SPECIAL_CUSTOM   = 1 << 4, // Custom entry (>= 100000)
    ITEM_SPECIAL_PRESERVE = 1 << 5  // Cosmetic / Class unlocker (no combat stats)
};

enum class ItemScalingPolicy : uint8
{
    STANDARD       = 0, // Normal stat normalization
    TIER_ALIGNED   = 1, // Scaled strictly according to authoritative ContentTier
    PRESERVE       = 2, // Do not mutate combat fields or metadata
    REVIEW_SPECIAL = 3  // Base stats scale, special proc/use/socket preserved untouched
};

constexpr std::string_view ItemScalingPolicyToString(ItemScalingPolicy policy)
{
    switch (policy)
    {
        case ItemScalingPolicy::STANDARD:       return "STANDARD";
        case ItemScalingPolicy::TIER_ALIGNED:   return "TIER_ALIGNED";
        case ItemScalingPolicy::PRESERVE:       return "PRESERVE";
        case ItemScalingPolicy::REVIEW_SPECIAL: return "REVIEW_SPECIAL";
        default:                                return "UNKNOWN";
    }
}

struct ItemScalingContext
{
    ContentEra era{ContentEra::Classic};
    ContentTier tier{ContentTier::WORLD};
    ItemScalingPolicy policy{ItemScalingPolicy::STANDARD};
    uint32 sourceMap{0};
    uint8 specialFlags{0};
    bool hasGeneratedProfile{false};
    bool fallbackTierInference{false};

    static ItemScalingContext Resolve(ItemTemplate const* proto);
};

struct ScaledItemBudget
{
    uint32 effectiveRequiredLevel{1};
    uint32 effectiveItemLevel{1};
    float statMultiplier{1.0f};
    float ratingMultiplier{1.0f};
    float armorMultiplier{1.0f};
    float weaponDpsMultiplier{1.0f};
    float spellPowerMultiplier{1.0f};
};

// Which cut applies to a flat amount an item hands out through a spell. The applying code and the
// code that answers the client what a tooltip should say both read this, so the two can never come
// to different answers about the same effect.
enum class ScaledAmountFactor : uint8
{
    None   = 0,
    Stat   = 1,
    Rating = 2
};

[[nodiscard]] constexpr ScaledAmountFactor GetAuraAmountFactor(uint32 auraType)
{
    switch (auraType)
    {
        case SPELL_AURA_MOD_STAT:
        case SPELL_AURA_MOD_INCREASE_HEALTH:
        case SPELL_AURA_MOD_DAMAGE_DONE:
        case SPELL_AURA_MOD_HEALING_DONE:
        case SPELL_AURA_MOD_ATTACK_POWER:
        case SPELL_AURA_MOD_RANGED_ATTACK_POWER:
        case SPELL_AURA_SCHOOL_ABSORB:
            return ScaledAmountFactor::Stat;

        case SPELL_AURA_MOD_RATING:
            return ScaledAmountFactor::Rating;

        default:
            return ScaledAmountFactor::None;
    }
}

[[nodiscard]] constexpr ScaledAmountFactor GetSpellEffectAmountFactor(uint32 effectType)
{
    switch (effectType)
    {
        case SPELL_EFFECT_SCHOOL_DAMAGE:
        case SPELL_EFFECT_HEALTH_LEECH:
        case SPELL_EFFECT_HEAL:
        case SPELL_EFFECT_POWER_BURN:
            return ScaledAmountFactor::Stat;

        default:
            return ScaledAmountFactor::None;
    }
}

// What an item was actually cut by when the templates were rewritten. The rewrite happens in place,
// so afterwards the template no longer remembers what it was authored as and the cut cannot be
// derived from it a second time.
struct AppliedItemScaling
{
    uint32 authoredRequiredLevel{0};
    uint32 effectiveRequiredLevel{0};
    uint32 authoredItemLevel{0};
    uint32 effectiveItemLevel{0};
    float statMultiplier{1.0f};
    float ratingMultiplier{1.0f};
};

class ItemBudgetScaler
{
public:
    static ItemBudgetScaler* Instance();

    // Reconcile and calculate scaled budget for an item
    ScaledItemBudget CalculateItemBudget(ItemTemplate const* proto, ProgressionLayout const& layout) const;
    ScaledItemBudget CalculateItemBudget(ItemTemplate const* proto, ProgressionLayout const& layout,
                                         ItemScalingContext const& context) const;

    // The average item level an instance asks for, in the item levels this realm's gear now carries.
    [[nodiscard]] uint32 ScaleRequiredAverageItemLevel(uint32 authoredItemLevel, ContentEra era,
                                                       ProgressionLayout const& layout) const;

    // Apply scaled budget onto ItemTemplate in-memory (called post ObjectMgr load)
    void ScaleItemTemplate(ItemTemplate* proto, ProgressionLayout const& layout) const;

    // Process all loaded items in ObjectMgr (one-time idempotent execution)
    void ScaleAllItems(ProgressionLayout const& layout);

    // What an item was cut by, kept so that the spells it casts can be asked about too: their flat
    // values belong to the spell and are not touched when the template is rewritten.
    [[nodiscard]] float GetStatMultiplier(uint32 itemEntry) const;
    [[nodiscard]] float GetRatingMultiplier(uint32 itemEntry) const;

    [[nodiscard]] AppliedItemScaling const* FindAppliedScaling(uint32 itemEntry) const;
    // The same for any item, including a lifted copy of a cut item, which is cut from its own levels.
    [[nodiscard]] std::optional<AppliedItemScaling> ResolveAppliedScaling(uint32 itemEntry) const;
    // The cut of a copy at these levels of an item cut as in `base`: the same measure, from the copy's levels.
    [[nodiscard]] static AppliedItemScaling LiftedScaling(AppliedItemScaling const& base, uint32 itemLevel,
                                                          uint32 requiredLevel);

    [[nodiscard]] bool AreItemsScaled() const { return _itemsScaled; }
    void ResetScaledState() { _itemsScaled = false; }

    // Items that keep their authored template: loot of maps played at their authored levels.
    void PreserveItems(std::unordered_set<uint32> items) { _preservedItems = std::move(items); }
    [[nodiscard]] bool IsPreservedItem(uint32 itemEntry) const { return _preservedItems.count(itemEntry) != 0; }

    // What an item is worth at the level it was given: its authored value moved along the vendor prices
    // of items of its kind, from the item level it was written for to the one it has now. A custom item's
    // price was set by hand for this realm and stays.
    [[nodiscard]] uint32 ScaleMarketValue(uint32 itemEntry, uint32 value) const;

    // The item level an item was written with, given the one it has here. A lifted copy is measured from
    // the item it copies, so its lift carries over in proportion.
    [[nodiscard]] uint32 AuthoredItemLevel(uint32 itemEntry, uint32 itemLevel) const;

private:
    ItemBudgetScaler() = default;

    void CollectPriceSample(ItemTemplate const* proto);
    void FinishPriceCurves();
    [[nodiscard]] double PriceAt(uint64 key, uint32 itemLevel) const;

    bool _itemsScaled{false};
    std::unordered_map<uint32, AppliedItemScaling> _appliedScaling;
    std::unordered_set<uint32> _preservedItems;
    // Vendor prices by item level for each kind of item, from the authored templates.
    std::unordered_map<uint64, std::map<uint32, std::vector<double>>> _priceSamples;
    std::unordered_map<uint64, std::vector<double>> _priceCurves;
    std::unordered_set<uint32> _handPricedItems;
};

#define sItemBudgetScaler ItemBudgetScaler::Instance()

#endif // COA_ITEM_BUDGET_SCALER_H
