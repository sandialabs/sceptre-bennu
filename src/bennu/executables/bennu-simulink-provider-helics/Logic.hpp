//
// SUPPORTED LOGIC OPERATORS/FUNCTIONS:
//
// Unary operators. +, -, !
// Binary operators. +, -, /, *, %, <<, >>, **
// Boolean operators. <, >, <=, >=, ==, !=, &&, ||
// Functions. sin, cos, tan, abs
//
#ifndef BENNU_EXECUTABLES_LOGIC_HPP
#define BENNU_EXECUTABLES_LOGIC_HPP

#include <iostream>
#include <map>
#include <numeric>
#include <string>
#include <vector>

#include <boost/algorithm/string.hpp>

#include "cparse/Cparse.hpp" // cparse_start(), calculator::calculate()

namespace bennu {
namespace executables {
namespace logic {

void initParser()
{
    cparse_startup();
}

// In-place trim
void trim(std::string& str)
{
    boost::algorithm::trim(str);
}

// In-place lowercase
void lower(std::string& str)
{
    boost::algorithm::to_lower(str);
}

void replaceAll(std::string& data, std::string toSearch, std::string replaceStr)
{
    // Example data (i.e. logic):
    //    "bus-1.gen_mw > 5"
    // Example data (after being replaced):
    //    "400.55 > 5"
    trim(data);
    // Get the first occurrence
    size_t pos = data.find(toSearch);
    // Repeat till end is reached
    while (pos != std::string::npos)
    {
        // Replace this occurrence. Check pos+1 to make
        // sure this isn't a substring of a different variable.
        //   e.g. 'var_O1' vs 'var_O12'
        if (!std::isalnum(data[pos+toSearch.size()]))
        {
            data.replace(pos, toSearch.size(), replaceStr);
        }
        // Get the next occurrence from the current position
        pos = data.find(toSearch, pos + toSearch.size());
    }
}

std::vector<std::string> splitExpression(std::string phrase, std::string delimiter)
{
    std::vector<std::string> list;
    size_t pos = 0;
    std::string lhs;
    pos = phrase.find(delimiter);
    if (pos != std::string::npos)
    {
        lhs = phrase.substr(0, pos);
        list.push_back(lhs);
        phrase.erase(0, pos + delimiter.length());
        list.push_back(phrase); // rhs
    }
    return list;
}

std::vector<std::string> splitStr(std::string phrase, std::string delimiter)
{
    std::vector<std::string> list;
    size_t pos = 0;
    std::string token;
    while ((pos = phrase.find(delimiter)) != std::string::npos)
    {
        token = phrase.substr(0, pos);
        list.push_back(token);
        phrase.erase(0, pos + delimiter.length());
    }
    list.push_back(phrase);
    return list;
}

// Use lambda to sort string vector by largest length first. Helps to solve the substring problem
std::vector<std::string> sortByLargest(std::vector<std::string> vector)
{
    // e.g. logic = 'foo bar foobar baz', foo = '666'
    //      tags = ["foo", "foobar"]
    //          BAD: 'foo bar foobar baz' --> '666 bar 666bar baz'
    //      sortByLargest(tags) = ["foobar", "foo"]
    //          GOOD: 'foo bar foobar baz' --> '666 bar foobar baz'
    std::sort(vector.begin(), vector.end(), [](std::string a, std::string b) {
        return a.size() > b.size();
    });
    return vector;
}

} // namespace logic
} // namespace executables
} // bennu

#endif // BENNU_EXECUTABLES_LOGIC_HPP
