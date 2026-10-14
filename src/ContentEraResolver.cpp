/*
 * CoA Universal Content Scaling
 * ContentEraResolver: Implementation of hierarchical era resolution.
 */

#include "ContentEraResolver.h"
#include "DBCStores.h"
#include "InstanceProfile.h"
#include "Log.h"
#include <algorithm>
#include <array>

namespace
{
    constexpr uint32 OUTLAND_MAP = 530;

    // Zones on the Outland map written for levels 1 to 20: Eversong Woods, Ghostlands, Silvermoon
    // City, Azuremyst Isle, Bloodmyst Isle and The Exodar.
    constexpr std::array<uint32, 6> ClassicZonesOnOutland = { 3430, 3433, 3487, 3524, 3525, 3557 };

    ContentEra EraOfExpansion(uint8 expansion)
    {
        switch (expansion)
        {
            case 0:  return ContentEra::Classic;
            case 1:  return ContentEra::TBC;
            default: return ContentEra::WotLK;
        }
    }
}

ContentEraResolver* ContentEraResolver::Instance()
{
    static ContentEraResolver instance;
    return &instance;
}

void ContentEraResolver::LoadClientData()
{
    if (_finalized)
        return;

    for (MapEntry const* map : sMapStore)
        if (map)
            RegisterMapExpansion(map->MapID, static_cast<uint8>(map->Expansion()));

    for (AreaTableEntry const* area : sAreaTableStore)
    {
        if (!area)
            continue;

        RegisterAreaMap(area->ID, area->mapid);

        uint32 const zone = area->zone ? area->zone : area->ID;
        if (area->mapid == OUTLAND_MAP &&
            std::find(ClassicZonesOnOutland.begin(), ClassicZonesOnOutland.end(), zone) != ClassicZonesOnOutland.end())
            RegisterClassicArea(area->ID);
    }

    LOG_INFO("server.loading", "ContentEraResolver: {} map expansions, {} area maps and {} Classic areas on Outland",
             uint32(_mapExpansions.size()), uint32(_areaMaps.size()), uint32(_classicAreas.size()));
}

void ContentEraResolver::Finalize()
{
    _finalized = true;
}

void ContentEraResolver::RegisterMapExpansion(uint32 mapId, uint8 expansion)
{
    _mapExpansions[mapId] = EraOfExpansion(expansion);
}

void ContentEraResolver::RegisterAreaMap(uint32 areaId, uint32 mapId)
{
    _areaMaps[areaId] = mapId;
}

void ContentEraResolver::RegisterClassicArea(uint32 areaId)
{
    _classicAreas.insert(areaId);
}

void ContentEraResolver::RegisterMapOverride(uint32 mapId, ContentEra era)
{
    _mapOverrides[mapId] = era;
}

void ContentEraResolver::RegisterAreaOverride(uint32 areaId, ContentEra era)
{
    _areaOverrides[areaId] = era;
}

void ContentEraResolver::RegisterCreatureOverride(uint32 entry, ContentEra era)
{
    _creatureOverrides[entry] = era;
}

void ContentEraResolver::RegisterQuestOverride(uint32 questId, ContentEra era)
{
    _questOverrides[questId] = era;
}

void ContentEraResolver::RegisterItemOverride(uint32 itemId, ContentEra era)
{
    _itemOverrides[itemId] = era;
}

EraResolutionResult ContentEraResolver::ResolveEraDetailsForMap(uint32 mapId, uint8 authoredExpansion) const
{
    // 1. Explicit map override
    auto it = _mapOverrides.find(mapId);
    if (it != _mapOverrides.end())
        return EraResolutionResult{it->second, EraResolutionSource::ExplicitOverride, 1.0f};

    // 2. InstanceProfile registry, which also knows the maps whose era depends on the difficulty
    if (auto const era = sInstanceProfileRegistry->GetEraForMap(mapId))
        return EraResolutionResult{*era, EraResolutionSource::InstanceProfile, 1.0f};

    // 3. The expansion the client data names for the map
    auto const expansion = _mapExpansions.find(mapId);
    if (expansion != _mapExpansions.end())
        return EraResolutionResult{expansion->second, EraResolutionSource::MapExpansion, 1.0f};

    // Canonical open-world continents
    if (mapId == 0 || mapId == 1) // Eastern Kingdoms, Kalimdor
        return EraResolutionResult{ContentEra::Classic, EraResolutionSource::InstanceProfile, 1.0f};
    if (mapId == OUTLAND_MAP)
        return EraResolutionResult{ContentEra::TBC, EraResolutionSource::InstanceProfile, 1.0f};
    if (mapId == 571) // Northrend
        return EraResolutionResult{ContentEra::WotLK, EraResolutionSource::InstanceProfile, 1.0f};

    // 4. Authored expansion hint
    if (authoredExpansion == 2)
        return EraResolutionResult{ContentEra::WotLK, EraResolutionSource::AuthoredExpansion, 0.95f};
    if (authoredExpansion == 1)
        return EraResolutionResult{ContentEra::TBC, EraResolutionSource::AuthoredExpansion, 0.95f};

    // 5. Safe fallback
    return EraResolutionResult{ContentEra::Classic, EraResolutionSource::SafeFallback, 0.70f};
}

