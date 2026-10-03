/*
 * CoA Universal Content Scaling
 * ProgressionLayout: Implementation of progression layout interpolation and mapping.
 */

#include "ProgressionLayout.h"
#include <sstream>

ProgressionLayout ProgressionLayout::Create(uint8 maxLevel, bool tbcEnabled, bool wotlkEnabled,
                                            uint8 customClassicEnd, uint8 customTbcEnd)
{
    ProgressionLayout layout;
    layout.maxLevel = maxLevel;
    layout.tbcEnabled = tbcEnabled;
    layout.wotlkEnabled = wotlkEnabled;

    // Determine reference layout anchors via linear interpolation between 60 and 80 profiles:
    // Cap 60 reference: Classic ends at 45, TBC ends at 55, WotLK ends at 60
    // Cap 80 reference: Classic ends at 60, TBC ends at 70, WotLK ends at 80
    uint8 autoClassicEnd = 45;
    uint8 autoTbcEnd = 55;

    if (maxLevel <= 60)
    {
        autoClassicEnd = 45;
        autoTbcEnd = 55;
    }
    else if (maxLevel >= 80)
    {
        autoClassicEnd = 60;
        autoTbcEnd = 70;
    }
    else
    {
        // Interpolate monotonic anchors between 60 and 80
        float const alpha = float(maxLevel - 60) / 20.0f;
        autoClassicEnd = static_cast<uint8>(std::round(45.0f + alpha * 15.0f));
        autoTbcEnd     = static_cast<uint8>(std::round(55.0f + alpha * 15.0f));
    }

    // Apply custom overrides if specified (> 0)
    uint8 const classicEnd = (customClassicEnd > 0) ? customClassicEnd : autoClassicEnd;
    uint8 const tbcEnd     = (customTbcEnd > 0) ? customTbcEnd : autoTbcEnd;

    // 1. Classic Only
    if (!tbcEnabled && !wotlkEnabled)
    {
        layout.classic = LevelRange(1, maxLevel);
        layout.tbc = std::nullopt;
        layout.wotlk = std::nullopt;
    }
    // 2. Classic + TBC
    else if (tbcEnabled && !wotlkEnabled)
    {
        uint8 const effectiveClassicEnd = std::min(classicEnd, maxLevel);
        layout.classic = LevelRange(1, effectiveClassicEnd);
        layout.tbc = LevelRange(effectiveClassicEnd, maxLevel);
        layout.wotlk = std::nullopt;
    }
    // 3. Classic + WotLK (without TBC)
    else if (!tbcEnabled && wotlkEnabled)
    {
        uint8 const effectiveClassicEnd = std::min(classicEnd, maxLevel);
        layout.classic = LevelRange(1, effectiveClassicEnd);
        layout.tbc = std::nullopt;
        layout.wotlk = LevelRange(effectiveClassicEnd, maxLevel);
    }
    // 4. Classic + TBC + WotLK
    else
    {
        uint8 const effectiveClassicEnd = std::min(classicEnd, maxLevel);
        uint8 const effectiveTbcEnd     = std::clamp<uint8>(tbcEnd, effectiveClassicEnd, maxLevel);

        layout.classic = LevelRange(1, effectiveClassicEnd);
        layout.tbc     = LevelRange(effectiveClassicEnd, effectiveTbcEnd);
        layout.wotlk   = LevelRange(effectiveTbcEnd, maxLevel);
    }

    return layout;
}

bool ProgressionLayout::Validate(std::string& outError) const
{
    if (maxLevel < 60 || maxLevel > 80)
    {
        outError = "MaxPlayerLevel " + std::to_string(maxLevel) + " is outside supported range [60, 80]";
        return false;
    }

    if (!classic.IsValid() || classic.minLevel != 1)
    {
        outError = "Classic range is invalid or does not start at 1";
        return false;
    }

    if (tbc.has_value())
    {
        if (!tbc->IsValid())
        {
            outError = "TBC range is invalid";
            return false;
        }
        if (tbc->minLevel != classic.maxLevel)
        {
            outError = "TBC does not seamlessly continue Classic boundary";
            return false;
        }
    }

    if (wotlk.has_value())
    {
        if (!wotlk->IsValid())
        {
            outError = "WotLK range is invalid";
            return false;
        }
        uint8 const expectedWotlkStart = tbc.has_value() ? tbc->maxLevel : classic.maxLevel;
        if (wotlk->minLevel != expectedWotlkStart)
        {
            outError = "WotLK does not seamlessly continue preceding era boundary";
            return false;
        }
        if (wotlk->maxLevel != maxLevel)
        {
            outError = "WotLK does not terminate at MaxPlayerLevel";
            return false;
        }
    }
    else if (tbc.has_value())
    {
        if (tbc->maxLevel != maxLevel)
        {
            outError = "Last active era (TBC) does not terminate at MaxPlayerLevel";
            return false;
        }
    }
    else
    {
        if (classic.maxLevel != maxLevel)
        {
            outError = "Classic-only does not terminate at MaxPlayerLevel";
            return false;
        }
    }

    return true;
}

