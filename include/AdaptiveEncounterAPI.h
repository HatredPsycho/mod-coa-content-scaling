/*
 * CoA Universal Content Scaling
 * AdaptiveEncounterAPI: Dynamic encounter mechanics adaptation for scalable group sizes.
 */

#ifndef COA_ADAPTIVE_ENCOUNTER_API_H
#define COA_ADAPTIVE_ENCOUNTER_API_H

#include "CoaLfgCompat.h"

#include "DBCEnums.h"
#include "Define.h"
#include "LFG.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <string_view>
#include <unordered_map>

enum class EncounterCompatibility : uint8
{
    AUTO            = 0,
    NEEDS_OVERRIDE  = 1,
    OVERRIDDEN      = 2,
    UNSUPPORTED     = 3
};

constexpr std::string_view CompatibilityToString(EncounterCompatibility comp)
{
    switch (comp)
    {
        case EncounterCompatibility::AUTO:           return "AUTO";
        case EncounterCompatibility::NEEDS_OVERRIDE: return "NEEDS_OVERRIDE";
        case EncounterCompatibility::OVERRIDDEN:     return "OVERRIDDEN";
        case EncounterCompatibility::UNSUPPORTED:    return "UNSUPPORTED";
        default:                                     return "UNKNOWN";
    }
}

enum class EncounterMechanicType : uint8
{
    TARGET_COUNT         = 0,
    ADD_COUNT            = 1,
    REQUIRED_PLAYERS     = 2,
    REQUIRED_INTERACTORS = 3,
    OBJECTIVE_COUNT      = 4,
    VEHICLE_COUNT        = 5,
    WAVE_SIZE            = 6,
    STACK_THRESHOLD      = 7,
    SPLIT_DIVISOR        = 8,
    TIMER_MS             = 9,
    FAIL_THRESHOLD       = 10,
    PROXIMITY_DISTANCE   = 11,
    HEALING_CONTRIBUTION = 12
};

struct EncounterAdapterKey
{
    uint32 mapId{0};
    uint32 encounterId{0};

    bool operator==(EncounterAdapterKey const& other) const = default;
};

template <>
struct std::hash<EncounterAdapterKey>
{
    std::size_t operator()(EncounterAdapterKey const& k) const noexcept
    {
        return (static_cast<std::size_t>(k.mapId) << 32) | k.encounterId;
    }
};

struct EncounterContext
{
    uint32 mapId{0};
    uint32 instanceId{0};
    uint32 encounterId{0};

    uint32 actualParticipants{1};
    float combatEffectivePlayers{1.0f};
    uint32 mechanicParticipants{1};

    uint32 intendedPlayers{5};
    uint32 challengeSize{0};

    Difficulty difficulty{DUNGEON_DIFFICULTY_NORMAL};
    lfg::LfgCompositionMode compositionMode{lfg::LfgCompositionMode::MATCHMAKING};

    bool isPhysicallySolo{true};
    bool isMechanicSolo{true};

    uint64 snapshotGeneration{0};

    // Backwards compatibility helper
    [[nodiscard]] float GetEffectivePlayers() const { return combatEffectivePlayers; }
    [[nodiscard]] bool IsSolo() const { return isMechanicSolo; }
};

class IEncounterAdapter
{
public:
    virtual ~IEncounterAdapter() = default;

    [[nodiscard]] virtual uint32 GetMapId() const = 0;
    [[nodiscard]] virtual uint32 GetEncounterId() const = 0;
    [[nodiscard]] virtual std::string_view GetName() const = 0;
    [[nodiscard]] virtual EncounterCompatibility GetCompatibility() const { return EncounterCompatibility::OVERRIDDEN; }

    [[nodiscard]] virtual uint32 ResolveMechanic(uint32 mechanicId, EncounterMechanicType type,
                                                 uint32 authoredValue, EncounterContext const& ctx) const
    {
        switch (type)
        {
            case EncounterMechanicType::TARGET_COUNT:
                return ScaleTargetCount(authoredValue, ctx);
            case EncounterMechanicType::ADD_COUNT:
                return ScaleAddCount(authoredValue, ctx);
            case EncounterMechanicType::REQUIRED_PLAYERS:
            case EncounterMechanicType::REQUIRED_INTERACTORS:
                return ScaleRequiredPlayers(authoredValue, ctx);
            default:
                return authoredValue;
        }
    }

    [[nodiscard]] virtual uint32 ScaleTargetCount(uint32 authoredTargets, EncounterContext const& ctx) const
    {
        if (ctx.isMechanicSolo)
            return 1;
        float const ratio = float(ctx.mechanicParticipants) / float(std::max(1u, ctx.intendedPlayers));
        return std::max(1u, static_cast<uint32>(std::round(float(authoredTargets) * ratio)));
    }

    [[nodiscard]] virtual uint32 ScaleAddCount(uint32 authoredAdds, EncounterContext const& ctx) const
    {
        if (ctx.isMechanicSolo)
            return std::max(1u, authoredAdds / 3);
        float const ratio = float(ctx.mechanicParticipants) / float(std::max(1u, ctx.intendedPlayers));
        return std::max(1u, static_cast<uint32>(std::round(float(authoredAdds) * ratio)));
    }

    [[nodiscard]] virtual uint32 ScaleRequiredPlayers(uint32 authoredRequirement, EncounterContext const& ctx) const
    {
        if (ctx.isMechanicSolo)
            return 1;
        return std::min(authoredRequirement, std::max(1u, ctx.mechanicParticipants));
    }
};

class AdaptiveEncounterMgr
{
public:
    static AdaptiveEncounterMgr* Instance();

    void RegisterAdapter(std::shared_ptr<IEncounterAdapter> adapter);
    [[nodiscard]] IEncounterAdapter const* GetAdapter(uint32 mapId, uint32 encounterId) const;
    [[nodiscard]] IEncounterAdapter const* GetAdapter(uint32 encounterId) const; // Fallback legacy lookup

    [[nodiscard]] uint32 ResolveMechanic(uint32 mapId, uint32 encounterId, uint32 mechanicId,
                                         EncounterMechanicType type, uint32 authoredValue,
                                         EncounterContext const& ctx) const;

    [[nodiscard]] uint32 ScaleTargetCount(uint32 encounterId, uint32 authoredTargets, EncounterContext const& ctx) const;
    [[nodiscard]] uint32 ScaleAddCount(uint32 encounterId, uint32 authoredAdds, EncounterContext const& ctx) const;
    [[nodiscard]] uint32 ScaleRequiredPlayers(uint32 encounterId, uint32 authoredRequirement, EncounterContext const& ctx) const;

    [[nodiscard]] EncounterCompatibility ClassifyEncounter(uint32 mapId, uint32 encounterId) const;
    [[nodiscard]] EncounterCompatibility ClassifyEncounter(uint32 encounterId) const;

    void Clear();

private:
    AdaptiveEncounterMgr() = default;
    std::unordered_map<EncounterAdapterKey, std::shared_ptr<IEncounterAdapter>> _adapters;
    std::unordered_map<uint32, std::shared_ptr<IEncounterAdapter>> _legacyAdapters;
};

#define sAdaptiveEncounterMgr AdaptiveEncounterMgr::Instance()

#endif // COA_ADAPTIVE_ENCOUNTER_API_H