EraResolutionResult ContentEraResolver::ResolveEraDetailsForArea(uint32 areaId, uint32 mapId, uint8 authoredExpansion) const
{
    // 1. Explicit area override
    auto it = _areaOverrides.find(areaId);
    if (it != _areaOverrides.end())
        return EraResolutionResult{it->second, EraResolutionSource::ExplicitAreaOverride, 1.0f};

    auto const mapOverride = _mapOverrides.find(mapId);
    if (mapOverride != _mapOverrides.end())
        return EraResolutionResult{mapOverride->second, EraResolutionSource::ExplicitOverride, 1.0f};

    if ((mapId == OUTLAND_MAP || mapId == MAP_UNSPECIFIED) && IsClassicArea(areaId))
        return EraResolutionResult{ContentEra::Classic, EraResolutionSource::ZoneSortMetadata, 1.0f};

    // 2. Defer to map resolution, through the area's own map when none is given
    if (mapId == MAP_UNSPECIFIED)
    {
        auto const areaMap = _areaMaps.find(areaId);
        if (areaMap != _areaMaps.end())
            mapId = areaMap->second;
    }

    if (mapId != MAP_UNSPECIFIED)
        return ResolveEraDetailsForMap(mapId, authoredExpansion);

    // 3. Authored expansion metadata
    if (authoredExpansion == 2)
        return EraResolutionResult{ContentEra::WotLK, EraResolutionSource::AuthoredExpansion, 0.95f};
    if (authoredExpansion == 1)
        return EraResolutionResult{ContentEra::TBC, EraResolutionSource::AuthoredExpansion, 0.95f};

    return EraResolutionResult{ContentEra::Classic, EraResolutionSource::SafeFallback, 0.70f};
}

EraResolutionResult ContentEraResolver::ResolveEraDetailsForCreature(uint32 creatureEntry, uint32 mapId, uint32 areaId,
                                                                     uint8 authoredExpansion, uint8 authoredLevel) const
{
    // Priority 1: Explicit creature entry override
    auto it = _creatureOverrides.find(creatureEntry);
    if (it != _creatureOverrides.end())
        return EraResolutionResult{it->second, EraResolutionSource::ExplicitOverride, 1.0f};

    // Priority 2: Explicit area override
    if (areaId > 0)
    {
        auto ait = _areaOverrides.find(areaId);
        if (ait != _areaOverrides.end())
            return EraResolutionResult{ait->second, EraResolutionSource::ExplicitAreaOverride, 1.0f};
    }

    if (mapId != MAP_UNSPECIFIED)
    {
        auto const mapOverride = _mapOverrides.find(mapId);
        if (mapOverride != _mapOverrides.end())
            return EraResolutionResult{mapOverride->second, EraResolutionSource::ExplicitOverride, 1.0f};
    }

    if (mapId == OUTLAND_MAP)
    {
        if (IsClassicArea(areaId))
            return EraResolutionResult{ContentEra::Classic, EraResolutionSource::ZoneSortMetadata, 1.0f};
        if (areaId > 0)
            return ResolveEraDetailsForArea(areaId, mapId, authoredExpansion);
    }

    // Priority 3: Instance profile or map expansion
    // If creature is placed on a map with clear era ownership, map ownership takes precedence over level
    if (mapId != MAP_UNSPECIFIED)
    {
        EraResolutionResult const mapRes = ResolveEraDetailsForMap(mapId, authoredExpansion);
        if (mapRes.source == EraResolutionSource::ExplicitOverride ||
            mapRes.source == EraResolutionSource::MapExpansion ||
            mapRes.source == EraResolutionSource::InstanceProfile)
        {
            return EraResolutionResult{mapRes.era, EraResolutionSource::InstanceProfile, 1.0f};
        }
    }

    // Priority 4: Authored expansion metadata (creature_template.exp)
    if (authoredExpansion == 2)
        return EraResolutionResult{ContentEra::WotLK, EraResolutionSource::AuthoredExpansion, 0.95f};
    if (authoredExpansion == 1)
        return EraResolutionResult{ContentEra::TBC, EraResolutionSource::AuthoredExpansion, 0.95f};

    // Priority 5: Zone/sort metadata
    if (areaId > 0)
    {
        EraResolutionResult const areaRes = ResolveEraDetailsForArea(areaId, mapId, authoredExpansion);
        if (areaRes.source != EraResolutionSource::SafeFallback)
            return EraResolutionResult{areaRes.era, EraResolutionSource::ZoneSortMetadata, 0.85f};
    }

    // Priority 6: Authored level heuristic — STRICTLY LAST RESORT
    // Notice: Classic level 60 creatures on open world do NOT get promoted to TBC.
    if (authoredLevel >= 71)
        return EraResolutionResult{ContentEra::WotLK, EraResolutionSource::AuthoredLevelHeuristic, 0.60f};
    if (authoredLevel >= 61)
        return EraResolutionResult{ContentEra::TBC, EraResolutionSource::AuthoredLevelHeuristic, 0.60f};

    // Priority 7: Safe fallback
    return EraResolutionResult{ContentEra::Classic, EraResolutionSource::SafeFallback, 0.70f};
}

