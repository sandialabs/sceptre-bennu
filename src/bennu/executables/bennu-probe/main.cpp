#include <functional>
#include <iostream>
#include <sstream>

#include <boost/program_options.hpp>

#include "bennu/distributed/Client.hpp"
#include "bennu/distributed/Utils.hpp"

namespace po = boost::program_options;

class Probe : public bennu::distributed::Client
{
public:
    Probe(const bennu::distributed::Endpoint& endpoint) :
        bennu::distributed::Client(endpoint)
    {
        setHandler(std::bind(&Probe::handler, this, std::placeholders::_1));
    }

    void handler(const std::string& reply)
    {
        auto response = reply;
        std::string delim{","}, status, msg;
        try
        {
            auto split = bennu::distributed::split(response, delim);
            std::cout << "Reply:" << std::endl;
            if (split.size() > 1)
            {
                for (auto& tag : split)
                {
                    std::cout << "\t" << tag << std::endl;
                }
            }
            else
            {
                std::cout << "\t" << split[0] << std::endl;
            }
        } catch (std::exception& e) {
            printf("Err: %s", e.what());
        }
    }
};

int main(int argc, char** argv)
{
    std::string program = "Command line probe for querying/reading/writing values to/from a bennu FEP or provider";
    po::options_description desc(program);
    desc.add_options()
        ("help", "show this help menu")
        ("endpoint", po::value<std::string>()->default_value("tcp://127.0.0.1:1330"), "FEP (:1330) or Provider (:5555) endpoint")
        ("command", po::value<std::string>(), "Command: query|read|write")
        ("tag", po::value<std::string>(), "Full name of the tag, e.g. bus1.active")
        ("value", po::value<float>(), "Value for a analog write")
        ("status", po::value<bool>(), "Status for a boolean write");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);

    po::positional_options_description p;
    p.add("command", 1);
    po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
    po::notify(vm);

    if (vm.count("help"))
    {
        std::cout << desc << std::endl;
        return 0;
    }

    std::string endpoint = "";
    if (vm.count("endpoint"))
    {
        endpoint = vm["endpoint"].as<std::string>();
    }

    std::string command;
    if (vm.count("command"))
    {
        command = vm["command"].as<std::string>();
    }
    else
    {
        std::cout << "Error: you must define a command: query, read, or write." << std::endl;
        return -1;
    }

    std::string tag;
    if (vm.count("tag") && (command == "read" || command == "write"))
    {
        tag = vm["tag"].as<std::string>();
    }
    else if (command != "query")
    {
        std::cout << "Error: you must define a tag for the read/write command." << std::endl;
        return -1;
    }

    bennu::distributed::Endpoint ep;
    ep.str = endpoint;
    Probe probe(ep);

    std::ostringstream ss;
    ss << command << "=";

    if (command == "query")
    {
        if (vm.count("tag") || vm.count("value") || vm.count("status"))
        {
            std::cout << "You cannot specify a tag, or set a value or a status for a query command." << std::endl;
            return -1;
        }
    }
    else if (command == "read")
    {
        if (vm.count("value") || vm.count("status"))
        {
            std::cout << "You cannot set a value or a status for a read command." << std::endl;
            return -1;
        }
        ss << tag;
    }
    else if (command == "write")
    {
        if (vm.count("value") && vm.count("status"))
        {
            std::cout << "You cannot set a value and a status. Use one of the other depending tag type" << std::endl;
            return -1;
        }
        if (!vm.count("value") && !vm.count("status"))
        {
            std::cout << "For a \"write\" command, you must set a value or status depending on type of write." << std::endl;
            return -1;
        }

        ss << tag << ":";

        if (vm.count("status"))
        {
            auto status = vm["status"].as<bool>();
            std::string str = status ? "true" : "false";
            ss << str;
        }
        else
        {
            auto value = vm["value"].as<float>();
            ss << value;
        }
    }
    else
    {
        std::cout << "ERROR: command needs to be query, read, or write!" << std::endl;
        return -1;
    }

    fflush(stdout);
    std::string msg{ss.str()};
    probe.send(msg);
    return 0;
}
