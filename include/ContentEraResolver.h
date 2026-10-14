/*
 * CoA Universal Content Scaling
 * ContentEraResolver: Which expansion a map, area, creature, quest or item belongs to.
 */

#ifndef COA_CONTENT_ERA_RESOLVER_H
#define COA_CONTENT_ERA_RESOLVER_H

#include "ContentEra.h"
#include "Define.h"
#include <unordered_map>
#include <unordered_set>

class ContentEraResolver
{
public:
    static ContentEraResolver* Instance();

    // Reads the expansion of every map and the map of every area from the client data. Lifecycle
    // control: registration is allowed only before finalization.
    void LoadClientData();
    void Finalize();
    [[nodiscard]] bool IsFinalized() const { return _finalized; }

    void RegisterMapExpansion(uint32 mapId, uint8 expansion);
    void RegisterAreaMap(uint32 areaId, uint32 mapId);
    // Areas that shipped with an expansion but were written for Classic levels: the Blood Elf and
    // Draenei starting zones on the Outland map.
    void RegisterClassicArea(uint32 areaId);

    // Manual overrides
    void RegisterMapOverride(uint32 mapId, ContentEra era);
    void RegisterAreaOverride(uint32 areaId, ContentEra era);
    void RegisterCreatureOverride(uint32 entry, ContentEra era);
    void RegisterQuestOverride(uint32 questId, ContentEra era);
    void RegisterItemOverride(uint32 itemId, ContentEra era);

    // Resolution chain:
    // 1. Explicit entry override
    // 2. Explicit area override
    // 3. Explicit instance/map profile
    // 4. Map expansion from the client data
    // 5. Authored expansion metadata
    // 6. Zone/sort metadata
    // 7. Authored level heuristic (last resort)
    static constexpr uint32 MAP_UNSPECIFIED = 0xFFFFFFFF;

    [[nodiscard]] EraResolutionResult ResolveEraDetailsForMap(uint32 mapId, uint8 authoredExpansion = 0) const;
    [[nodiscard]] EraResolutionResult ResolveEraDetailsForArea(uint32 areaId, uint32 mapId = MAP_UNSPECIFIED, uint8 authoredExpansion = 0) const;
    [[nodiscard]] EraResolutionResult ResolveEraDetailsForCreature(uint32 creatureEntry, uint32 mapId = MAP_UNSPECIFIED, uint32 areaId = 0,
                                                                   uint8 authoredExpansion = 0, uint8 authoredLevel = 0) const;
    [[nodiscard]] EraResolutionResult ResolveEraDetailsForQuest(uint32 questId, int32 zoneOrSort = 0,
                                                                uint8 authoredExpansion = 0, uint8 authoredLevel = 0) const;
    [[nodiscard]] EraResolutionResult ResolveEraDetailsForItem(uint32 itemId, uint32 itemLevel = 0,
                                                               uint32 requiredLevel = 0, uint8 authoredExpansion = 0) const;

    [[nodiscard]] ContentEra ResolveEraForMap(uint32 mapId, uint8 authoredExpansion = 0) const;
    [[nodiscard]] ContentEra ResolveEraForArea(uint32 areaId, uint32 mapId = MAP_UNSPECIFIED, uint8 authoredExpansion = 0) const;
    [[nodiscard]] ContentEra ResolveEraForCreature(uint32 creatureEntry, uint32 mapId = MAP_UNSPECIFIED, uint32 areaId = 0,
                                                   uint8 authoredExpansion = 0, uint8 authoredLevel = 0) const;
    [[nodiscard]] ContentEra ResolveEraForQuest(uint32 questId, int32 zoneOrSort = 0,
                                                uint8 authoredExpansion = 0, uint8 authoredLevel = 0) const;
    [[nodiscard]] ContentEra ResolveEraForItem(uint32 itemId, uint32 itemLevel = 0,
                                               uint32 requiredLevel = 0, uint8 authoredExpansion = 0) const;

    [[nodiscard]] bool IsClassicArea(uint32 areaId) const { return _classicAreas.count(areaId) > 0; }
    [[nodiscard]] size_t GetMapExpansionCount() const { return _mapExpansions.size(); }
    [[nodiscard]] size_t GetAreaMapCount() const { return _areaMaps.size(); }
    [[nodiscard]] size_t GetClassicAreaCount() const { return _classicAreas.size(); }

    void Clear();

private:
    ContentEraResolver() = default;

    bool _finalized{false};

    std::unordered_map<uint32, ContentEra> _mapExpansions;
    std::unordered_map<uint32, uint32> _areaMaps;
    std::unordered_set<uint32> _classicAreas;

    std::unordered_map<uint32, ContentEra> _mapOverrides;
    std::unordered_map<uint32, ContentEra> _areaOverrides;
    std::unordered_map<uint32, ContentEra> _creatureOverrides;
    std::unordered_map<uint32, ContentEra> _questOverrides;
    std::unordered_map<uint32, ContentEra> _itemOverrides;
};

#define sContentEraResolver ContentEraResolver::Instance()

#endif // COA_CONTENT_ERA_RESOLVER_H
