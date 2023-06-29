#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

#include <boost/program_options.hpp>

#include "bennu/distributed/Provider.hpp"
#include "bennu/distributed/Utils.hpp"

namespace po = boost::program_options;

using namespace bennu::distributed;

class ElectricPowerService : public Provider
{
public:
    ElectricPowerService(const Endpoint& serverEndpoint, const Endpoint& publishEndpoint, bool debug) :
        Provider(serverEndpoint, publishEndpoint),
        mLock(),
        mDebug(debug)
    {
        mPS["bus-1.active"] = "true";
        mPS["bus-1.voltage"] = "0.93";
        mPS["bus-1.gen_mw"] = "10.2";
        mPS["bus-1.number"] = "1";

        mPS["bus-2.active"] = "true";
        mPS["bus-2.voltage"] = "-1.45";
        mPS["bus-2.gen_mw"] = "10.2";
        mPS["bus-2.number"] = "2";

        mPS["bus-3.active"] = "true";
        mPS["bus-3.voltage"] = "1.45";
        mPS["bus-3.gen_mw"] = "-100.2";
        mPS["bus-3.number"] = "3";

        mPS["branch-1-2_1.active"] = "true";
        mPS["branch-1-2_1.source"] = "1";
        mPS["branch-1-2_1.target"] = "2";
        mPS["branch-1-2_1.current"] = "20.0";

        mPS["branch-1-3_1.active"] = "true";
        mPS["branch-1-3_1.source"] = "1";
        mPS["branch-1-3_1.target"] = "3";

        mPS["branch-3-2_1.active"] = "true";
        mPS["branch-3-2_1.source"] = "3";
        mPS["branch-3-2_1.target"] = "2";

        mPS["load-1_bus-1.active"] = "true";
        mPS["load-1_bus-1.mw"] = "400.55";
        mPS["load-1_bus-1.mvar"] = "-90.34";
        mPS["load-1_bus-1.bid"] = "5.6";

        mPS["load-1_bus-2.active"] = "true";
        mPS["load-1_bus-2.mw"] = "10.0";
        mPS["load-1_bus-2.mvar"] = "1.22";

        mPS["load-1_bus-3.active"] = "false";
        mPS["load-1_bus-3.mw"] = "10.0";
        mPS["load-1_bus-3.mvar"] = "4.3";

        mPS["generator-1_bus-1.active"] = "true";
        mPS["generator-1_bus-1.mw"] = "10.0";
    }

    // Must return "ACK=tag1,tag2,..." or "ERR=<error message>"
    std::string query()
    {
        if (mDebug) { std::cout << "ElectricPowerService::query ---- received query request" << std::endl; }
        std::string result = "ACK=";
        for (const auto& kv : mPS)
        {
            result += kv.first + ",";
        }
        return result;
    }

    // Must return "ACK=<value>" or "ERR=<error message>"
    std::string read(const std::string& tag)
    {
        if (mDebug) { std::cout << "ElectricPowerService::read ---- received read for tag: " << tag << std::endl; }
        std::scoped_lock<std::shared_mutex> lock(mLock);
        std::string result;
        if (mPS.count(tag))
        {
            result += "ACK=";
            result += mPS[tag];
        }
        else
        {
            result += "ERR=Tag not found";
        }
        return result;
    }

    // Must return "ACK=<success message>" or "ERR=<error message>"
    std::string write(const std::unordered_map<std::string,std::string>& tags)
    {
        std::string result;

        for (auto &it : tags)
        {
            std::string tag{it.first};
            std::string value{it.second};

            if (mDebug) { std::cout << "ElectricPowerService::write ---- received write for tag: " << tag << " -- " << value << std::endl; }
            std::scoped_lock<std::shared_mutex> lock(mLock);
            if (mPS.count(tag))
            {
                mPS[tag] = value;
            }
            else
            {
                result += "ERR=Tag not found";
            }
        }

        if (result.empty())
        {
            result += "ACK=Updated tags in Electric Power";
        }

        return result;
    }

    [[ noreturn ]] void periodicPublish()
    {
        while (1)
        {
            publishData();
            std::this_thread::sleep_for(std::chrono::microseconds(1000000)); // =1 second
        }
    }

    void publishData()
    {
        std::scoped_lock<std::shared_mutex> lock(mLock);
        std::string message;
        for (const auto& kv : mPS)
        {
            message += kv.first + ":" + kv.second + ",";
        }
        publish(message);
    }

private:
    std::unordered_map<std::string, std::string> mPS;
    std::shared_mutex mLock;
    bool mDebug;
};

int main(int argc, char** argv)
{
    std::string program = "Electric power test worker service";
    po::options_description desc(program);
    desc.add_options()
        ("help",  "show this help menu")
        ("debug", po::value<bool>()->default_value(false), "print debugging information")
        ("server-endpoint", po::value<std::string>()->default_value("tcp://127.0.0.1:5555"), "server listening endpoint")
        ("publish-endpoint", po::value<std::string>()->default_value("udp://239.0.0.1:40000"), "publishing endpoint");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help"))
    {
        std::cout << desc << std::endl;
        return 0;
    }

    Endpoint sEndpoint, pEndpoint;
    sEndpoint.str = vm["server-endpoint"].as<std::string>();
    pEndpoint.str = vm["publish-endpoint"].as<std::string>();
    ElectricPowerService ep(sEndpoint, pEndpoint, vm["debug"].as<bool>());
    ep.run();
    return 0;
}
