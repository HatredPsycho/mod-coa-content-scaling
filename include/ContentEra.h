/*
 * CoA Universal Content Scaling
 * ContentEra: Enumeration and identification of content progression eras.
 */

#ifndef COA_CONTENT_ERA_H
#define COA_CONTENT_ERA_H

#include "Define.h"
#include <string_view>

enum class ContentEra : uint8
{
    Classic = 0,
    TBC     = 1,
    WotLK   = 2,
    Custom  = 3
};

constexpr std::string_view ContentEraToString(ContentEra era)
{
    switch (era)
    {
        case ContentEra::Classic: return "Classic";
        case ContentEra::TBC:     return "TBC";
        case ContentEra::WotLK:   return "WotLK";
        case ContentEra::Custom:  return "Custom";
        default:                  return "Unknown";
    }
}

enum class EraResolutionSource : uint8
{
    ExplicitOverride       = 0,
    ExplicitAreaOverride   = 1,
    InstanceProfile        = 2,
    MapExpansion           = 3,
    AuthoredExpansion      = 4,
    ZoneSortMetadata       = 5,
    AuthoredLevelHeuristic = 6,
    SafeFallback           = 7
};

constexpr std::string_view EraResolutionSourceToString(EraResolutionSource source)
{
    switch (source)
    {
        case EraResolutionSource::ExplicitOverride:       return "ExplicitOverride";
        case EraResolutionSource::ExplicitAreaOverride:   return "ExplicitAreaOverride";
        case EraResolutionSource::InstanceProfile:        return "InstanceProfile";
        case EraResolutionSource::MapExpansion:           return "MapExpansion";
        case EraResolutionSource::AuthoredExpansion:      return "AuthoredExpansion";
        case EraResolutionSource::ZoneSortMetadata:       return "ZoneSortMetadata";
        case EraResolutionSource::AuthoredLevelHeuristic: return "AuthoredLevelHeuristic";
        case EraResolutionSource::SafeFallback:           return "SafeFallback";
        default:                                          return "Unknown";
    }
}

struct EraResolutionResult
{
    ContentEra era{ContentEra::Classic};
    EraResolutionSource source{EraResolutionSource::SafeFallback};
    float confidence{1.0f};
};

enum class MapContentKind : uint8
{
    WORLD        = 0,
    DUNGEON      = 1,
    RAID         = 2,
    BATTLEGROUND = 3,
    ARENA        = 4,
    CUSTOM_PVE   = 5,
    UNKNOWN      = 6
};

constexpr std::string_view MapContentKindToString(MapContentKind kind)
{
    switch (kind)
    {
        case MapContentKind::WORLD:        return "WORLD";
        case MapContentKind::DUNGEON:      return "DUNGEON";
        case MapContentKind::RAID:         return "RAID";
        case MapContentKind::BATTLEGROUND: return "BATTLEGROUND";
        case MapContentKind::ARENA:        return "ARENA";
        case MapContentKind::CUSTOM_PVE:   return "CUSTOM_PVE";
        default:                           return "UNKNOWN";
    }
}

constexpr bool IsPvEInstanceKind(MapContentKind kind)
{
    return kind == MapContentKind::DUNGEON || kind == MapContentKind::RAID || kind == MapContentKind::CUSTOM_PVE;
}

#endif // COA_CONTENT_ERA_H
