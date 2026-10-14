/*
 * CoA Universal Content Scaling
 * JourneyRules: what a level, an item, an amount of money becomes on the journey layout. Plain arithmetic,
 * nothing of the core, so the test bench reads the same rules the server applies.
 *
 * Ported from mod-world-journey by aspollon (https://github.com/aspollon/mod-world-journey, faed6d6),
 * released under GNU AGPL v3. The rules read their shape from an object instead of a global.
 */

#ifndef COA_JOURNEY_RULES_H
#define COA_JOURNEY_RULES_H

#include <cstdint>
#include <map>

namespace journey
{
    struct Range
    {
        int from = 1;
        int to = 1;
    };

    /// Everything the regulators decide. The original levels each part is laid over are fixed: the old world's
    /// zones open from 1 to 55, Outland's from 58 to 67, Northrend's from 68 to 77.
    struct Shape
    {
        bool outland = true;                    // a part left out keeps its original levels
        bool northrend = true;
        Range classic{ 1, 35 };
        Range outlandRange{ 30, 43 };
        Range northrendRange{ 40, 55 };
        int endgame = 60;                       // the level of Outland's and Northrend's heroics and raids
        int bossLevels = 3;                     // how far above it their bosses may stand
        Range outlandItems{ 75, 110 };          // item levels of Outland's top rewards at the endgame
        Range northrendItems{ 95, 140 };
        std::map<uint32_t, int> zoneEntries;    // zone id -> entry level, set by hand
        std::map<uint32_t, int> instanceEntries; // map id -> entry level, set by hand

        /// The shape with its regulators held inside the limits the rules are written for.
        [[nodiscard]] Shape Normalized() const;
    };

    enum Era : uint8_t
    {
        ERA_KEEP,           // left as it is: classic raids, world bosses, what is above the journey
        ERA_CLASSIC,        // the old world, its dungeons, the starting zones of the blood elves and draenei
        ERA_OUTLAND,        // Outland and its normal dungeons
        ERA_NORTHREND,      // Northrend and its normal dungeons
        ERA_TOP_OUTLAND,    // raids and heroic dungeons of Outland: the end of the journey
        ERA_TOP_NORTHREND   // ... and of Northrend
    };

    enum InstanceKind : uint8_t { INSTANCE_DUNGEON, INSTANCE_HEROIC, INSTANCE_RAID };

    enum ItemEra : uint8_t
    {
        ITEM_KEEP,          // no level, or the raids of the old world: as it is
        ITEM_CLASSIC,
        ITEM_TBC,
        ITEM_WOTLK
    };

    struct ItemPlan
    {
        ItemEra era = ITEM_KEEP;
        bool endgame = false;   // a reward of the raids, heroics and arenas of Outland or Northrend
        int requiredLevel = 0;
        int itemLevel = 0;
    };

    /// The era of something found on a map, at a level.
    Era EraOf(uint32_t map, int level, bool heroic);
    /// Something that is nowhere to be found: judged by its level.
    Era EraByLevel(int level);
    bool IsInstanceOfOutland(uint32_t map);
    bool IsInstanceOfNorthrend(uint32_t map);
    bool IsClassicRaid(uint32_t map);

    /// The part of the world an era belongs to: 0 old world, 1 Outland, 2 Northrend, 3 the endgame of Outland and
    /// Northrend.
    int PartOf(Era era);

    /// Money goes with the square of the level, roughly; it shrinks with it.
    uint32_t ScaleMoney(uint32_t money, int from, int to);

    /// Where a creature lifted to a character stands, relative to that character: from its place in its zone - the
    /// zone's lowest creatures `below` levels under the character, its highest `above` levels over - plus one for
    /// an elite or a rare, within the window.
    int WindowOffset(int originalLevel, int zoneOriginalEntry, int part, bool eliteOrRare, int below, int above);

    class Rules
    {
    public:
        Rules() = default;
        explicit Rules(Shape const& shape);

        [[nodiscard]] Shape const& GetShape() const { return _shape; }

        /// The old world: entries 1..55 laid over the classic range.
        [[nodiscard]] int Classic(int level) const;
        /// Outland: entries 58..67 laid over its range; below 58 like the old world.
        [[nodiscard]] int Outland(int level) const;
        /// Northrend: entries 68..77 laid over its range, never above the endgame; below 68 like the old world.
        [[nodiscard]] int Northrend(int level) const;
        /// Raids and heroics: their trash at the endgame, their bosses a few levels above.
        [[nodiscard]] int Top(int level, int base) const;
        [[nodiscard]] int Compress(int level, Era era) const;

        /// The entry level of a zone, from its original one and the part it belongs to (0 classic, 1 Outland, 2
        /// Northrend). A level set by hand wins.
        [[nodiscard]] int ZoneEntry(uint32_t zone, int part, int originalEntry, bool manual = true) const;
        /// The entry level of an instance, from its original entry level and the level of its creatures.
        [[nodiscard]] int InstanceEntry(uint32_t map, InstanceKind kind, int originalEntry, int contentLevel,
            bool manual = true) const;

        /// What an item becomes: its required level follows the journey, its item level follows the required
        /// level - so that the stats of a level-37 item are those of a level-37 item, whichever game it came from.
        [[nodiscard]] ItemPlan PlanItem(int quality, int requiredLevel, int itemLevel) const;

    private:
        Shape _shape;
    };
}

#endif // COA_JOURNEY_RULES_H
