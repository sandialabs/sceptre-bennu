#ifndef BENNU_EXECUTABLES_HELICS_HELPER_HPP
#define BENNU_EXECUTABLES_HELICS_HELPER_HPP

#include <csignal> // signal handler
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include <helics/helics.hpp>
#include "Logic.hpp"

#include <deps/json.hpp>

namespace bennu {
namespace executables {

std::sig_atomic_t exitRequested{0};

void exit_handler(int /*signal*/)
{
    exitRequested = 1;
}

class HelicsFederate
{
public:
    HelicsFederate(std::string& config)
    {
        std::signal(SIGINT, exit_handler); // ctrl + c

        // Initialize logic parser
        logic::initParser();

        // Default max simulation time is 1 week (in seconds)
        auto hours = 24*7; // one week
        auto total_interval = int(60 * 60 * hours);

        // Get federate config values at runtime
        std::ifstream input(config);
        auto cfg = nlohmann::json::parse(input);
        std::cout << "Parsed the json file: " << config << std::endl;
        auto endTime = cfg.value("end_time", "");
        mEndTime = endTime == "" ?
            helics::loadTimeFromString(std::to_string(total_interval)) :
            helics::loadTimeFromString(endTime);
        auto reqTime = cfg.value("request_time", "");
        mRequestTime = helics::loadTimeFromString("1");
        if (reqTime != "")
        {
            mRequestTime = reqTime == "max" ?
                helics::Time::maxVal() :
                helics::loadTimeFromString(reqTime);
        }

        // ------------- Registering federate from json -------------
        pFed = std::make_unique<helics::CombinationFederate>(config);
        auto name = pFed->getName();
        std::cout << "Created Federate: " << name << std::endl;
        mEndCount = pFed->getEndpointCount();
        std::cout << "\tNumber of endpoints: " << std::to_string(mEndCount) << std::endl;
        mSubCount = pFed->getInputCount();
        std::cout << "\tNumber of subscriptions: " << std::to_string(mSubCount) << std::endl;
        mPubCount = pFed->getPublicationCount();
        std::cout << "\tNumber of publications: " << std::to_string(mPubCount) << std::endl;

        // Diagnostics to confirm JSON config correctly added the required
        //   endpoints, publications, and subscriptions
        for (int i=0; i<mEndCount; i++)
        {
            mEndId[i] = pFed->getEndpoint(i);
            auto endName = mEndId[i].getName();
            auto endType = mEndId[i].getType();
            std::cout << "\tRegistered endpoint ---> " + endName << std::endl;
            mTypes[endName] = endType;
        }
        for (int i=0; i<mSubCount; i++)
        {
            mSubId[i] = pFed->getInput(i);
            auto subName = mSubId[i].getTarget();
            auto subType = mSubId[i].getType();
            auto subInfo = mSubId[i].getInfo(); // stores logic for interdependencies
            std::cout << "\tRegistered subscription ---> " + subName << std::endl;
            auto found = subName.find('/');
            if (found != std::string::npos)
            {
                subName = subName.substr(found+1, subName.length());
            }
            mTags.insert(subName);
            mTypes[subName] = subType;
            if (subInfo != "")
            {
                std::cout << "\t\t********** LOGIC **********" << std::endl;
                mLogicVars[subName] = subType == "bool" ? "false" : "0";
                auto exps = logic::splitStr(subInfo, ";");
                for (auto& exp : exps)
                {
                    logic::trim(exp);
                    auto exprParts = logic::splitExpression(exp, "=");
                    std::string tag = exprParts[0];
                    std::string logic = exprParts[1];
                    logic::trim(tag);
                    logic::trim(logic);
                    mLogic[tag] = logic;
                    std::cout << "\t\t" << exp << std::endl;
                }
            }
        }
        for (int i=0; i<mPubCount; i++)
        {
            mPubId[i] = pFed->getPublication(i);
            auto pubName = mPubId[i].getName();
            auto pubType = mPubId[i].getType();
            std::cout << "\tRegistered publication ---> " + pubName << std::endl;
            auto found = pubName.find('/');
            if (found != std::string::npos)
            {
                pubName = pubName.substr(found+1, pubName.length());
            }
            mTags.insert(pubName);
            mTypes[pubName] = pubType;
        }
    }

