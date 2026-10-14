/*
 * CoA Universal Content Scaling
 * JourneyAddon: what the ZoneLevels addon is told about the journey, in the shape mod-world-journey's addon reads.
 *
 *   B                                      a new set begins
 *   Z<key>=<low>-<high>;...                the levels of zones, as the world map knows them
 *   L<id>=<min>-<max>-<recommended>;...    the levels of the Dungeon Finder's dungeons
 *   E<before>~<after>                      a line of a gem or an enchantment: what it said, what it gives now
 *   D                                      the set is complete
 */

#ifndef COA_JOURNEY_ADDON_H
#define COA_JOURNEY_ADDON_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace journey::addon
{
    inline constexpr char const* Prefix = "ZoneLevels";
    inline constexpr std::size_t MaximumBody = 230;

    /// Entries of one kind, packed into as few messages as fit the client's addon line.
    std::vector<std::string> Chunk(char kind, std::vector<std::string> const& entries,
        std::size_t limit = MaximumBody);

    /// An enchantment's text with its numbers changed, each one in order and only where it stands as a whole number.
    std::string Retext(std::string text, std::vector<std::pair<uint32_t, uint32_t>> const& changes);

    /// Whether a line of the enchantment channel can travel: it may not hold the separators the addon splits on.
    bool CanCarry(std::string_view text);

    /// Whether a whisper to oneself is the addon asking for the set again.
    bool IsRequest(std::string_view message);
}

#endif // COA_JOURNEY_ADDON_H