EraResolutionResult ContentEraResolver::ResolveEraDetailsForQuest(uint32 questId, int32 zoneOrSort,
                                                                  uint8 authoredExpansion, uint8 authoredLevel) const
{
    // Priority 1: Explicit quest override
    auto it = _questOverrides.find(questId);
    if (it != _questOverrides.end())
        return EraResolutionResult{it->second, EraResolutionSource::ExplicitOverride, 1.0f};

    // Priority 2: Explicit area override
    if (zoneOrSort > 0)
    {
        auto const areaOverride = _areaOverrides.find(uint32(zoneOrSort));
        if (areaOverride != _areaOverrides.end())
            return EraResolutionResult{areaOverride->second, EraResolutionSource::ExplicitAreaOverride, 1.0f};

        if (IsClassicArea(uint32(zoneOrSort)))
            return EraResolutionResult{ContentEra::Classic, EraResolutionSource::ZoneSortMetadata, 1.0f};
    }

    // Priority 3: Authored expansion metadata
    if (authoredExpansion == 2)
        return EraResolutionResult{ContentEra::WotLK, EraResolutionSource::AuthoredExpansion, 0.95f};
    if (authoredExpansion == 1)
        return EraResolutionResult{ContentEra::TBC, EraResolutionSource::AuthoredExpansion, 0.95f};

    // Priority 4: The map of the quest's zone
    if (zoneOrSort > 0)
    {
        auto const areaMap = _areaMaps.find(uint32(zoneOrSort));
        if (areaMap != _areaMaps.end())
        {
            EraResolutionResult const mapRes = ResolveEraDetailsForMap(areaMap->second);
            if (mapRes.source != EraResolutionSource::SafeFallback)
                return EraResolutionResult{mapRes.era, EraResolutionSource::ZoneSortMetadata, 0.95f};
        }
    }

    // Priority 5: Zone/sort metadata
    // Canonical Outland zones
    if (zoneOrSort == 3483 || zoneOrSort == 3518 || zoneOrSort == 3519 || zoneOrSort == 3520 ||
        zoneOrSort == 3521 || zoneOrSort == 3522 || zoneOrSort == 3523)
    {
        return EraResolutionResult{ContentEra::TBC, EraResolutionSource::ZoneSortMetadata, 0.90f};
    }

    // Canonical Northrend zones
    if (zoneOrSort == 3537 || zoneOrSort == 65 || zoneOrSort == 394 || zoneOrSort == 495 ||
        zoneOrSort == 2817 || zoneOrSort == 3711 || zoneOrSort == 66 || zoneOrSort == 67 ||
        zoneOrSort == 210 || zoneOrSort == 4395 || zoneOrSort == 4742)
    {
        return EraResolutionResult{ContentEra::WotLK, EraResolutionSource::ZoneSortMetadata, 0.90f};
    }

    // Priority 6: Authored level heuristic — STRICTLY LAST RESORT
    if (authoredLevel >= 71)
        return EraResolutionResult{ContentEra::WotLK, EraResolutionSource::AuthoredLevelHeuristic, 0.60f};
    if (authoredLevel >= 61)
        return EraResolutionResult{ContentEra::TBC, EraResolutionSource::AuthoredLevelHeuristic, 0.60f};

    // Priority 7: Safe fallback
    return EraResolutionResult{ContentEra::Classic, EraResolutionSource::SafeFallback, 0.70f};
}

