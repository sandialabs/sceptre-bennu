#include "doctest.h"

#include <set>
#include <string>
#include <vector>

#include "bennu/distributed/Client.hpp"
#include "bennu/distributed/Utils.hpp"

using bennu::distributed::chunkMessage;
using bennu::distributed::Client;
using bennu::distributed::split;

static std::string tags(int n, bool trailingComma)
{
    std::string s;
    for (int i = 0; i < n; i++)
    {
        s += "tag" + std::to_string(i) + ":" + std::to_string(i) + ".5";
        if (i < n - 1 || trailingComma)
        {
            s += ",";
        }
    }
    return s;
}

static std::set<std::string> delivered(const std::vector<std::string>& chunks)
{
    std::set<std::string> out;
    for (auto& c : chunks)
    {
        for (auto& p : split(c, ","))
        {
            if (!p.empty())
            {
                out.insert(p);
            }
        }
    }
    return out;
}

TEST_CASE("chunkMessage delivers every tag of an 800-tag message")
{
    const size_t mtu = 1500;
    for (bool trailing : {false, true})
    {
        auto chunks = chunkMessage(tags(800, trailing), mtu);
        CHECK(delivered(chunks).size() == 800);
        for (auto& c : chunks)
        {
            CHECK(c.length() < mtu);
        }
    }
}

TEST_CASE("chunkMessage edge cases")
{
    CHECK(chunkMessage("", 100).empty());
    CHECK(chunkMessage(",,,", 100).empty());

    auto one = chunkMessage("a:1,b:2", 100);
    REQUIRE(one.size() == 1);
    CHECK(one[0] == "a:1,b:2,");

    // a single part longer than mtu goes out alone, never as an empty chunk
    auto big = chunkMessage("a:1," + std::string(50, 'x') + ",b:2", 20);
    REQUIRE(big.size() == 3);
    CHECK(big[0] == "a:1,");
    CHECK(big[1] == std::string(50, 'x') + ",");
    CHECK(big[2] == "b:2,");
}

TEST_CASE("split is const-correct and correct")
{
    const std::string in = "a,b,,c";
    auto parts = split(in, ",");
    CHECK(in == "a,b,,c");
    CHECK(parts == std::vector<std::string>{"a", "b", "", "c"});
    CHECK(split("abc", ",") == std::vector<std::string>{"abc"});
    CHECK(split("a=b=c", "=") == std::vector<std::string>{"a", "b", "c"});
    CHECK(split("a::b", "::") == std::vector<std::string>{"a", "b"});
}

TEST_CASE("buildWritePayload format")
{
    CHECK(Client::buildWritePayload({}, {}) == "");
    CHECK(Client::buildWritePayload({{"b1", true}, {"b2", false}}, {}) == "b1:true,b2:false");
    CHECK(Client::buildWritePayload({}, {{"a1", 999}}) == "a1:999.000000");
    CHECK(Client::buildWritePayload({{"b1", true}}, {{"a1", 1.5}, {"a2", -2}}) ==
          "b1:true,a1:1.500000,a2:-2.000000");
}

TEST_CASE("split without delimiter yields one element, so .at(1) throws")
{
    auto parts = split("QUERY", "=");
    REQUIRE(parts.size() == 1);
    CHECK_THROWS_AS(parts.at(1), std::out_of_range);
}
