#include <syslog.h>
#include <sys/stat.h>
#include <chrono>
#include <csignal>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>

#include <boost/program_options.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include "picosha2.h"

namespace po = boost::program_options;
namespace pt = boost::property_tree;

static bool deviceRunning = false;
static bool running = true;

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
        syslog(LOG_ERR, "Unable to open the lockfile %s.", lockfile.c_str());
        std::cerr << "Unable to open the lockfile " << lockfile << "." << std::endl;
        return false;
    }

    pid_t pid = 0;
    is >> pid;

    syslog(LOG_NOTICE, "bennu-watcherd | START | pid: %d", pid);
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

    std::cerr << "pid = " << pid << std::endl;
    syslog(LOG_NOTICE, "bennu-watcherd | STOP  | pid: %d", pid);
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
        syslog(LOG_ERR, "Invalid process id for bennu-watcherd daemon!");
        exit(EXIT_FAILURE);
    }
    else
    {
        syslog(LOG_INFO, "Valid daemon process id for bennu-watcherd daemon!");
    }

    std::string lockfile("/var/run/" + instance + ".pid");
    std::ofstream os(lockfile.c_str(), std::fstream::trunc);
    os << getpid();
    os.close();

    //Change File Mask
    umask(0);
}

void writeLog(const std::string& message, const std::string& type = "INFO")
{
    std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    char * time_str = ctime(&now_time);
    time_str[std::strlen(time_str)-1] = '\0';

    std::cout << std::setw(24) << std::left
              << time_str
              << " - "
              << std::setw(83) << std::left
              << message
              << "- "
              << std::setw(10) << std::left
              << type
              << std::endl << std::flush;
}

bool isDeviceRunning(const std::string& instance)
{
    // If the LOCKFILE cannot be opened, then we're going to assume that this is the first time
    //   we've run the bennu-field-deviced on this machine.
    std::string lockfile("/var/run/" + instance + ".pid");
    std::ifstream is(lockfile.c_str());
    if (!is.is_open())
    {
        std::cerr << "Unable to open the lockfile " << lockfile << "." << std::endl;
        writeLog("Unable to open the lockfile " + lockfile + ".", "WARNING");
        return false;
    }

    pid_t pid = 0;
    is >> pid;

    std::cerr << "pid = " << pid << std::endl;
    if (pid != 0)
    {
        return true;
    }

    return false;
}

