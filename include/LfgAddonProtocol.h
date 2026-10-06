/*
 * CoA Universal Content Scaling
 * LfgAddonProtocol: the messages the CoALFGMode addon and the server exchange about a player's
 * Dungeon Finder mode and challenge size.
 *
 * The client whispers itself in LANG_ADDON ("CoALFG\t<request>"), the server answers the same
 * way with the current state. Requests: "GET", "MODE <matchmaking|bots|party>",
 * "CHALLENGE <0..40>". Answer: "STATE <mode> <challenge> <scaling 0|1> <botfill 0|1>".
 */

#ifndef COA_LFG_ADDON_PROTOCOL_H
#define COA_LFG_ADDON_PROTOCOL_H

#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace CoALfgAddon
{
    constexpr std::string_view Prefix = "CoALFG";
    constexpr std::uint32_t MaxChallengeSize = 40;

    enum class Mode : std::uint8_t
    {
        Matchmaking = 0,
        Bots        = 1,
        Party       = 2
    };

    enum class RequestKind : std::uint8_t
    {
        Get,
        SetMode,
        SetChallenge
    };

    struct Request
    {
        RequestKind kind{RequestKind::Get};
        Mode mode{Mode::Matchmaking};
        std::uint32_t challenge{0};
    };

    struct State
    {
        Mode mode{Mode::Matchmaking};
        std::uint32_t challenge{0};
        bool scalingEnabled{false};
        bool botFillAvailable{false};
    };

    inline std::string_view ModeName(Mode mode)
    {
        switch (mode)
        {
            case Mode::Bots:
                return "bots";
            case Mode::Party:
                return "party";
            default:
                return "matchmaking";
        }
    }

    inline std::optional<Mode> ParseMode(std::string_view name)
    {
        if (name == "matchmaking")
            return Mode::Matchmaking;
        if (name == "bots")
            return Mode::Bots;
        if (name == "party")
            return Mode::Party;
        return std::nullopt;
    }

    /// The request part of an addon whisper, or nothing when the whisper belongs to another addon.
    inline std::optional<std::string_view> Body(std::string_view message)
    {
        if (message.size() <= Prefix.size() || message.substr(0, Prefix.size()) != Prefix)
            return std::nullopt;
        if (message[Prefix.size()] != '\t')
            return std::nullopt;
        return message.substr(Prefix.size() + 1);
    }

    inline std::optional<Request> ParseRequest(std::string_view body)
    {
        std::size_t const space = body.find(' ');
        std::string_view const verb = body.substr(0, space);
        std::string_view const argument = space == std::string_view::npos ? std::string_view() : body.substr(space + 1);

        if (verb == "GET" && argument.empty())
            return Request{RequestKind::Get};

        if (verb == "MODE")
        {
            if (std::optional<Mode> mode = ParseMode(argument))
                return Request{RequestKind::SetMode, *mode};
            return std::nullopt;
        }

        if (verb == "CHALLENGE" && !argument.empty())
        {
            std::uint32_t value = 0;
            auto const [end, error] = std::from_chars(argument.data(), argument.data() + argument.size(), value);
            if (error != std::errc() || end != argument.data() + argument.size() || value > MaxChallengeSize)
                return std::nullopt;
            return Request{RequestKind::SetChallenge, Mode::Matchmaking, value};
        }

        return std::nullopt;
    }

    inline std::string FormatState(State const& state)
    {
        return "STATE " + std::string(ModeName(state.mode)) + ' ' + std::to_string(state.challenge) + ' ' +
            (state.scalingEnabled ? '1' : '0') + ' ' + (state.botFillAvailable ? '1' : '0');
    }
}

#endif
