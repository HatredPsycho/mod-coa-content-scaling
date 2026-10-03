#include "ContentPackRegistry.h"
#include "GeneratedContentCensus.h"
#include "InstanceProfile.h"
#include "ItemBudgetScaler.h"
#include "Log.h"
#include <atomic>
#include <iostream>
#include <mutex>
#include <unordered_map>

InstanceProfileRegistry::InstanceProfileRegistry() = default;
InstanceProfileRegistry* InstanceProfileRegistry::Instance()
{
    static InstanceProfileRegistry registry;
    return &registry;
}
std::optional<ContentEra> InstanceProfileRegistry::GetEraForMap(uint32, uint8) const { return std::nullopt; }
// ACTUAL_KEYS
constexpr unsigned MAX_ITEM_PROTO_SPELLS = 5;
constexpr unsigned MAX_ITEM_PROTO_SOCKETS = 3;
struct ItemTemplate
{
    uint32 ItemId = 768, ItemLevel = 9, RequiredLevel = 4, ItemSet = 0;
    struct { uint32 SpellId = 0, SpellTrigger = 0; } Spells[MAX_ITEM_PROTO_SPELLS];
    struct { uint32 Color = 0; } Socket[MAX_ITEM_PROTO_SOCKETS];
};
struct Map { uint32 GetId() const { return 0; } };
struct Creature
{
    Map* GetMap() const { return nullptr; }
    uint32 GetAreaId() const { return 12; }
};
struct CreatureTemplate { uint32 Entry = 30; uint8 expansion = 0; };
struct Quest
{
    int32 GetQuestLevel() const { return 43; }
    uint32 GetMinLevel() const { return 39; }
    uint32 GetQuestId() const { return 8325; }
    int32 GetZoneOrSort() const { return 3431; }
};
struct Config
{
    bool scaleItems = true;
    template <typename T> T GetOption(char const*, T) { return T(scaleItems); }
} config;
Config* sConfigMgr = &config;
struct InstanceMgr { unsigned loads = 0; void LoadCalibratedBossFlex() { ++loads; } } instances;
InstanceMgr* sInstanceScalingMgr = &instances;
unsigned itemMutations = 0;
ItemBudgetScaler* ItemBudgetScaler::Instance() { static ItemBudgetScaler scaler; return &scaler; }
void ItemBudgetScaler::ScaleAllItems(ProgressionLayout const&) { ++itemMutations; }
namespace LocalLevelScaling
{
    std::atomic<bool> ContentScalingActive{true};
    std::atomic<void*> QuestBaseLevelOwner{reinterpret_cast<void*>(1)}, QuestMinLevelOwner{reinterpret_cast<void*>(1)},
        CreatureBaseLevelOwner{reinterpret_cast<void*>(1)}, QuestMoneyMaxLevelOwner{reinterpret_cast<void*>(1)},
        KillContentLevelOwner{reinterpret_cast<void*>(1)}, QuestRewardRateOwner{reinterpret_cast<void*>(1)};
}
using ObjectGuid = unsigned;
struct Group { uint8 GetMembersCount() const { return 2; } };
struct Player { Group* GetGroup() { return nullptr; } };
namespace ObjectAccessor { Player* FindPlayer(ObjectGuid) { return nullptr; } }
namespace lfg
{
    enum class LfgCompositionMode { MATCHMAKING, BOT_FILL, CURRENT_PARTY };
    struct LfgQueuePolicy
    {
        LfgCompositionMode compositionMode = LfgCompositionMode::MATCHMAKING;
        uint32 challengeSize = 0;
        bool bypassMatchmaking = false, requireStandardRoles = true;
        uint8 minPlayers = 5, targetPlayers = 5;
    };
}
struct ScriptMgr { bool HasLfgAutoFillProvider() { return false; } } scripts;
ScriptMgr* sScriptMgr = &scripts;
struct PlayerLfgSettings
{
    lfg::LfgCompositionMode compositionMode = lfg::LfgCompositionMode::CURRENT_PARTY;
    uint32 challengeSize = 20;
};
class CoAContentScaling
{
public:
    bool _enabled = false;
    unsigned layouts = 0, hooks = 0;
    ProgressionLayout _layout = ProgressionLayout::Create(60, true, true);
    std::mutex _lfgSettingsLock;
    std::unordered_map<ObjectGuid, PlayerLfgSettings> _playerLfgSettings;
    lfg::LfgCompositionMode _defaultLfgCompositionMode = lfg::LfgCompositionMode::CURRENT_PARTY;
    uint32 _defaultLfgChallengeSize = 20;
    void FinalizeAndInitialize();
    void InitializeLayout() { ++layouts; }
    void RegisterLocalLevelScalingHooks() { ++hooks; }
    void UnregisterLocalLevelScalingHooks();
    void OnResolveLfgQueuePolicy(ObjectGuid const&, lfg::LfgQueuePolicy&);
    uint8 GetEffectiveCreatureLevel(CreatureTemplate const*, Creature const*, uint8) const;
    int32 GetEffectiveQuestLevel(Quest const*) const;
    uint32 GetEffectiveQuestMinLevel(Quest const*) const;
};
// ACTUAL_METHODS

