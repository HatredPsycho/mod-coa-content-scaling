/*
 * CoA Universal Content Scaling
 * ContentPackRegistry: Implementation of registry and hierarchical era resolution.
 */

#include "ContentPackRegistry.h"
#include "GeneratedContentCensus.h"
#include "InstanceProfile.h"
#include "Log.h"
#include <algorithm>

ContentPackRegistry* ContentPackRegistry::Instance()
{
    static ContentPackRegistry instance;
    return &instance;
}

void ContentPackRegistry::Finalize()
{
    _finalized = true;
    LOG_INFO("server.loading", "ContentPackRegistry: Finalized with {} active content packs",
             uint32(_packs.size()));
}

void ContentPackRegistry::RegisterPack(std::shared_ptr<IContentPack> pack)
{
    if (!pack)
        return;

    if (_finalized)
    {
        LOG_ERROR("module.coa_content_scaling",
                  "ContentPackRegistry::RegisterPack: Attempted to register pack '{}' after registry was finalized! Restart required.",
                  pack->GetName());
        return;
    }

    UnregisterPack(pack->GetEra());
    _packs.push_back(pack);
}

void ContentPackRegistry::UnregisterPack(ContentEra era)
{
    if (_finalized)
    {
        LOG_ERROR("module.coa_content_scaling",
                  "ContentPackRegistry::UnregisterPack: Attempted to unregister era '{}' after registry was finalized! Restart required.",
                  ContentEraToString(era));
        return;
    }

    _packs.erase(
        std::remove_if(_packs.begin(), _packs.end(),
            [era](std::shared_ptr<IContentPack> const& p) { return p->GetEra() == era; }),
        _packs.end());
}

IContentPack const* ContentPackRegistry::GetPack(ContentEra era) const
{
    for (auto const& pack : _packs)
        if (pack->GetEra() == era)
            return pack.get();
    return nullptr;
}

bool ContentPackRegistry::HasPack(ContentEra era) const
{
    return GetPack(era) != nullptr;
}

void ContentPackRegistry::RegisterMapOverride(uint32 mapId, ContentEra era)
{
    _mapOverrides[mapId] = era;
}

void ContentPackRegistry::RegisterAreaOverride(uint32 areaId, ContentEra era)
{
    _areaOverrides[areaId] = era;
}

void ContentPackRegistry::RegisterCreatureOverride(uint32 entry, ContentEra era)
{
    _creatureOverrides[entry] = era;
}

void ContentPackRegistry::RegisterQuestOverride(uint32 questId, ContentEra era)
{
    _questOverrides[questId] = era;
}

void ContentPackRegistry::RegisterItemOverride(uint32 itemId, ContentEra era)
{
    _itemOverrides[itemId] = era;
}

