/*
 * CoA Universal Content Scaling
 * InstanceProfile: Explicit metadata and sizing rules for dungeons and raids across eras.
 */

#ifndef COA_INSTANCE_PROFILE_H
#define COA_INSTANCE_PROFILE_H

#include "ContentEra.h"
#include "ContentTier.h"
#include "Define.h"
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct InstanceProfile
{
    uint32 mapId{0};
    std::string name;
    ContentEra era{ContentEra::Classic};
    ContentTier tier{ContentTier::DUNGEON_NORMAL};
    MapContentKind kind{MapContentKind::DUNGEON};
    uint32 defaultGroupSize{5};
    uint32 raid10Size{10};
    uint32 raid25Size{25};
    bool isRaid{false};
};

class InstanceProfileRegistry
{
public:
    static InstanceProfileRegistry* Instance();

    // Builds a profile for every dungeon and raid the client data names, sized by MapDifficulty.dbc
    // and tiered by the authored raid tiers.
    void Initialize();
    void Clear();
    void RegisterProfile(InstanceProfile profile);
    void RegisterMapKind(uint32 mapId, MapContentKind kind);

    [[nodiscard]] InstanceProfile const* GetProfile(uint32 mapId) const;
    [[nodiscard]] std::optional<ContentEra> GetEraForMap(uint32 mapId, uint8 difficulty = 0) const;
    [[nodiscard]] ContentTier GetTierForMap(uint32 mapId, uint8 difficulty = 0) const;
    [[nodiscard]] uint32 GetIntendedPlayers(uint32 mapId, uint8 difficulty) const;
    [[nodiscard]] MapContentKind GetKindForMap(uint32 mapId) const;
    [[nodiscard]] bool HasProfile(uint32 mapId) const;
    [[nodiscard]] size_t GetProfileCount() const { return _profiles.size(); }

    [[nodiscard]] static ContentTier GetAuthoredTier(uint32 mapId, bool isRaid);
    [[nodiscard]] static bool IsCustomMap(uint32 mapId);

    [[nodiscard]] bool ValidateAll(std::vector<std::string>& issues) const;

private:
    InstanceProfileRegistry() = default;

    std::unordered_map<uint32, InstanceProfile> _profiles;
    std::unordered_map<uint32, MapContentKind> _kinds;
};

#define sInstanceProfileRegistry InstanceProfileRegistry::Instance()

#endif // COA_INSTANCE_PROFILE_H