    void run()
    {
        // ----------------- Entering Execution Mode -----------------
        pFed->enterInitializingMode();
        pFed->enterExecutingMode();
        std::cout << "Entered HELICS execution mode" << std::endl;

        // Blocking call for a time request at simulation time 0
        auto time = helics::timeZero;
        std::cout << "Requesting initial time " << time << std::endl;
        auto grantedTime = pFed->requestTime(time);
        std::cout << "Granted time " << std::to_string(grantedTime) << std::endl;

        // Publish initial values to helics
        for (int i=0; i<mPubCount; i++)
        {
            auto pubName = mPubId[i].getName();
            auto found = pubName.find('/');
            if (found != std::string::npos)
            {
                pubName = pubName.substr(found+1, pubName.length());
            }
            auto value = tag(pubName);
            mPubId[i].publish(value);
            std::cout << "\tPublishing " << pubName <<
                ":" << value << " at time " <<
                std::to_string(grantedTime) << std::endl;
        }

        // ----------------- Main co-simulation loop -----------------
        // As long as granted time is in the time range to be simulated...
        while (grantedTime < mEndTime)
        {
            if (exitRequested)
            {
                std::cout << "SIGINT or CTRL-C detected. Exiting gracefully" << std::endl;
                break;
            }

            printState();

            // Time request for the next physical interval to be simulated
            auto requestedTime = grantedTime + mRequestTime;
            requestedTime = mRequestTime == helics::Time::maxVal() ? mRequestTime : requestedTime;
            std::cout << "Requesting time " << std::to_string(requestedTime) << std::endl;
            grantedTime = pFed->requestTime(requestedTime);
            std::cout << "Granted time " << std::to_string(grantedTime) << std::endl;

            // Process subscriptions
            for (int i=0; i<mSubCount; i++)
            {
                if (mSubId[i].isUpdated())
                {
                    auto subName = mSubId[i].getTarget();
                    auto found = subName.find('/');
                    if (found != std::string::npos)
                    {
                        subName = subName.substr(found+1, subName.length());
                    }
                    auto value = mSubId[i].getValue<std::string>();
                    tag(subName, value);
                    std::cout << "\tUpdated " << subName <<
                        ":" << value << " at time " <<
                        std::to_string(grantedTime) << std::endl;
                }
            }

            // Process logic
            std::vector<std::string> tagsToVector{mTags.begin(), mTags.end()};
            auto tags = logic::sortByLargest(tagsToVector);
            TokenMap vars;
            for (const auto& [tag_name, logic] : mLogic)
            {
                auto data = logic;
                for (auto const& t : tags)
                {
                    logic::replaceAll(data, t, tag(t));
                }
                try {
                    auto result = calculator::calculate(data.data(), &vars).str();
                    logic::lower(result);
                    if (result != tag(tag_name))
                    {
                        std::cout << "\tLOGIC: " + tag_name << " = " << logic << " ----> " << result << std::endl;
                        tag(tag_name, result);
                    }
                } catch (std::exception& e) {
                    std::cout << "ERROR: [ " << logic << " ] Failed to parse logic: " << e.what() << std::endl;
                    continue;
                }
            }

            // Process endpoints
            for (int i=0; i<mEndCount; i++)
            {
                while (mEndId[i].hasMessage())
                {
                    auto endName = mEndId[i].getName();
                    auto found = endName.find('/');
                    if (found != std::string::npos)
                    {
                        endName = endName.substr(found+1, endName.length());
                    }
                    auto msg = mEndId[i].getMessage();
                    auto value = msg->to_string();
                    std::cout << "\tReceived message from endpoint " <<
                        msg->source << " at time " <<
                        std::to_string(grantedTime) << " with data " <<
                        value << std::endl;
                    tag(endName, value);
                    std::cout << "\tUpdated " << endName <<
                        ":" << value << " at time " <<
                        std::to_string(grantedTime) << std::endl;
                }
            }

            // Process publications
            for (int i=0; i<mPubCount; i++)
            {
                auto pubName = mPubId[i].getName();
                auto found = pubName.find('/');
                if (found != std::string::npos)
                {
                    pubName = pubName.substr(found+1, pubName.length());
                }
                auto value = tag(pubName);
                mPubId[i].publish(value);
                std::cout << "\tPublishing " << pubName <<
                    ":" << value << " at time " <<
                    std::to_string(grantedTime) << std::endl;
            }
        }

        // Clean up HELICS
        pFed->finalize();
    }

    void printState()
    {
        std::cout << "=================== DATA ===================" << std::endl;
        for (auto& t : mTags)
        {
            std::cout << std::left << std::setw(30) << t << " --- "
                      << tag(t) << std::endl;
        }
        std::cout << "============================================" << std::endl;
    }

    std::string getType(const std::string& tag)
    {
        return mTypes[tag];
    }

    virtual void tag(const std::string& tag, const std::string& strval) = 0;

    virtual std::string tag(const std::string& tag) = 0;

    std::unique_ptr<helics::CombinationFederate> pFed;

    std::map<std::string, std::string> mLogic;

    TokenMap mLogicVars;

private:
    std::unordered_set<std::string> mTags;
    std::map<std::string, std::string> mTypes;
    int mEndCount, mSubCount, mPubCount;
    helics::Time mEndTime, mRequestTime;
    std::map<int, helics::Endpoint> mEndId;
    std::map<int, helics::Input> mSubId;
    std::map<int, helics::Publication> mPubId;

};

} // namespace field_device
} // namespace bennu

#endif // BENNU_EXECUTABLES_HELICS_HELPER_HPP