EraResolutionResult ContentPackRegistry::ResolveEraDetailsForMap(uint32 mapId, uint8 authoredExpansion) const
{
    // 1. Explicit map override
    auto it = _mapOverrides.find(mapId);
    if (it != _mapOverrides.end())
        return EraResolutionResult{it->second, EraResolutionSource::ExplicitOverride, 1.0f};

    // 2. Query registered content packs
    for (auto const& pack : _packs)
    {
        if (pack->HandlesMap(mapId))
            return EraResolutionResult{pack->GetEra(), EraResolutionSource::ContentPack, 1.0f};
    }

    // 3. InstanceProfile registry
    if (auto const era = sInstanceProfileRegistry->GetEraForMap(mapId))
        return EraResolutionResult{*era, EraResolutionSource::InstanceProfile, 1.0f};

    // Canonical open-world continents
    if (mapId == 0 || mapId == 1) // Eastern Kingdoms, Kalimdor
        return EraResolutionResult{ContentEra::Classic, EraResolutionSource::InstanceProfile, 1.0f};
    if (mapId == 530) // Outland
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

EraResolutionResult ContentPackRegistry::ResolveEraDetailsForArea(uint32 areaId, uint32 mapId, uint8 authoredExpansion) const
{
    // 1. Explicit area override
    auto it = _areaOverrides.find(areaId);
    if (it != _areaOverrides.end())
        return EraResolutionResult{it->second, EraResolutionSource::ExplicitAreaOverride, 1.0f};

    auto const mapOverride = _mapOverrides.find(mapId);
    if (mapOverride != _mapOverrides.end())
        return EraResolutionResult{mapOverride->second, EraResolutionSource::ExplicitOverride, 1.0f};

    if ((mapId == 530 || mapId == MAP_UNSPECIFIED) && IsGeneratedStartingArea(areaId))
        return EraResolutionResult{ContentEra::Classic, EraResolutionSource::ZoneSortMetadata, 1.0f};

    // 2. Query registered content packs
    for (auto const& pack : _packs)
    {
        if (pack->HandlesArea(areaId))
            return EraResolutionResult{pack->GetEra(), EraResolutionSource::ContentPack, 1.0f};
    }

    // 3. Defer to map resolution if mapId is provided
    if (mapId != MAP_UNSPECIFIED)
        return ResolveEraDetailsForMap(mapId, authoredExpansion);

    // 4. Authored expansion metadata
    if (authoredExpansion == 2)
        return EraResolutionResult{ContentEra::WotLK, EraResolutionSource::AuthoredExpansion, 0.95f};
    if (authoredExpansion == 1)
        return EraResolutionResult{ContentEra::TBC, EraResolutionSource::AuthoredExpansion, 0.95f};

    return EraResolutionResult{ContentEra::Classic, EraResolutionSource::SafeFallback, 0.70f};
}

EraResolutionResult ContentPackRegistry::ResolveEraDetailsForCreature(uint32 creatureEntry, uint32 mapId, uint32 areaId,
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

    if (mapId == 530)
    {
        auto const mapOverride = _mapOverrides.find(mapId);
        if (mapOverride != _mapOverrides.end())
            return EraResolutionResult{mapOverride->second, EraResolutionSource::ExplicitOverride, 1.0f};
        if (IsGeneratedStartingArea(areaId))
            return EraResolutionResult{ContentEra::Classic, EraResolutionSource::ZoneSortMetadata, 1.0f};
        if (areaId > 0)
            return ResolveEraDetailsForArea(areaId, mapId, authoredExpansion);
    }

    // Priority 2.5: Placement-Aware Generated Creature Census Profile
    if (mapId != MAP_UNSPECIFIED)
    {
        if (auto const* gc = FindGeneratedCreaturePlacement(creatureEntry, mapId))
            return EraResolutionResult{gc->era, EraResolutionSource::ContentCensus, static_cast<float>(gc->confidence) / 100.0f};
    }

    // Priority 3: Explicit instance/map profile
    // If creature is placed on a map with clear era ownership, map ownership takes precedence over level
    if (mapId != MAP_UNSPECIFIED)
    {
        EraResolutionResult const mapRes = ResolveEraDetailsForMap(mapId, authoredExpansion);
        if (mapRes.source == EraResolutionSource::ExplicitOverride ||
            mapRes.source == EraResolutionSource::ContentPack ||
            mapRes.source == EraResolutionSource::InstanceProfile)
        {
            return EraResolutionResult{mapRes.era, EraResolutionSource::InstanceProfile, 1.0f};
        }
    }

    // Priority 4: Registered content pack ownership
    for (auto const& pack : _packs)
    {
        if (pack->HandlesCreature(creatureEntry))
            return EraResolutionResult{pack->GetEra(), EraResolutionSource::ContentPack, 1.0f};
    }

    // Priority 5: Authored expansion metadata (creature_template.exp)
    if (authoredExpansion == 2)
        return EraResolutionResult{ContentEra::WotLK, EraResolutionSource::AuthoredExpansion, 0.95f};
    if (authoredExpansion == 1)
        return EraResolutionResult{ContentEra::TBC, EraResolutionSource::AuthoredExpansion, 0.95f};

    // Priority 6: Zone/sort metadata
    if (areaId > 0)
    {
        EraResolutionResult const areaRes = ResolveEraDetailsForArea(areaId, mapId, authoredExpansion);
        if (areaRes.source != EraResolutionSource::SafeFallback)
            return EraResolutionResult{areaRes.era, EraResolutionSource::ZoneSortMetadata, 0.85f};
    }

    // Priority 7: Authored level heuristic — STRICTLY LAST RESORT
    // Notice: Classic level 60 creatures on open world do NOT get promoted to TBC.
    if (authoredLevel >= 71)
    {
        return EraResolutionResult{ContentEra::WotLK, EraResolutionSource::AuthoredLevelHeuristic, 0.60f};
    }
    if (authoredLevel >= 61)
    {
        return EraResolutionResult{ContentEra::TBC, EraResolutionSource::AuthoredLevelHeuristic, 0.60f};
    }

    // Priority 8: Safe fallback
    return EraResolutionResult{ContentEra::Classic, EraResolutionSource::SafeFallback, 0.70f};
}

EraResolutionResult ContentPackRegistry::ResolveEraDetailsForQuest(uint32 questId, int32 zoneOrSort,
                                                                   uint8 authoredExpansion, uint8 authoredLevel) const
{
    // Priority 1: Explicit quest override
    auto it = _questOverrides.find(questId);
    if (it != _questOverrides.end())
        return EraResolutionResult{it->second, EraResolutionSource::ExplicitOverride, 1.0f};

    // Priority 2: Registered content pack ownership
    for (auto const& pack : _packs)
    {
        if (pack->HandlesQuest(questId))
            return EraResolutionResult{pack->GetEra(), EraResolutionSource::ContentPack, 1.0f};
    }

    if (zoneOrSort > 0)
    {
        auto const areaOverride = _areaOverrides.find(uint32(zoneOrSort));
        if (areaOverride != _areaOverrides.end())
            return EraResolutionResult{areaOverride->second, EraResolutionSource::ExplicitAreaOverride, 1.0f};
        if (IsGeneratedStartingArea(uint32(zoneOrSort)))
            return EraResolutionResult{ContentEra::Classic, EraResolutionSource::ZoneSortMetadata, 1.0f};
    }

    // Priority 2.5: Generated Quest Census Profile
    if (auto const* gq = FindGeneratedQuestProfile(questId))
    {
        return EraResolutionResult{gq->era, EraResolutionSource::ContentCensus, static_cast<float>(gq->confidence) / 100.0f};
    }

    // Priority 4: Authored expansion metadata
    if (authoredExpansion == 2)
        return EraResolutionResult{ContentEra::WotLK, EraResolutionSource::AuthoredExpansion, 0.95f};
    if (authoredExpansion == 1)
        return EraResolutionResult{ContentEra::TBC, EraResolutionSource::AuthoredExpansion, 0.95f};

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

EraResolutionResult ContentPackRegistry::ResolveEraDetailsForItem(uint32 itemId, uint32 itemLevel,
                                                                 uint32 requiredLevel, uint8 authoredExpansion) const
{
    // Priority 1: Explicit item override
    auto it = _itemOverrides.find(itemId);
    if (it != _itemOverrides.end())
        return EraResolutionResult{it->second, EraResolutionSource::ExplicitOverride, 1.0f};

    // Priority 2: Registered content pack ownership
    for (auto const& pack : _packs)
    {
        if (pack->HandlesItem(itemId))
            return EraResolutionResult{pack->GetEra(), EraResolutionSource::ContentPack, 1.0f};
    }

    // Priority 2.5: Generated Item Source Profile
    if (auto const* gi = FindGeneratedItemProfile(itemId))
    {
        return EraResolutionResult{gi->era, EraResolutionSource::InstanceProfile, 0.95f};
    }

    // Priority 3: Authored expansion metadata (ItemTemplate.RequiredExpansion)
    if (authoredExpansion == 2)
        return EraResolutionResult{ContentEra::WotLK, EraResolutionSource::AuthoredExpansion, 1.0f};
    if (authoredExpansion == 1)
        return EraResolutionResult{ContentEra::TBC, EraResolutionSource::AuthoredExpansion, 1.0f};
    if (authoredExpansion == 0 && requiredLevel <= 60 && itemLevel <= 92)
    {
        // Explicitly Classic (even high-end Naxxramas items with ilvl up to 92)
        return EraResolutionResult{ContentEra::Classic, EraResolutionSource::AuthoredExpansion, 1.0f};
    }

    // Priority 4: Level and ilvl heuristics — LAST RESORT
    if (requiredLevel >= 71 || itemLevel >= 150)
        return EraResolutionResult{ContentEra::WotLK, EraResolutionSource::AuthoredLevelHeuristic, 0.65f};
    if (requiredLevel >= 61 || (itemLevel > 92 && itemLevel < 150))
        return EraResolutionResult{ContentEra::TBC, EraResolutionSource::AuthoredLevelHeuristic, 0.65f};

    // Priority 5: Safe fallback
    return EraResolutionResult{ContentEra::Classic, EraResolutionSource::SafeFallback, 0.70f};
}

ContentEra ContentPackRegistry::ResolveEraForMap(uint32 mapId, uint8 authoredExpansion) const
{
    return ResolveEraDetailsForMap(mapId, authoredExpansion).era;
}

ContentEra ContentPackRegistry::ResolveEraForArea(uint32 areaId, uint32 mapId, uint8 authoredExpansion) const
{
    return ResolveEraDetailsForArea(areaId, mapId, authoredExpansion).era;
}

ContentEra ContentPackRegistry::ResolveEraForCreature(uint32 creatureEntry, uint32 mapId, uint32 areaId,
                                                     uint8 authoredExpansion, uint8 authoredLevel) const
{
    return ResolveEraDetailsForCreature(creatureEntry, mapId, areaId, authoredExpansion, authoredLevel).era;
}

ContentEra ContentPackRegistry::ResolveEraForQuest(uint32 questId, int32 zoneOrSort,
                                                  uint8 authoredExpansion, uint8 authoredLevel) const
{
    return ResolveEraDetailsForQuest(questId, zoneOrSort, authoredExpansion, authoredLevel).era;
}

ContentEra ContentPackRegistry::ResolveEraForItem(uint32 itemId, uint32 itemLevel,
                                                 uint32 requiredLevel, uint8 authoredExpansion) const
{
    return ResolveEraDetailsForItem(itemId, itemLevel, requiredLevel, authoredExpansion).era;
}

void ContentPackRegistry::Clear()
{
    _finalized = false;
    _packs.clear();
    _mapOverrides.clear();
    _areaOverrides.clear();
    _creatureOverrides.clear();
    _questOverrides.clear();
    _itemOverrides.clear();
}
