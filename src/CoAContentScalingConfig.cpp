/*
 * CoA Universal Content Scaling
 * CoAContentScalingConfig: Implementation of canonical parsers.
 */

#include "CoAContentScalingConfig.h"
#include "Log.h"
#include <algorithm>
#include <cctype>

namespace CoAContentScalingConfig
{
    std::string Trim(std::string_view str)
    {
        size_t start = 0;
        while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start])))
            ++start;

        size_t end = str.size();
        while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1])))
            --end;

        return std::string(str.substr(start, end - start));
    }

    std::unordered_set<uint32> ParseMapList(std::string_view raw)
    {
        std::unordered_set<uint32> maps;
        while (!raw.empty())
        {
            size_t const comma = raw.find(',');
            std::string const token = Trim(raw.substr(0, comma));
            raw = comma == std::string_view::npos ? std::string_view() : raw.substr(comma + 1);

            if (token.empty())
                continue;

            if (std::all_of(token.begin(), token.end(), [](unsigned char c) { return std::isdigit(c); }) &&
                token.size() <= 9)
                maps.insert(static_cast<uint32>(std::stoul(token)));
            else
                LOG_WARN("module.coa_content_scaling",
                         "CoAContentScaling: Ignoring '{}' in AuthenticMaps, it is not a map id.", token);
        }

        return maps;
    }

    std::string ParseProgressionMode(std::string_view raw)
    {
        std::string s = Trim(raw);
        std::string lower = s;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });

        if (lower == "custom")
            return "Custom";

        if (lower != "auto" && !lower.empty())
        {
            LOG_WARN("module.coa_content_scaling",
                     "CoAContentScaling: Invalid Progression.Mode '{}', falling back to 'Auto'.", s);
        }

        return "Auto";
    }

    SoloAssistMode ParseSoloAssistMode(std::string_view raw)
    {
        std::string s = Trim(raw);
        std::string lower = s;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });

        if (lower == "full" || lower == "2")
            return SoloAssistMode::FULL;

        if (lower == "light" || lower == "1")
            return SoloAssistMode::LIGHT;

        if (lower == "none" || lower == "0" || lower == "disabled" || lower == "off")
            return SoloAssistMode::NONE;

        if (!lower.empty())
        {
            LOG_WARN("module.coa_content_scaling",
                     "CoAContentScaling: Invalid SoloAssist.Mode '{}', safely falling back to 'None'.", s);
        }

        return SoloAssistMode::NONE;
    }

    lfg::LfgCompositionMode ParseLfgCompositionMode(std::string_view raw)
    {
        std::string s = Trim(raw);
        std::string lower = s;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });

        if (lower == "botfill" || lower == "bots" || lower == "bot" || lower == "fill")
            return lfg::LfgCompositionMode::BOT_FILL;

        if (lower == "currentparty" || lower == "party" || lower == "premade" || lower == "solo")
            return lfg::LfgCompositionMode::CURRENT_PARTY;

        if (lower == "matchmaking" || lower == "match" || lower == "standard" || lower == "normal")
            return lfg::LfgCompositionMode::MATCHMAKING;

        if (!lower.empty())
        {
            LOG_WARN("module.coa_content_scaling",
                     "CoAContentScaling: Invalid LFG.DefaultMode '{}', safely falling back to 'Matchmaking'.", s);
        }

        return lfg::LfgCompositionMode::MATCHMAKING;
    }
}