EraResolutionResult ContentEraResolver::ResolveEraDetailsForItem(uint32 itemId, uint32 itemLevel,
                                                                 uint32 requiredLevel, uint8 authoredExpansion) const
{
    // Priority 1: Explicit item override
    auto it = _itemOverrides.find(itemId);
    if (it != _itemOverrides.end())
        return EraResolutionResult{it->second, EraResolutionSource::ExplicitOverride, 1.0f};

    // Priority 2: Authored expansion metadata (ItemTemplate.RequiredExpansion)
    if (authoredExpansion == 2)
        return EraResolutionResult{ContentEra::WotLK, EraResolutionSource::AuthoredExpansion, 1.0f};
    if (authoredExpansion == 1)
        return EraResolutionResult{ContentEra::TBC, EraResolutionSource::AuthoredExpansion, 1.0f};
    if (authoredExpansion == 0 && requiredLevel <= 60 && itemLevel <= 92)
    {
        // Explicitly Classic (even high-end Naxxramas items with ilvl up to 92)
        return EraResolutionResult{ContentEra::Classic, EraResolutionSource::AuthoredExpansion, 1.0f};
    }

    // Priority 3: Level and ilvl heuristics — LAST RESORT. The required level names the expansion
    // first: Sunwell's level 70 items reach item level 164, above where Northrend's start.
    if (requiredLevel >= 71)
        return EraResolutionResult{ContentEra::WotLK, EraResolutionSource::AuthoredLevelHeuristic, 0.65f};
    if (requiredLevel >= 61)
        return EraResolutionResult{ContentEra::TBC, EraResolutionSource::AuthoredLevelHeuristic, 0.65f};
    if (itemLevel >= 150)
        return EraResolutionResult{ContentEra::WotLK, EraResolutionSource::AuthoredLevelHeuristic, 0.65f};
    if (itemLevel > 92)
        return EraResolutionResult{ContentEra::TBC, EraResolutionSource::AuthoredLevelHeuristic, 0.65f};

    // Priority 4: Safe fallback
    return EraResolutionResult{ContentEra::Classic, EraResolutionSource::SafeFallback, 0.70f};
}

ContentEra ContentEraResolver::ResolveEraForMap(uint32 mapId, uint8 authoredExpansion) const
{
    return ResolveEraDetailsForMap(mapId, authoredExpansion).era;
}

ContentEra ContentEraResolver::ResolveEraForArea(uint32 areaId, uint32 mapId, uint8 authoredExpansion) const
{
    return ResolveEraDetailsForArea(areaId, mapId, authoredExpansion).era;
}

ContentEra ContentEraResolver::ResolveEraForCreature(uint32 creatureEntry, uint32 mapId, uint32 areaId,
                                                     uint8 authoredExpansion, uint8 authoredLevel) const
{
    return ResolveEraDetailsForCreature(creatureEntry, mapId, areaId, authoredExpansion, authoredLevel).era;
}

ContentEra ContentEraResolver::ResolveEraForQuest(uint32 questId, int32 zoneOrSort,
                                                  uint8 authoredExpansion, uint8 authoredLevel) const
{
    return ResolveEraDetailsForQuest(questId, zoneOrSort, authoredExpansion, authoredLevel).era;
}

ContentEra ContentEraResolver::ResolveEraForItem(uint32 itemId, uint32 itemLevel,
                                                 uint32 requiredLevel, uint8 authoredExpansion) const
{
    return ResolveEraDetailsForItem(itemId, itemLevel, requiredLevel, authoredExpansion).era;
}

void ContentEraResolver::Clear()
{
    _finalized = false;
    _mapExpansions.clear();
    _areaMaps.clear();
    _classicAreas.clear();
    _mapOverrides.clear();
    _areaOverrides.clear();
    _creatureOverrides.clear();
    _questOverrides.clear();
    _itemOverrides.clear();
}
