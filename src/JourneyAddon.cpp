/*
 * CoA Universal Content Scaling
 * JourneyAddon: Implementation of the ZoneLevels addon channel's packing.
 *
 * The message shape follows mod-world-journey by aspollon (JourneyServer.cpp, faed6d6, GNU AGPL v3).
 */

#include "JourneyAddon.h"
#include <cctype>

namespace journey::addon
{
    std::vector<std::string> Chunk(char kind, std::vector<std::string> const& entries, std::size_t limit)
    {
        std::vector<std::string> messages;
        std::string chunk;
        for (std::string const& entry : entries)
        {
            if (!chunk.empty() && chunk.size() + entry.size() > limit)
            {
                messages.push_back(kind + chunk);
                chunk.clear();
            }
            chunk += entry;
        }
        if (!chunk.empty())
            messages.push_back(kind + chunk);
        return messages;
    }

    std::string Retext(std::string text, std::vector<std::pair<uint32_t, uint32_t>> const& changes)
    {
        std::size_t from = 0;
        for (auto const& [before, after] : changes)
        {
            std::string const old = std::to_string(before);
            std::size_t at = text.find(old, from);
            while (at != std::string::npos &&
                ((at > 0 && std::isdigit(static_cast<unsigned char>(text[at - 1]))) ||
                 (at + old.size() < text.size() && std::isdigit(static_cast<unsigned char>(text[at + old.size()])))))
                at = text.find(old, at + 1);
            if (at == std::string::npos)
                continue;

            std::string const now = std::to_string(after);
            text.replace(at, old.size(), now);
            from = at + now.size();
        }
        return text;
    }

    bool CanCarry(std::string_view text)
    {
        return !text.empty() && text.find_first_of("~^\t") == std::string_view::npos;
    }

    bool IsRequest(std::string_view message)
    {
        std::string const request = std::string(Prefix) + "\t?";
        return message.substr(0, request.size()) == request;
    }
}
