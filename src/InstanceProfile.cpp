/*
 * CoA Universal Content Scaling
 * InstanceProfile: Implementation of instance profile registry with explicit sizing.
 */

#include "InstanceProfile.h"
#include "CoAContentScaling.h"
#include "DBCEnums.h"
#include "DBCStores.h"
#include <algorithm>
#include <array>
#include <utility>

namespace
{
    struct AuthoredRaidTier
    {
        uint32 mapId;
        ContentTier tier;
    };

    constexpr std::array<AuthoredRaidTier, 23> AuthoredRaidTiers
    {{
        { 309, ContentTier::RAID_ENTRY },    // Zul'Gurub
        { 509, ContentTier::RAID_ENTRY },    // Ruins of Ahn'Qiraj
        { 409, ContentTier::RAID_MID },      // Molten Core
        { 469, ContentTier::RAID_END },      // Blackwing Lair
        { 531, ContentTier::RAID_PINNACLE }, // Ahn'Qiraj Temple
        { 532, ContentTier::RAID_ENTRY },    // Karazhan
        { 565, ContentTier::RAID_ENTRY },    // Gruul's Lair
        { 544, ContentTier::RAID_ENTRY },    // Magtheridon's Lair
        { 548, ContentTier::RAID_MID },      // Serpentshrine Cavern
        { 550, ContentTier::RAID_MID },      // Tempest Keep
        { 534, ContentTier::RAID_END },      // The Battle for Mount Hyjal
        { 564, ContentTier::RAID_END },      // Black Temple
        { 568, ContentTier::RAID_END },      // Zul'Aman
        { 580, ContentTier::RAID_PINNACLE }, // Sunwell Plateau
        { 249, ContentTier::RAID_ENTRY },    // Onyxia's Lair
        { 533, ContentTier::RAID_ENTRY },    // Naxxramas
        { 615, ContentTier::RAID_ENTRY },    // The Obsidian Sanctum
        { 616, ContentTier::RAID_ENTRY },    // The Eye of Eternity
        { 624, ContentTier::RAID_ENTRY },    // Vault of Archavon
        { 603, ContentTier::RAID_MID },      // Ulduar
        { 649, ContentTier::RAID_END },      // Trial of the Crusader
        { 631, ContentTier::RAID_PINNACLE }, // Icecrown Citadel
        { 724, ContentTier::RAID_PINNACLE }  // The Ruby Sanctum
    }};

    // Ascension's own dungeons and world boss instances.
    constexpr std::array<uint32, 6> CustomMaps = { 169, 880, 883, 889, 890, 936 };

    // Naxxramas shipped with Classic, but the 3.3.5 data only has the level 80 version.
    constexpr uint32 NAXXRAMAS_MAP = 533;
    constexpr uint32 ONYXIAS_LAIR_MAP = 249;

    MapContentKind KindOf(MapEntry const* map)
    {
        if (InstanceProfileRegistry::IsCustomMap(map->MapID))
            return MapContentKind::CUSTOM_PVE;
        if (map->IsRaid())
            return MapContentKind::RAID;
        if (map->IsNonRaidDungeon())
            return MapContentKind::DUNGEON;
        if (map->IsBattleground())
            return MapContentKind::BATTLEGROUND;
        if (map->IsBattleArena())
            return MapContentKind::ARENA;
        if (map->IsWorldMap())
            return MapContentKind::WORLD;
        return MapContentKind::UNKNOWN;
    }

    ContentEra EraOf(MapEntry const* map)
    {
        if (InstanceProfileRegistry::IsCustomMap(map->MapID))
            return ContentEra::Custom;
        if (map->MapID == NAXXRAMAS_MAP)
            return ContentEra::WotLK;

        switch (map->Expansion())
        {
            case 0:  return ContentEra::Classic;
            case 1:  return ContentEra::TBC;
            default: return ContentEra::WotLK;
        }
    }

    uint32 MaxPlayers(MapEntry const* map, Difficulty difficulty)
    {
        if (MapDifficulty const* data = GetMapDifficultyData(map->MapID, difficulty))
            if (data->maxPlayers)
                return data->maxPlayers;
        return 0;
    }
}

InstanceProfileRegistry* InstanceProfileRegistry::Instance()
{
    static InstanceProfileRegistry instance;
    return &instance;
}

ContentTier InstanceProfileRegistry::GetAuthoredTier(uint32 mapId, bool isRaid)
{
    for (AuthoredRaidTier const& authored : AuthoredRaidTiers)
        if (authored.mapId == mapId)
            return authored.tier;

    return isRaid ? ContentTier::RAID_ENTRY : ContentTier::DUNGEON_NORMAL;
}

bool InstanceProfileRegistry::IsCustomMap(uint32 mapId)
{
    return std::find(CustomMaps.begin(), CustomMaps.end(), mapId) != CustomMaps.end();
}

void InstanceProfileRegistry::Clear()
{
    _profiles.clear();
    _kinds.clear();
}

void InstanceProfileRegistry::RegisterProfile(InstanceProfile profile)
{
    _kinds[profile.mapId] = profile.kind;
    _profiles[profile.mapId] = std::move(profile);
}

void InstanceProfileRegistry::RegisterMapKind(uint32 mapId, MapContentKind kind)
{
    _kinds[mapId] = kind;
}

