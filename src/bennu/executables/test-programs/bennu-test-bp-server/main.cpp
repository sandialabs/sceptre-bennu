#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

#include <boost/program_options.hpp>

#include "bennu/distributed/Provider.hpp"
#include "bennu/distributed/Utils.hpp"

namespace po = boost::program_options;

using namespace bennu::distributed;

class BatchProcessService : public Provider
{
public:
    BatchProcessService(const Endpoint& serverEndpoint, const Endpoint& publishEndpoint, bool debug = false) :
        Provider(serverEndpoint, publishEndpoint),
        mLock(),
        mDebug(debug)
    {
        mBP["tank-1.temperature"] = "98.6";
        mBP["tank-1.fill_valve_switch"] = "false";
        mBP["tank-1.tank_level"] = "10";
        mBP["tank-1.emergency_shutoff"] = "false";
        mBP["tank-1.powered"] = "true";

        mBP["tank-2.mix_percent"] = "20";
        mBP["tank-2.fill_valve_switch"] = "false";
        mBP["tank-2.tank_level"] = "5";
        mBP["tank-2.emergency_shutoff"] = "false";
        mBP["tank-2.powered"] = "true";

        mBP["tank-3.temperature"] = "225";
        mBP["tank-3.fill_valve_switch"] = "false";
        mBP["tank-3.tank_level"] = "20";
        mBP["tank-3.emergency_shutoff"] = "false";
        mBP["tank-3.powered"] = "true";
    }

    // Must return "ACK=tag1,tag2,..." or "ERR=<error message>"
    std::string query()
    {
        if (mDebug) { std::cout << "BatchProcessService::query ---- received query request" << std::endl; }
        std::string result = "ACK=";
        for (const auto& kv : mBP)
        {
            result += kv.first + ",";
        }
        return result;
    }

    // Must return "ACK=<value>" or "ERR=<error message>"
    std::string read(const std::string& tag)
    {
        if (mDebug) { std::cout << "BatchProcessService::read ---- received read for tag: " << tag << std::endl; }
        std::scoped_lock<std::shared_mutex> lock(mLock);
        std::string result;
        if (mBP.count(tag))
        {
            result += "ACK=";
            result += mBP[tag];
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

            if (mDebug) { std::cout << "BatchProcessService::write ---- received write for tag: " << tag << " -- " << value << std::endl; }
            std::scoped_lock<std::shared_mutex> lock(mLock);
            std::string val;
            if (mBP.count(tag))
            {
                if (mBP[tag] == "true" || mBP[tag] == "false")
                {
                    val = value == "1" ? "true" : "false";
                }
                else
                {
                    val = value;
                }
                mBP[tag] = val;
            }
            else
            {
                result += "ERR=Tag not found";
            }
        }

        if (result.empty())
        {
            result += "ACK=Updated tags in Batch Process";
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
        for (const auto& kv : mBP)
        {
            message += kv.first + ":" + kv.second + ",";
        }
        publish(message);
    }

private:
    std::unordered_map<std::string, std::string> mBP;
    std::shared_mutex mLock;
    bool mDebug;
};

int main(int argc, char** argv)
{
    std::string program = "Batch process test worker service";
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
    BatchProcessService bp(sEndpoint, pEndpoint, vm["debug"].as<bool>());
    bp.run();
    return 0;
}