void startDevice(const std::string& env, const std::string& file)
{
    std::string command = "/usr/bin/bennu-field-deviced --c start --env " + env + " --file " + file;

    int value = system(command.c_str());

    if (value != 0)
    {
        std::cerr << "Error on bennu-field-deviced start!" << std::endl;
        writeLog("Error on bennu-field-deviced start!", "ERROR");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
}

void stopDevice(const std::string& env)
{
    std::string command = "/usr/bin/bennu-field-deviced --c stop --env " + env;

    int value = system(command.c_str());

    if (value != 0)
    {
        std::cerr << "Error on bennu-field-deviced stop!" << std::endl;
        writeLog("Error on bennu-field-deviced stop!", "ERROR");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
}

int calc_sha256(const char* path, std::string& hash_str)
{
    std::ifstream f(path, std::ios::binary);
    std::vector<unsigned char> s(picosha2::k_digest_size);
    picosha2::hash256(f, s.begin(), s.end());
    hash_str = picosha2::bytes_to_hex_string(s.begin(), s.end());
    return 0;
}

int main (int argc, char** argv)
{
    po::variables_map vm;

    try
    {
        std::string program = "A watcher for a basic field-device daemon in bennu (must be run as root).";
        po::options_description desc(program);
        desc.add_options()
            ("help", "show this help menu")
            ("command", po::value<std::string>(), "Command, start, stop, and restart")
            ("env", po::value<std::string>()->default_value("default"), "String identifier of the particular bennu-field-deviced instance to watch")
            ("file", po::value<std::string>()->default_value("/etc/sceptre/config.xml"), "Field device config file to load")
            ("binary", po::value<std::string>()->default_value("/home/sceptre/bennu-field-deviced.firmware"), "Binary to watch")
            ("watcher_config", po::value<std::string>()->default_value("/etc/sceptre/watcher.ini"), "Watcher specific configuration")
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
        std::cerr << "Must be root user in order to launch bennu-watcherd!" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);


    std::string env = vm["env"].as<std::string>();
    std::string instance = env + "-bennu-watcherd";

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
            std::cerr << "Starting Watcher process....";
            start(instance);
            std::cerr << "DONE" << std::endl;
        }
        else
        {
            syslog(LOG_ERR, "bennu-watcherd is already running, so either stop or restart the process!");
            std::cerr << "bennu-watcherd with environment " << env << " is already running, so either stop or restart the process!" << std::endl;
            exit(EXIT_FAILURE);
        }

    }
    else if (command == "stop")
    {
        if (isAlreadyRunning(instance))
        {
            std::cerr << "Stopping Watcher process....";
            stop(instance);
            std::cerr << "DONE" << std::endl;
            exit(EXIT_SUCCESS);
        }
        else
        {
            std::cerr << "bennu-watcherd is not running, so it cannot be stopped." << std::endl;
            exit(EXIT_SUCCESS);
        }
    }
    else if (command == "restart")
    {
        if (isAlreadyRunning(instance))
        {
            stop(instance);
        }
        start(instance);
    }
    else
    {
        std::cerr << "Unrecognized \"command\" " << command << " so no action will be taken." << std::endl;
        exit(EXIT_FAILURE);
    }

    std::string stdoutLog("/etc/sceptre/log/watcher-stdout.log");
    std::string stderrLog("/etc/sceptre/log/watcher-stderr.log");
    if (freopen(stdoutLog.c_str(), "w", stdout) == nullptr)
    {
        std::cerr << "error opening stdout log: " << stdoutLog.c_str() << std::endl;
    }
    if (freopen(stderrLog.c_str(), "w", stderr) == nullptr)
    {
        std::cerr << "error opening stderr log: " << stderrLog.c_str() << std::endl;
    }

    const std::string binToWatch(vm["binary"].as<std::string>());

    pt::ptree wconfig;
    pt::ini_parser::read_ini(vm["watcher_config"].as<std::string>(), wconfig);
    const std::string reghash(wconfig.get<std::string>("Hashes.reghash"));
    const std::string shellhashA(wconfig.get<std::string>("Hashes.shellhashA"));
    const std::string shellhashB(wconfig.get<std::string>("Hashes.shellhashB"));

    std::string deviceInstance = env + "-bennu-field-deviced";
    std::string configFile = vm["file"].as<std::string>();

    std::string current_hash;
    std::string new_hash;

    calc_sha256(binToWatch.c_str(), current_hash);

    if (!isDeviceRunning(deviceInstance))
    {
        writeLog("Starting field-device process....");
        startDevice(env, configFile);

        if (isDeviceRunning(deviceInstance))
        {
            writeLog("DONE");
            deviceRunning = true;
        }
        else
        {
            writeLog("field-device failed to start", "ERROR");
            deviceRunning = false;
            current_hash = "Firmware not loaded";
        }
    }
    else
    {
        writeLog("field-device process is already running", "WARNING");
        deviceRunning = true;
    }

    writeLog("Firmware Version Hash: " + current_hash);

    while (running)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        calc_sha256(binToWatch.c_str(), new_hash);

        if (current_hash != new_hash && (new_hash == reghash || new_hash == shellhashA || new_hash == shellhashB))
        {
            writeLog("Firmware Updated");
            stopDevice(env);
            std::string copyCommand("cp " + binToWatch + " /usr/bin/bennu-field-deviced");
            int value = system(copyCommand.c_str());
            if (value != 0)
            {
                std::cerr << "Error copying new firmware!" << std::endl;
                writeLog("Error copying new firmware!", "ERROR");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            startDevice(env, configFile);

            if (isDeviceRunning(deviceInstance))
            {
                writeLog("field-device restarted");
                deviceRunning = true;
                current_hash = new_hash;
            }
            else
            {
                writeLog("field-device failed to restart on firmware update", "ERROR");
                deviceRunning = false;
                current_hash = "Firmware not loaded";
            }

            writeLog("Firmware Version Hash: " + current_hash);

        }
        else if (current_hash != new_hash && new_hash == "Firmware not found!")
        {
             current_hash = new_hash;
        }

        if (deviceRunning && !isDeviceRunning(deviceInstance))
        {
            writeLog("field-device stopped");
            deviceRunning = false;
        }

        if (!deviceRunning && isDeviceRunning(deviceInstance))
        {
            writeLog("field-device started");
            deviceRunning = true;
            calc_sha256(binToWatch.c_str(), current_hash);
        }
    }
}
