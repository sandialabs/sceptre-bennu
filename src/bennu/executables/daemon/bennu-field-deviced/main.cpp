#include <fcntl.h>
#include <syslog.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

#include <boost/program_options.hpp>

#include "bennu/devices/modules/shell/BrashServer.hpp"
#include "bennu/parsers/Parser.hpp"

namespace po = boost::program_options;

static bool running = true;
static std::string ss = "START_SHELL_0";

#define LOCKMODE (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)

void signalHandler(int)
{
    // try to stop running, but if you can't after a second, then SIGKILL is called.
    //std::cout << "signal received = " << signal << std::endl;
    running = false;
}

bool isAlreadyRunning(const std::string& instance)
{
    // If the LOCKFILE cannot be opened, then we're going to assume that this is the first time
    //   we've run the bennu-field-deviced on this machine.
    std::string lockfile("/var/run/" + instance + ".pid");
    std::ifstream is(lockfile.c_str());
    if (!is.is_open())
    {
        syslog(LOG_INFO, "Unable to open the lockfile %s.", lockfile.c_str());
        return false;
    }

    pid_t pid = 0;
    is >> pid;

    syslog(LOG_NOTICE, "bennu-field-deviced | START | pid: %d", pid);
    if (pid != 0)
    {
        return true;
    }

    return false;
}

void stop(const std::string& instance)
{
    std::string lockfile("/var/run/" + instance + ".pid");

    std::ifstream is;
    is.open (lockfile.c_str());
    if (!is.is_open())
    {
        syslog(LOG_ERR, "The process is not running and has never run on this machine.");
        std::cerr << "The " << instance << " bennu-field-deviced process is not running and has never run on this machine." << std::endl;
        exit(EXIT_SUCCESS);
    }

    pid_t pid = 0;
    is >> pid;
    is.close();

    syslog(LOG_NOTICE, "bennu-field-deviced | STOP  | pid: %d", pid);
    if (pid != 0)
    {
        kill(pid, SIGTERM);
        sleep(1);
        kill(pid, SIGKILL);
    }

    std::ofstream os(lockfile.c_str(), std::fstream::trunc);
    os << 0;
    os.close();
}

void start(const std::string& instance)
{
    //Set our Logging Mask and open the Log
    setlogmask(LOG_UPTO(LOG_ERR));
    openlog(instance.c_str(), LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER);

    //Fork the Parent Process
    int value = daemon(1, 0);
    if (value != 0)
    {
        syslog(LOG_ERR, "Invalid process id for bennu-field-deviced daemon!");
        exit(EXIT_FAILURE);
    }
    else
    {
        syslog(LOG_INFO, "Valid daemon process id for bennu-field-deviced daemon!");
    }

    std::string lockfile("/var/run/" + instance + ".pid");
    std::ofstream os(lockfile.c_str(), std::fstream::trunc);
    os << getpid();
    os.close();

    //Change File Mask
    umask(0);
}

int main(int argc, char** argv)
{
    po::variables_map vm;

    try
    {
        std::string program = "A simulation startup for a basic field device daemon in bennu (must be run as root).";
        po::options_description desc(program);
        desc.add_options()
            ("help", "show this help menu")
            ("command", po::value<std::string>(), "Command, start, stop, and restart")
            ("env", po::value<std::string>()->default_value("default"), "String identifier of this particular bennu-field-deviced instance")
            ("file", po::value<std::string>(), "Field-device config file to load")
        ;

        po::store(po::parse_command_line(argc, argv, desc), vm);

        po::positional_options_description p;
        p.add("command", 1);
        po::store(po::command_line_parser(argc, argv).
                  options(desc).positional(p).run(), vm);
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
        exit(EXIT_FAILURE);
    }


    if (getuid() != 0)
    {
        syslog(LOG_ERR, "Must be root user in order to launch bennu-field-deviced!");
        std::cerr << "Must be root user in order to launch bennu-field-deviced!" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Set up and run Brash server
    bennu::shell::BrashServer brash = bennu::shell::BrashServer();
    if (ss.find("1") != std::string::npos)
    {
        if (!brash.isRunning())
        {
            std::cerr << "Starting shell..." << std::endl;
            if (brash.start() != 0) std::cerr << "Shell failed to start!" << std::endl;
        }
    }
    else
    {
        if (brash.isRunning())
        {
            std::cerr << "Stopping shell..." << std::endl;
            brash.stop();
        }
    }

    std::string env = vm["env"].as<std::string>();
    std::string instance = env + "-bennu-field-deviced";

    std::string command;
    if (vm.count("command"))
    {
        command = vm["command"].as<std::string>();
    }
    else
    {
        std::cerr << "Error: you must define a command: start, stop, or restart." << std::endl;
        exit(EXIT_FAILURE);
    }

    if (command == "start")
    {
        if (!isAlreadyRunning(instance))
        {
            std::cerr << "Starting field-device process...";
            start(instance);
            std::cerr << "DONE" << std::endl;
        }
        else
        {
            syslog(LOG_ERR, "bennu-field-deviced is already running, so either stop or restart the process!");
            std::cerr << "bennu-field-deviced with environment " << env << " is already running, so either stop or restart the process!" << std::endl;
            exit(EXIT_FAILURE);
        }

    }
    else if (command == "stop")
    {
        if (isAlreadyRunning(instance))
        {
            std::cerr << "Stopping field-device process....";
            stop(instance);
            std::cerr << "DONE" << std::endl;
            exit(EXIT_SUCCESS);
        }
        else
        {
            std::cerr << "bennu-field-deviced is not running, so it cannot be stopped." << std::endl;
            exit(EXIT_SUCCESS);
        }
    }
    else if (command == "restart")
    {
        std::cerr << "Restarting field-device process....";
        if (isAlreadyRunning(instance))
        {
            stop(instance);
        }
        start(instance);
        std::cerr << "DONE" << std::endl;
    }
    else
    {
        std::cerr << "Unrecognized \"command\" " << command << " so no action will be taken." << std::endl;
        exit(EXIT_FAILURE);
    }

    std::string stdoutLog("/etc/sceptre/log/" + instance + "-stdout.log");
    std::string stderrLog("/etc/sceptre/log/" + instance + "-stderr.log");
    if (freopen(stdoutLog.c_str(), "w", stdout) == nullptr)
    {
        std::cerr << "error opening stdout log: " << stdoutLog.c_str() << std::endl;
    }
    if (freopen(stderrLog.c_str(), "w", stderr) == nullptr)
    {
        std::cerr << "error opening stderr log: " << stderrLog.c_str() << std::endl;
    }

    bennu::parsers::Parser::the()->registerTagForDynamicLibrary("field-device", "bennu-field-device-base");

    if (vm.count("file"))
    {
        if (!bennu::parsers::Parser::the()->load(vm["file"].as<std::string>()))
        {
            std::cerr << "ERROR: bennu-field-device failed when loading the field device configuration file!" << std::endl;
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        std::cerr << "ERROR: bennu-field-device must define a --file option for the field device configuration file!" << std::endl;
        exit(EXIT_FAILURE);
    }

    //logger.start();

    while (running)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Close the log
    syslog(LOG_INFO, "bennu-field-deviced exiting...");
    std::cerr << instance << " exiting..." << std::endl;
    closelog();

    fclose(stdout);
    fclose(stderr);

    return 0;
}
