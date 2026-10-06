/*
 * CoA Universal Content Scaling
 * LfgAddonProtocolTest: the CoALFGMode addon's requests are read exactly, and anything else is left alone.
 */

#include "LfgAddonProtocol.h"
#include "gtest/gtest.h"

using namespace CoALfgAddon;

TEST(LfgAddonProtocol, BodyOnlyForItsOwnPrefix)
{
    EXPECT_EQ(Body("CoALFG\tGET"), std::optional<std::string_view>("GET"));
    EXPECT_FALSE(Body("CoACompanions\t1:2:3"));
    EXPECT_FALSE(Body("CoALFGX\tGET"));
    EXPECT_FALSE(Body("CoALFG GET"));
    EXPECT_FALSE(Body("CoALFG"));
}

TEST(LfgAddonProtocol, ReadsEveryRequest)
{
    std::optional<Request> get = ParseRequest("GET");
    ASSERT_TRUE(get);
    EXPECT_EQ(get->kind, RequestKind::Get);

    for (Mode mode : {Mode::Matchmaking, Mode::Bots, Mode::Party})
    {
        std::optional<Request> set = ParseRequest("MODE " + std::string(ModeName(mode)));
        ASSERT_TRUE(set);
        EXPECT_EQ(set->kind, RequestKind::SetMode);
        EXPECT_EQ(set->mode, mode);
    }

    std::optional<Request> adaptive = ParseRequest("CHALLENGE 0");
    ASSERT_TRUE(adaptive);
    EXPECT_EQ(adaptive->kind, RequestKind::SetChallenge);
    EXPECT_EQ(adaptive->challenge, 0u);

    std::optional<Request> raid = ParseRequest("CHALLENGE 40");
    ASSERT_TRUE(raid);
    EXPECT_EQ(raid->challenge, 40u);
}

TEST(LfgAddonProtocol, RejectsMalformedRequests)
{
    for (std::string_view body : {"", "get", "GET now", "MODE", "MODE solo", "MODE party extra", "CHALLENGE",
             "CHALLENGE 41", "CHALLENGE -1", "CHALLENGE 5x", "CHALLENGE  5", "RESET"})
        EXPECT_FALSE(ParseRequest(body)) << body;
}

TEST(LfgAddonProtocol, StateNamesModeChallengeAndAvailability)
{
    EXPECT_EQ(FormatState({Mode::Party, 0, true, false}), "STATE party 0 1 0");
    EXPECT_EQ(FormatState({Mode::Bots, 25, false, true}), "STATE bots 25 0 1");
    EXPECT_EQ(FormatState({}), "STATE matchmaking 0 0 0");
}
