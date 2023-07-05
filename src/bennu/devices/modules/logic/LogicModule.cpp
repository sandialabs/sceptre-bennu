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

#include <boost/algorithm/string.hpp>

#include "bennu/devices/field-device/DataManager.hpp"
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
    try {
        mLogic = tree.get<std::string>("logic", "");
        boost::algorithm::trim(mLogic);
    } catch (boost::property_tree::ptree_bad_path& e) {
        std::cerr << "ERROR: Format was incorrect in the Logic module XML: " << e.what() << std::endl;
        success = false;
    } catch (boost::property_tree::ptree_error& e) {
        std::cerr << "ERROR: There was a problem parsing the Logic module XML: " << e.what() << std::endl;
        success = false;
    }
    return success;
}

void LogicModule::scanInputs()
{
    // Replace strings in RHS logic with corresponding boolean or analog data
    mCurrentLogic = mLogic;
    for (auto& tag : mSortByLargest(mDataManager->getBinaryTags()))
    {
        bool status = mDataManager->getDataByTag<bool>(tag);
        mReplaceAllRhs(mCurrentLogic, tag, status ? "True" : "False");
    }
    for (auto& tag : mSortByLargest(mDataManager->getAnalogTags()))
    {
        double value = mDataManager->getDataByTag<double>(tag);
        mReplaceAllRhs(mCurrentLogic, tag, std::to_string(value));
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
        if (line == "") { continue; } // ignore blank lines

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
                std::cout << "LOGIC (" << delay << "): " + lhs <<
                             " = " << logic << " ----> [ DELAYED " << newDelay << "ms ]" << std::endl;
                continue;
            }
        }

        // evaluate logic expression
        try {
            if (mDataManager->isBinary(lhs))
            {
                bool result = calculator::calculate(logic.data(), &vars).asBool();
                std::cout << "LOGIC (" << delay << "): " + lhs <<
                             " = " << logic << " ----> " << result << std::endl;
                // if new evaluated data has changed from the datastore, update the tag
                if (result != mDataManager->getDataByTag<bool>(lhs) && !mDataManager->isUpdatedBinaryTag(lhs))
                {
                    if (delay > 0 && !isDelayedTag(lhs))
                    {
                        delay = delay * static_cast<int>(cycleTime); // delay (ms) = # of cycles * scan cycle time (ms)
                        setDelayedTag(lhs, delay);
                        std::cout << "\nI: Delaying tag: " << lhs << " for " << delay << "ms" << std::endl;
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
                std::cout << "LOGIC (" << delay << "): " + lhs <<
                             " = " << logic << " ----> " << result << std::endl;
                // if new evaluated data has changed from the datastore, update the tag
                if (result != mDataManager->getDataByTag<double>(lhs) && !mDataManager->isUpdatedAnalogTag(lhs))
                {
                    if (delay > 0 && !isDelayedTag(lhs))
                    {
                        delay = delay * static_cast<int>(cycleTime); // delay (ms) = # of cycles * scan cycle time (ms)
                        setDelayedTag(lhs, delay);
                        std::cout << "\nI: Delaying tag: " << lhs << " for " << delay << "ms" << std::endl;
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
        } catch (std::exception& e) {
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
            try {
                int val = std::stoi(delayParts[1]);
                delay = (val >= 0) ? val : 0; // cannot have negative delay
            } catch (std::invalid_argument& e) {
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

void LogicModule::mReplaceAllRhs(std::string& data, std::string toSearch, std::string replaceStr)
{
    // Example data (i.e. logic):
    //    "load-breaker-toggle = ! load-power > 500 ;\n        gen-power = load-breaker-toggle ;"
    // Example data (after being replaced. note the extra '\n' at the end):
    //    "load-breaker-toggle = ! 400.55 > 500 ;\ngen-power = True ;\n"
    std::stringstream ss(data);
    std::string line;
    std::string replaced;
    while (std::getline(ss, line, '\n'))
    {
        boost::algorithm::trim(line);
        // Get the first occurrence
        size_t pos = line.find(toSearch);
        // Repeat till end is reached
        while(pos != std::string::npos) {
            // Replace this occurrence, only if it is RHS of expression. Also check pos+1 to make
            // sure this isn't a substring of a different variable.
            //   e.g. 'var_O1' vs 'var_O12'
            if (pos != 0 && !std::isalnum(line[pos+toSearch.size()]))
            {
                line.replace(pos, toSearch.size(), replaceStr);
            }
            // Get the next occurrence from the current position
            pos = line.find(toSearch, pos + toSearch.size());
        }
        replaced += line + "\n";
    }
    data = replaced;
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
