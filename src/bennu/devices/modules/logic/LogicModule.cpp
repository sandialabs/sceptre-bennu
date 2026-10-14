//
// SUPPORTED LOGIC OPERATORS/FUNCTIONS:
//
// Unary operators. +, -, !
// Binary operators. +, -, /, *, %, <<, >>, **
// Boolean operators. <, >, <=, >=, ==, !=, &&, ||
// Functions. sin, cos, tan, abs
//
// NOTE: A 'delay' tag on a logic line such as:
//       var_O1 = var_O0,delay:10
//   Is calculated as:
//       delay (ms) = # of cycles * scan cycle time (ms)
//
//   e.g. if the cycle time is 1000ms, and the delay is 10,
//   the total delay before that tag is checked for change
//   and updated will be 10 seconds
//

#include "LogicModule.hpp"

#include <iostream>
#include <numeric>
#include <sstream>
#include <string>

#include <boost/algorithm/string.hpp>

#include "bennu/devices/field-device/DataManager.hpp"
#include "bennu/utility/Log.hpp"
#include "bennu/devices/modules/logic/cparse/Cparse.hpp" // cparse_start(), calculator::calculate()

namespace bennu {
namespace logic {

LogicModule::LogicModule()
{
    // Initialize cparse
    cparse_startup();
}

bool LogicModule::handleTreeData(const boost::property_tree::ptree& tree)
{
    bool success = true;
    try
    {
        mLogic = tree.get<std::string>("logic", "");
        boost::algorithm::trim(mLogic);
    }
    catch (boost::property_tree::ptree_bad_path& e)
    {
        std::cerr << "ERROR: Format was incorrect in the Logic module XML: " << e.what() << std::endl;
        success = false;
    }
    catch (boost::property_tree::ptree_error& e)
    {
        std::cerr << "ERROR: There was a problem parsing the Logic module XML: " << e.what() << std::endl;
        success = false;
    }
    return success;
}

// Sort tags (longest first) and split/trim the logic into non-blank lines once; per line, remember
// which tags can occur in it so a cycle only touches those.
void LogicModule::mPrepare()
{
    for (auto& tag : mSortByLargest(mDataManager->getBinaryTags()))
    {
        mTags.push_back({tag, true});
    }
    for (auto& tag : mSortByLargest(mDataManager->getAnalogTags()))
    {
        mTags.push_back({tag, false});
    }

    std::stringstream ss(mLogic);
    std::string line;
    while (std::getline(ss, line, '\n'))
    {
        boost::algorithm::trim(line);
        if (line.empty())
        {
            continue;
        } // scanLogic ignores blank lines anyway
        Line l{line, {}};
        for (size_t i = 0; i < mTags.size(); i++)
        {
            if (line.find(mTags[i].name) != std::string::npos)
            {
                l.tags.push_back(i);
            }
        }
        mLines.push_back(std::move(l));
    }
    mPrepared = true;
}

void LogicModule::scanInputs()
{
    if (!mPrepared)
    {
        mPrepare();
    }

    // Replace strings in RHS logic with corresponding boolean or analog data
    std::vector<std::string> values(mTags.size());
    std::vector<bool> fetched(mTags.size(), false);
    mCurrentLogic.clear();
    for (auto& l : mLines)
    {
        std::string line = l.text;
        for (size_t i : l.tags)
        {
            if (!fetched[i])
            {
                values[i] = mTags[i].binary
                                ? (mDataManager->getDataByTag<bool>(mTags[i].name) ? "True" : "False")
                                : std::to_string(mDataManager->getDataByTag<double>(mTags[i].name));
                fetched[i] = true;
            }
            mReplaceAllRhs(line, mTags[i].name, values[i]);
        }
        mCurrentLogic += line + "\n";
    }
}

void LogicModule::scanLogic(unsigned int cycleTime)
{
    TokenMap vars;
    std::stringstream ss(mCurrentLogic);
    std::string line;
    std::vector<std::string> exprParts;
    std::string logic;
    while (std::getline(ss, line, '\n'))
    {
        if (line == "")
        {
            continue;
        } // ignore blank lines

        exprParts = mSplitExpression(line, "="); // e.g. line = 'foo = (((bar - baz) >= 90) || ((bar - baz) <= -90)),delay:5000'
        std::string lhs = exprParts[0];
        std::string rhs = exprParts[1];
        boost::algorithm::trim(lhs); // e.g. lhs = 'foo'
        boost::algorithm::trim(rhs); // e.g. rhs = '(((bar - baz) >= 90) || ((bar - baz) <= -90)),delay:5000'

        auto rhsParts = mSplitStr(rhs, ",");
        std::string logic = rhsParts[0]; // e.g. rhsParts[0] = '(((bar - baz) >= 90) || ((bar - baz) <= -90))'
        // check if there is a 'delay' in the right hand side of the expression
        int delay = mGetDelay(rhsParts);

        // skip tags that are currently being delayed
        if (isDelayedTag(lhs))
        {
            int newDelay = getDelayedTag(lhs) - static_cast<int>(cycleTime);
            if (newDelay > 0)
            {
                setDelayedTag(lhs, newDelay);
                BENNU_LOG_DEBUG("LOGIC (%d): %s = %s ----> [ DELAYED %dms ]\n", delay, lhs.c_str(), logic.c_str(), newDelay);
                continue;
            }
        }

        // evaluate logic expression
        try
        {
            if (mDataManager->isBinary(lhs))
            {
                bool result = calculator::calculate(logic.data(), &vars).asBool();
                BENNU_LOG_DEBUG("LOGIC (%d): %s = %s ----> %s\n", delay, lhs.c_str(), logic.c_str(), std::to_string(result).c_str());
                // if new evaluated data has changed from the datastore, update the tag
                if (result != mDataManager->getDataByTag<bool>(lhs) && !mDataManager->isUpdatedBinaryTag(lhs))
                {
                    if (delay > 0 && !isDelayedTag(lhs))
                    {
                        delay = delay * static_cast<int>(cycleTime); // delay (ms) = # of cycles * scan cycle time (ms)
                        setDelayedTag(lhs, delay);
                        BENNU_LOG_DEBUG("\nI: Delaying tag: %s for %dms\n", lhs.c_str(), delay);
                    }
                    else
                    {
                        mDataManager->addUpdatedBinaryTag(lhs, result);
                        removeDelayedTag(lhs);
                    }
                }
                else if (isDelayedTag(lhs))
                {
                    removeDelayedTag(lhs);
                }
            }
            else if (mDataManager->isAnalog(lhs))
            {
                double result = calculator::calculate(logic.data(), &vars).asDouble();
                BENNU_LOG_DEBUG("LOGIC (%d): %s = %s ----> %s\n", delay, lhs.c_str(), logic.c_str(), std::to_string(result).c_str());
                // if new evaluated data has changed from the datastore, update the tag
                if (result != mDataManager->getDataByTag<double>(lhs) && !mDataManager->isUpdatedAnalogTag(lhs))
                {
                    if (delay > 0 && !isDelayedTag(lhs))
                    {
                        delay = delay * static_cast<int>(cycleTime); // delay (ms) = # of cycles * scan cycle time (ms)
                        setDelayedTag(lhs, delay);
                        BENNU_LOG_DEBUG("\nI: Delaying tag: %s for %dms\n", lhs.c_str(), delay);
                    }
                    else
                    {
                        mDataManager->addUpdatedAnalogTag(lhs, result);
                        removeDelayedTag(lhs);
                    }
                }
                else if (isDelayedTag(lhs))
                {
                    removeDelayedTag(lhs);
                }
            }
        }
        catch (std::exception& e)
        {
            std::cout << "ERROR: [ " << logic << " ] Failed to parse logic: " << e.what() << std::endl;
            continue;
        }
    }
}

int LogicModule::mGetDelay(std::vector<std::string> rhsParts)
{
    int delay = 0;
    if (rhsParts.size() == 2) // a size of 2 means there should be a ',delay:X' in the logic
    {
        std::string delayStr = rhsParts[1]; // e.g. rhsParts[1] = 'delay:5'
        auto delayParts = mSplitStr(delayStr, ":");
        if (delayParts.size() == 2 && delayParts[0] == "delay")
        {
            try
            {
                int val = std::stoi(delayParts[1]);
                delay = (val >= 0) ? val : 0; // cannot have negative delay
            }
            catch (std::invalid_argument& e)
            {
                std::cout << "ERROR: Invalid delay in logic. Setting delay to 0: " << e.what() << std::endl;
            }
        }
        else
        {
            std::cout << "WARN: Delay logic error. Setting delay to 0." << std::endl;
        }
    }
    else if (rhsParts.size() > 2)
    {
        std::cout << "ERROR: Check logic...too many pieces detected." << std::endl;
    }
    return delay;
}

void LogicModule::mReplaceAllRhs(std::string& line, const std::string& toSearch, const std::string& replaceStr)
{
    // Example line (i.e. logic):
    //    "load-breaker-toggle = ! load-power > 500 ;"
    // Example line (after being replaced):
    //    "load-breaker-toggle = ! 400.55 > 500 ;"
    size_t pos = line.find(toSearch);
    while (pos != std::string::npos)
    {
        // Replace this occurrence, only if it is RHS of expression. Also check pos+1 to make
        // sure this isn't a substring of a different variable.
        //   e.g. 'var_O1' vs 'var_O12'
        if (pos != 0 && !std::isalnum(line[pos + toSearch.size()]))
        {
            line.replace(pos, toSearch.size(), replaceStr);
        }
        // Get the next occurrence from the current position
        pos = line.find(toSearch, pos + toSearch.size());
    }
}

std::vector<std::string> LogicModule::mSplitExpression(std::string phrase, std::string delimiter)
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

std::vector<std::string> LogicModule::mSplitStr(std::string phrase, std::string delimiter)
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
std::vector<std::string> LogicModule::mSortByLargest(std::vector<std::string> vector)
{
    // e.g. logic = 'foo bar foobar baz', foo = '666'
    //      tags = ["foo", "foobar"]
    //          BAD: 'foo bar foobar baz' --> '666 bar 666bar baz'
    //      mSortByLargest(tags) = ["foobar", "foo"]
    //          GOOD: 'foo bar foobar baz' --> '666 bar foobar baz'
    std::sort(vector.begin(), vector.end(), [](std::string a, std::string b) {
        return a.size() > b.size();
    });
    return vector;
}

} // namespace logic
} // namespace bennu