LevelRange ProgressionLayout::GetEraRange(ContentEra era) const
{
    switch (era)
    {
        case ContentEra::Classic:
            return classic;
        case ContentEra::TBC:
            return tbc.value_or(classic);
        case ContentEra::WotLK:
            return wotlk.value_or(tbc.value_or(classic));
        case ContentEra::Custom:
        default:
            return LevelRange(1, maxLevel);
    }
}

bool ProgressionLayout::IsEraEnabled(ContentEra era) const
{
    switch (era)
    {
        case ContentEra::Classic:
            return true;
        case ContentEra::TBC:
            return tbcEnabled && tbc.has_value();
        case ContentEra::WotLK:
            return wotlkEnabled && wotlk.has_value();
        case ContentEra::Custom:
            return true;
        default:
            return false;
    }
}

// Every boundary, not only the cap: a realm that keeps cap 80 but moves a band is not the stock
// layout, and reading it as one leaves its content unscaled.
//
// The boundaries are the ones Create lays out for a cap of 80, where the bands meet rather than
// overlap. They are not the canonical authored spans, which do overlap at 58 and 68: those say
// where an expansion's own content begins, this says where a character of that level stands.
bool ProgressionLayout::IsStockIdentity() const
{
    return maxLevel == 80 && tbcEnabled && wotlkEnabled &&
        classic.minLevel == 1 && classic.maxLevel == 60 &&
        tbc.has_value() && tbc->minLevel == 60 && tbc->maxLevel == 70 &&
        wotlk.has_value() && wotlk->minLevel == 70 && wotlk->maxLevel == 80;
}

LevelRange ProgressionLayout::CanonicalAuthoredRange(ContentEra era)
{
    switch (era)
    {
        case ContentEra::Classic:
            return LevelRange{1, 60};
        case ContentEra::TBC:
            return LevelRange{58, 70};
        case ContentEra::WotLK:
            return LevelRange{68, 80};
        default:
            return LevelRange{0, 0};
    }
}

// The era an expansion shipped a piece of content with says where its progression put it, not when
// it was written: the Blood Elf and Draenei starting zones came with Burning Crusade and are meant
// for levels 1 to 20. Mapping those through the Burning Crusade band lands them on its floor, which
// is how a level 6 character came to meet a level 45 Wretched Urchin.
//
// Only content levels are read this way. An entry requirement is a gate on the instance it guards
// and keeps the era of that instance, however far below its own progression the gate sits.
ContentEra ProgressionLayout::ResolveContentEra(ContentEra taggedEra, uint8 authoredLevel) const
{
    LevelRange const canonical = CanonicalAuthoredRange(taggedEra);
    if (!canonical.minLevel || authoredLevel >= canonical.minLevel)
        return taggedEra;

    return GetEraForAuthoredLevel(authoredLevel);
}

uint8 ProgressionLayout::MapAuthoredToEffective(ContentEra era, uint8 authoredLevel,
                                               uint8 sourceMin, uint8 sourceMax) const
{
    LevelRange const targetRange = GetEraRange(era);
    if (!targetRange.IsValid())
        return std::min(authoredLevel, maxLevel);

    // If sourceMin and sourceMax are unspecified, use canonical authored boundaries:
    if (sourceMin == 0 && sourceMax == 0)
    {
        LevelRange const canonical = CanonicalAuthoredRange(era);
        sourceMin = canonical.minLevel ? canonical.minLevel : targetRange.minLevel;
        sourceMax = canonical.maxLevel ? canonical.maxLevel : targetRange.maxLevel;
    }

    if (sourceMax <= sourceMin)
        return targetRange.minLevel;

    if (authoredLevel <= sourceMin)
        return targetRange.minLevel;

    if (authoredLevel >= sourceMax)
        return targetRange.maxLevel;

    float const progress = float(authoredLevel - sourceMin) / float(sourceMax - sourceMin);
    uint8 const mapped = targetRange.minLevel + static_cast<uint8>(std::round(progress * float(targetRange.maxLevel - targetRange.minLevel)));
    return std::clamp<uint8>(mapped, targetRange.minLevel, targetRange.maxLevel);
}

ContentEra ProgressionLayout::GetEraForAuthoredLevel(uint8 authoredLevel) const
{
    if (authoredLevel >= 68 && wotlkEnabled)
        return ContentEra::WotLK;
    if (authoredLevel >= 58 && tbcEnabled)
        return ContentEra::TBC;
    return ContentEra::Classic;
}

std::string ProgressionLayout::ToString() const
{
    std::ostringstream ss;
    ss << "MaxPlayerLevel: " << uint32(maxLevel) << "\n";
    ss << "Active Packs: Classic";
    if (tbcEnabled)
        ss << ", TBC";
    if (wotlkEnabled)
        ss << ", WotLK";
    ss << "\n";

    ss << "Classic: " << uint32(classic.minLevel) << " -> " << uint32(classic.maxLevel) << "\n";
    if (tbc.has_value())
        ss << "TBC:     " << uint32(tbc->minLevel) << " -> " << uint32(tbc->maxLevel) << "\n";
    if (wotlk.has_value())
        ss << "WotLK:   " << uint32(wotlk->minLevel) << " -> " << uint32(wotlk->maxLevel) << "\n";

    return ss.str();
}