void InstanceProfileRegistry::Initialize()
{
    Clear();

    for (MapEntry const* map : sMapStore)
    {
        if (!map)
            continue;

        MapContentKind const kind = KindOf(map);
        RegisterMapKind(map->MapID, kind);

        // Enforce strict PvP exclusion: Battlegrounds and Arenas must NEVER enter PvE registry
        if (!IsPvEInstanceKind(kind) || !map->IsDungeon())
            continue;

        bool const isRaid = map->IsRaid();
        uint32 const normalSize = MaxPlayers(map, REGULAR_DIFFICULTY);
        uint32 const defaultSize = normalSize ? normalSize : (map->maxPlayers ? map->maxPlayers : (isRaid ? 10u : 5u));
        uint32 const largeSize = isRaid ? MaxPlayers(map, RAID_DIFFICULTY_25MAN_NORMAL) : 0;

        RegisterProfile(InstanceProfile{
            .mapId = map->MapID,
            .name = map->name[0] ? map->name[0] : "",
            .era = EraOf(map),
            .tier = GetAuthoredTier(map->MapID, isRaid),
            .kind = kind,
            .defaultGroupSize = defaultSize,
            .raid10Size = 10u,
            .raid25Size = largeSize ? largeSize : (defaultSize >= 20 ? defaultSize : 25u),
            .isRaid = isRaid
        });
    }
}

InstanceProfile const* InstanceProfileRegistry::GetProfile(uint32 mapId) const
{
    auto it = _profiles.find(mapId);
    return (it != _profiles.end()) ? &it->second : nullptr;
}

std::optional<ContentEra> InstanceProfileRegistry::GetEraForMap(uint32 mapId, uint8 difficulty) const
{
    if (mapId == ONYXIAS_LAIR_MAP)
    {
        // Reused Onyxia: 25-man (1, 3) and 10-man heroic (2) are WotLK era.
        if (difficulty == RAID_DIFFICULTY_25MAN_NORMAL ||
            difficulty == RAID_DIFFICULTY_10MAN_HEROIC ||
            difficulty == RAID_DIFFICULTY_25MAN_HEROIC)
            return ContentEra::WotLK;

        // Difficulty 0 is 10-man normal in WotLK, or 40-man Classic when WotLK is inactive
        if (difficulty == RAID_DIFFICULTY_10MAN_NORMAL)
            return sCoAContentScaling->IsWotlkEnabled() ? ContentEra::WotLK : ContentEra::Classic;

        return ContentEra::Classic;
    }

    if (auto const* profile = GetProfile(mapId))
        return profile->era;
    return std::nullopt;
}

ContentTier InstanceProfileRegistry::GetTierForMap(uint32 mapId, uint8 difficulty) const
{
    if (auto const* profile = GetProfile(mapId))
    {
        if (!profile->isRaid && (difficulty == DUNGEON_DIFFICULTY_HEROIC))
            return ContentTier::DUNGEON_HEROIC;
        return profile->tier;
    }
    return ContentTier::WORLD;
}

uint32 InstanceProfileRegistry::GetIntendedPlayers(uint32 mapId, uint8 difficulty) const
{
    auto const* profile = GetProfile(mapId);
    if (!profile)
        return 5;

    if (!profile->isRaid)
        return profile->defaultGroupSize;

    if (profile->era == ContentEra::Classic)
    {
        // Onyxia map 249: difficulty 10/25 in WotLK, otherwise 40
        if (mapId == ONYXIAS_LAIR_MAP && (difficulty == RAID_DIFFICULTY_10MAN_NORMAL || difficulty == RAID_DIFFICULTY_10MAN_HEROIC))
            return 10;
        if (mapId == ONYXIAS_LAIR_MAP && (difficulty == RAID_DIFFICULTY_25MAN_NORMAL || difficulty == RAID_DIFFICULTY_25MAN_HEROIC))
            return 25;

        return profile->defaultGroupSize;
    }

    if (difficulty == RAID_DIFFICULTY_10MAN_NORMAL || difficulty == RAID_DIFFICULTY_10MAN_HEROIC)
        return profile->raid10Size;

    return profile->raid25Size;
}

MapContentKind InstanceProfileRegistry::GetKindForMap(uint32 mapId) const
{
    auto it = _kinds.find(mapId);
    return it != _kinds.end() ? it->second : MapContentKind::UNKNOWN;
}

bool InstanceProfileRegistry::HasProfile(uint32 mapId) const
{
    return _profiles.count(mapId) > 0;
}

bool InstanceProfileRegistry::ValidateAll(std::vector<std::string>& issues) const
{
    issues.clear();
    for (auto const& [mapId, prof] : _profiles)
    {
        // Enforce strict PvP exclusion
        if (prof.kind == MapContentKind::BATTLEGROUND || prof.kind == MapContentKind::ARENA)
            issues.push_back("PvP map " + std::to_string(mapId) + " (" + prof.name + ") found in PvE InstanceProfileRegistry");

        if (prof.defaultGroupSize == 0)
            issues.push_back("Map " + std::to_string(mapId) + " (" + prof.name + "): defaultGroupSize is 0");

        if (prof.isRaid)
        {
            if (prof.era == ContentEra::Classic && prof.defaultGroupSize != 10 && prof.defaultGroupSize != 20 && prof.defaultGroupSize != 25 && prof.defaultGroupSize != 40)
                issues.push_back("Classic raid " + prof.name + " has unexpected size: " + std::to_string(prof.defaultGroupSize));
            else if (prof.era == ContentEra::TBC && prof.defaultGroupSize != 10 && prof.defaultGroupSize != 25)
                issues.push_back("TBC raid " + prof.name + " has non-standard raid sizing");
            else if (prof.era == ContentEra::WotLK && (prof.raid10Size != 10 || prof.raid25Size != 25))
                issues.push_back("WotLK raid " + prof.name + " has non-standard raid sizing");
        }
    }
    return issues.empty();
}
