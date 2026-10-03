#include "ContentPackRegistry.h"
#include "GeneratedContentCensus.h"
#include "InstanceProfile.h"
#include <iostream>

InstanceProfileRegistry::InstanceProfileRegistry() = default;

InstanceProfileRegistry* InstanceProfileRegistry::Instance()
{
    static InstanceProfileRegistry registry;
    return &registry;
}

std::optional<ContentEra> InstanceProfileRegistry::GetEraForMap(uint32, uint8) const
{
    return std::nullopt;
}

class TbcPack : public IContentPack
{
public:
    ContentEra GetEra() const override { return ContentEra::TBC; }
    std::string_view GetName() const override { return "TBC"; }
    LevelRange GetSourceLevelRange() const override { return {58, 70}; }
    bool HandlesMap(uint32 map) const override { return map == 530; }
};

int main()
{
    auto* registry = sContentPackRegistry;
    registry->Clear();
    registry->RegisterPack(std::make_shared<TbcPack>());
    unsigned failures = 0;
    auto check = [&failures](bool condition, char const* name)
    {
        if (!condition)
        {
            ++failures;
            std::cerr << "FAIL: " << name << '\n';
        }
    };
    for (uint32 area : {3430u, 3433u, 3487u, 3524u, 3525u, 3557u, 3431u, 3526u, 10141u, 10142u})
    {
        check(registry->ResolveEraForArea(area, 530) == ContentEra::Classic, "starter area");
        check(registry->ResolveEraForCreature(15274, 530, area, 0, 1) == ContentEra::Classic, "starter creature");
        check(registry->ResolveEraForQuest(8325, area, 1, 1) == ContentEra::Classic, "starter quest");
    }
    for (uint8 cap : {60, 70, 80})
    {
        auto layout = ProgressionLayout::Create(cap, true, true);
        for (uint32 entry : {15274u, 16520u})
        {
            auto const era = registry->ResolveEraForCreature(entry, 530, 0, 0, 1);
            check(era == ContentEra::Classic, "starter census without runtime area");
            check(layout.MapAuthoredToEffective(era, 1) == 1, "starter remains level one at every cap");
        }
    }
    for (uint32 quest : {8325u, 9279u})
        check(registry->ResolveEraForQuest(quest, 0, 0, 1) == ContentEra::Classic, "starter quest census");
    check(registry->ResolveEraForItem(768, 9, 4, 0) == ContentEra::Classic, "starter loot source");
    check(registry->ResolveEraForCreature(15274, 530, 3483, 0, 1) == ContentEra::TBC, "actual Outland placement wins over coarse census");
    check(registry->ResolveEraForCreature(999999, 530, 3483, 1, 60) == ContentEra::TBC, "Outland remains TBC");
    check(registry->ResolveEraForQuest(10129, 3483, 1, 58) == ContentEra::TBC, "Outland quest remains TBC");
    check(registry->ResolveEraForCreature(999999, 571, 0, 2, 75) == ContentEra::WotLK, "Northrend remains WotLK");
    registry->RegisterAreaOverride(3431, ContentEra::Custom);
    check(registry->ResolveEraForCreature(15274, 530, 3431, 0, 1) == ContentEra::Custom, "area override");
    check(registry->ResolveEraForQuest(8325, 3431, 0, 1) == ContentEra::Custom, "quest area override");
    registry->RegisterCreatureOverride(15274, ContentEra::WotLK);
    check(registry->ResolveEraForCreature(15274, 530, 3431, 0, 1) == ContentEra::WotLK, "creature override");
    registry->RegisterQuestOverride(8325, ContentEra::WotLK);
    check(registry->ResolveEraForQuest(8325, 3431, 0, 1) == ContentEra::WotLK, "quest override");
    registry->Clear();
    registry->RegisterMapOverride(530, ContentEra::Custom);
    check(registry->ResolveEraForArea(3431, 530) == ContentEra::Custom, "area map override");
    check(registry->ResolveEraForCreature(15274, 530, 3431, 0, 1) == ContentEra::Custom, "map override");
    registry->Clear();
    std::cout << "Starting zone regression: " << failures << " failures\n";
    return failures ? 1 : 0;
}
