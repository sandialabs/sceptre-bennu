#include "doctest.h"

#include <cmath>
#include <string>
#include <vector>

#include "Logic.hpp"

using namespace bennu::executables::logic;

TEST_CASE("helics logic: trim and lower")
{
    // trim
    std::string strTrim = "   \t\n  hello bennu  \r\n\t  ";
    trim(strTrim);
    CHECK(strTrim == "hello bennu");

    std::string strEmpty = "   ";
    trim(strEmpty);
    CHECK(strEmpty.empty());

    // lower
    std::string strLower = "HeLLo WoRLD 123";
    lower(strLower);
    CHECK(strLower == "hello world 123");
}

TEST_CASE("helics logic: replaceAll word-boundary safety")
{
    // Ensure var_01 and var_02 are not replaced when searching var_0
    std::string expr = "var_01 > 5 && var_0 > 2 && var_02 < 10";
    replaceAll(expr, "var_0", "99");
    CHECK(expr == "var_01 > 5 && 99 > 2 && var_02 < 10");

    // Multiple occurrences of target variable
    std::string expr2 = "bus_1 + bus_1 * 2";
    replaceAll(expr2, "bus_1", "10");
    CHECK(expr2 == "10 + 10 * 2");

    // Variable at the very end of expression
    std::string expr3 = "flag == active";
    replaceAll(expr3, "active", "true");
    CHECK(expr3 == "flag == true");

    // Substring not found leaves string unchanged
    std::string expr4 = "bus_1 > 10";
    replaceAll(expr4, "bus_2", "20");
    CHECK(expr4 == "bus_1 > 10");
}

TEST_CASE("helics logic: splitExpression, splitStr, sortByLargest")
{
    // splitExpression
    auto parts = splitExpression("output = input * 2.5", "=");
    REQUIRE(parts.size() == 2);
    CHECK(parts[0] == "output ");
    CHECK(parts[1] == " input * 2.5");

    auto noSplit = splitExpression("no_delimiter_here", "=");
    CHECK(noSplit.empty());

    // splitStr
    auto tokens = splitStr("voltage,current,power,frequency", ",");
    REQUIRE(tokens.size() == 4);
    CHECK(tokens[0] == "voltage");
    CHECK(tokens[1] == "current");
    CHECK(tokens[2] == "power");
    CHECK(tokens[3] == "frequency");

    // sortByLargest (descending length)
    std::vector<std::string> tags = {"v", "voltage_substation_1", "volts", "volt"};
    auto sorted = sortByLargest(tags);
    REQUIRE(sorted.size() == 4);
    CHECK(sorted[0] == "voltage_substation_1");
    CHECK(sorted[1] == "volts");
    CHECK(sorted[2] == "volt");
    CHECK(sorted[3] == "v");
}

TEST_CASE("helics logic: cparse arithmetic, comparisons, boolean, and math functions")
{
    initParser();

    // Arithmetic
    CHECK(calculator::calculate("2 + 3 * 4").asInt() == 14);
    CHECK(calculator::calculate("(10 - 4) / 2").asInt() == 3);
    CHECK(calculator::calculate("2 ** 3").asInt() == 8);
    CHECK(calculator::calculate("17 % 5").asInt() == 2);
    CHECK(doctest::Approx(calculator::calculate("1.5 * 4.0").asDouble()) == 6.0);

    // Comparisons
    CHECK(calculator::calculate("5 < 10").asBool() == true);
    CHECK(calculator::calculate("10 <= 10").asBool() == true);
    CHECK(calculator::calculate("15 > 20").asBool() == false);
    CHECK(calculator::calculate("20 >= 20").asBool() == true);
    CHECK(calculator::calculate("42 == 42").asBool() == true);
    CHECK(calculator::calculate("42 != 43").asBool() == true);

    // Boolean operations (cparse supports Python-style True/False literals and numeric 1/0)
    CHECK(calculator::calculate("True && False").asBool() == false);
    CHECK(calculator::calculate("True && True").asBool() == true);
    CHECK(calculator::calculate("True || False").asBool() == true);
    CHECK(calculator::calculate("False || False").asBool() == false);
    CHECK(calculator::calculate("!False").asBool() == true);
    CHECK(calculator::calculate("!True").asBool() == false);
    CHECK(calculator::calculate("1 && 0").asBool() == false);
    CHECK(calculator::calculate("1 || 0").asBool() == true);

    // Math functions: sin, cos, abs
    CHECK(calculator::calculate("abs(-42)").asInt() == 42);
    CHECK(doctest::Approx(calculator::calculate("abs(-3.14)").asDouble()) == 3.14);
    CHECK(doctest::Approx(calculator::calculate("sin(0)").asDouble()) == 0.0);
    CHECK(doctest::Approx(calculator::calculate("cos(0)").asDouble()) == 1.0);

    // Syntax error handling throws cparse_error or std::exception
    CHECK_THROWS(calculator::calculate("2 + * 3"));
    CHECK_THROWS(calculator::calculate("(10 + 5"));
}