int main()
{
    unsigned failures = 0;
    auto check = [&failures](bool ok, char const* name)
    {
        if (!ok)
        {
            ++failures;
            std::cerr << "FAIL: " << name << '\n';
        }
    };
    auto* registry = sContentPackRegistry;
    registry->Clear();
    CoAContentScaling disabled;
    disabled.FinalizeAndInitialize();
    check(registry->IsFinalized(), "disabled lifecycle remains finalized for restart-only enable changes");
    check(disabled.layouts == 0 && instances.loads == 0 && itemMutations == 0 && disabled.hooks == 0,
          "disabled startup must not initialize scaling or mutate items");
    check(!LocalLevelScaling::ContentScalingActive && !LocalLevelScaling::QuestBaseLevelOwner &&
          !LocalLevelScaling::QuestMinLevelOwner && !LocalLevelScaling::CreatureBaseLevelOwner &&
          !LocalLevelScaling::QuestMoneyMaxLevelOwner && !LocalLevelScaling::KillContentLevelOwner &&
          !LocalLevelScaling::QuestRewardRateOwner, "disabled startup clears every local scaling callback");
    lfg::LfgQueuePolicy policy;
    disabled.OnResolveLfgQueuePolicy(1, policy);
    check(policy.compositionMode == lfg::LfgCompositionMode::MATCHMAKING && policy.challengeSize == 0 &&
          !policy.bypassMatchmaking && policy.requireStandardRoles && policy.minPlayers == 5 && policy.targetPlayers == 5,
          "disabled module must preserve the original LFG policy");
    CreatureTemplate creature;
    Quest quest;
    check(disabled.GetEffectiveCreatureLevel(&creature, nullptr, 45) == 45, "disabled creature level is unchanged");
    check(disabled.GetEffectiveQuestLevel(&quest) == 43 && disabled.GetEffectiveQuestMinLevel(&quest) == 39,
          "disabled quest and minimum levels are unchanged");
    registry->Clear();
    CoAContentScaling enabled;
    enabled._enabled = true;
    enabled.FinalizeAndInitialize();
    enabled.FinalizeAndInitialize();
    check(enabled.layouts == 1 && instances.loads == 1 && itemMutations == 1 && enabled.hooks == 1,
          "enabled startup initializes once and scales items");
    enabled.OnResolveLfgQueuePolicy(1, policy);
    check(policy.compositionMode == lfg::LfgCompositionMode::CURRENT_PARTY && policy.challengeSize == 20 &&
          policy.minPlayers == 1 && !policy.requireStandardRoles, "enabled LFG control remains active");
    registry->Clear();
    ItemTemplate item;
    auto original = ItemScalingContext::Resolve(&item);
    registry->RegisterItemOverride(item.ItemId, ContentEra::WotLK);
    auto overridden = ItemScalingContext::Resolve(&item);
    check(overridden.era == ContentEra::WotLK && overridden.hasGeneratedProfile, "item override beats census at runtime");
    check(original.tier == overridden.tier && original.sourceMap == overridden.sourceMap &&
          original.specialFlags == overridden.specialFlags && original.policy == overridden.policy,
          "era override retains the item's tier and safety policy");
    registry->Clear();
    unsigned preservedProfiles = 0;
    for (auto const& profile : sGeneratedItemProfiles)
    {
        if (profile.policy != uint8(ItemScalingPolicy::PRESERVE))
            continue;
        ++preservedProfiles;
        item.ItemId = profile.itemId;
        registry->RegisterItemOverride(item.ItemId, ContentEra::TBC);
        auto ctx = ItemScalingContext::Resolve(&item);
        check(ctx.era == ContentEra::TBC && ctx.policy == ItemScalingPolicy::PRESERVE,
              "explicit era override cannot remove PRESERVE protection");
        registry->Clear();
    }
    check(preservedProfiles > 0, "PRESERVE fixtures exist");
    std::cout << "Scaling controls regression: " << failures << " failures\n";
    return failures ? 1 : 0;
}
