#include <csignal>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

#include <boost/program_options.hpp>

#include "bennu/parsers/Parser.hpp"

namespace po = boost::program_options;

using std::placeholders::_1;
using std::placeholders::_2;

static bool running = true;
static size_t gSignalCount = 0;

void signalHandler(int)
{
    if (gSignalCount > 2)
    {
        std::cerr << "\nThere was a problem exiting bennu-field-device cleanly. Terminate." << std::endl;
        raise(SIGKILL);
    }
    gSignalCount++;
    running = false;
}

int main(int argc, char** argv)
{
    po::variables_map vm;

    try
    {
        std::string program = "A simulation startup for bennu field devices.";
        po::options_description desc(program);
        desc.add_options()
            ("help", "show this help menu")
            ("file", po::value<std::string>(), "Configuration file to load");

        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.count("help"))
        {
            std::cout << desc << std::endl;
            return 0;
        }
    }
    catch(po::error& e)
    {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }

    std::signal(SIGINT, signalHandler);

    bennu::parsers::Parser::the()->registerTagForDynamicLibrary("field-device", "bennu-field-device-base");

    if (vm.count("file"))
    {
        if (!bennu::parsers::Parser::the()->load(vm["file"].as<std::string>()))
        {
            std::cerr << "ERROR: bennu-field-device failed when loading the field device configuration file!" << std::endl;
            exit(1);
        }
    }
    else
    {
        std::cerr << "ERROR: bennu-field-device must define a --file option for the field device configuration file!" << std::endl;
        exit(1);
    }

    while (running)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return 0;
}
