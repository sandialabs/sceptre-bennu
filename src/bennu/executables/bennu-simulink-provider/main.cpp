#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <sstream>

#include <boost/date_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/program_options.hpp>

#include "bennu/distributed/Provider.hpp"
#include "bennu/distributed/Utils.hpp"

namespace po = boost::program_options;
using namespace bennu;
using namespace bennu::distributed;

/* *****************************************************************
 * Global Constants (VALUES MUST MATCH CONSTANTS IN SIMULINK SOLVER)
 ***************************************************************** */
/* shared memory for PublishPoints */
const unsigned int NUM_PUBLISH_POINTS_SHM_KEY {10610};
const unsigned int PUBLISH_POINTS_SHM_KEY     {10613};

/* fifo queue for UpdatePoints messages */
static const char* UPDATES_FIFO = "/tmp/updates_fifo";

/* provider <-> solver sync */
const unsigned int MAX_MSG_LEN {256};
const char* PUBLISH_SEM  {"publish_sem"};
const char* UPDATES_SEM  {"updates_sem"};

/* ********************************
 * Global Internal Helper Variables
 ******************************** */
/* used for setting the printed output precision of doubles / floats */
const unsigned int PRECISION {10};

static const unsigned int EXIT_ERROR = 1;

struct Dto
{
    std::string tag {""};
    std::string type {""};
    std::string data {""};
    std::string field {"processModelIO"}; /* field name default when not provided */

    Dto(std::string dto)
    {
        size_t pos = dto.find(":");
        if (std::string::npos == pos)
        {
            std::cout << "Error: Shared memory DTO format invalid!" << std::endl;
            std::cout << "\tInfo: DTO = " << dto << std::endl;
            std::exit(EXIT_ERROR);
        }
        tag = dto.substr(0, pos);
        dto.erase(0, pos + 1); // Remove up to, inluding, the first ":" from dto

        pos = tag.find(".");
        if (std::string::npos != pos)
        {
            field = tag.substr(pos + 1); // Get field name AFTER "." with +1
            tag = tag.substr(0, pos);
        }

        pos = dto.find(":");
        if (std::string::npos == pos)
        {
            std::cout << "Error: Shared memory DTO format invalid!" << std::endl;
            std::cout << "\tInfo: DTO = " << dto << std::endl;
            std::exit(EXIT_ERROR);
        }
        type = dto.substr(0, pos);
        dto.erase(0, pos + 1); // Remove up to, inluding, the second ":" from dto

        data = dto; // data is the remaining part of the dto
    }

    std::string getDataString()
    {
        std::stringstream strVal{data};
        strVal.precision(PRECISION);

        std::stringstream ret {""};

        if ("DOUBLE" == type)
        {
            double value;
            strVal >> value;
            ret << value;
        }
        else if ("BOOLEAN" == type)
        {
            bool status;
            strVal >> status;
            ret << std::boolalpha << status << std::noboolalpha;
        }
        else if ("INITIALIZED" == type)
        {
            // This will be the case when the update point hasn't be used yet,
            // the model hooks code initialized it to type:INITIALIZED with data:0;
            ret << "Null";
        }
        else
        {
            ret << "Unexpected";
        }

        return ret.str();
    }
};

constexpr auto durationToDuration(const float time_s)
{
    using namespace std::chrono;
    using fsec = duration<float>;
    return round<nanoseconds>(fsec{time_s});
}

class BennuSimulinkProvider : public Provider
{
public:
    double publishRate_setting;
    
    BennuSimulinkProvider(const Endpoint& serverEndpoint, const Endpoint& publishEndpoint, bool debug, double publishRate) :
        Provider(serverEndpoint, publishEndpoint),
        mLock(),
        mDebug(debug)
    {
        publishRate_setting = publishRate;
        mPublishSemaphore = sem_open(PUBLISH_SEM, O_CREAT, 0644, 0);
        if(SEM_FAILED == mPublishSemaphore)
        {
            std::cout << "Fatal: Unable to attach to publish semaphore" << std::endl;
            exit(EXIT_ERROR);
        }
        mUpdatesSemaphore = sem_open(UPDATES_SEM, O_CREAT, 0644, 0);
        if(SEM_FAILED == mUpdatesSemaphore)
        {
            std::cout << "Fatal: Unable to attach to updates semaphore" << std::endl;
            exit(EXIT_ERROR);
        }

        sem_wait(mPublishSemaphore);
        {
            /* Get number of PublishPoints from shared memory */
            int numPublishPointsShmId = shmget(NUM_PUBLISH_POINTS_SHM_KEY, sizeof(unsigned int), IPC_CREAT | 0666);
            char* numPublishPointsShmAddress = (char*)shmat(numPublishPointsShmId, NULL, 0);
            std::stringstream numPublishPointsStr {numPublishPointsShmAddress};
            numPublishPointsStr >> mNumPublishPoints;
            shmdt(numPublishPointsShmAddress);
            std::cout << "Info: Read " << mNumPublishPoints << " PublishPoints" << std::endl;

            /* Attach to PublishPoints shared memory (sending from simulink model to plcs) */
            mPublishPointsShmSize = mNumPublishPoints * MAX_MSG_LEN;
            mPublishPointsShmId = shmget(PUBLISH_POINTS_SHM_KEY, mPublishPointsShmSize, IPC_CREAT | 0666);
            mPublishPointsShmAddress = (char*)shmat(mPublishPointsShmId, NULL, 0);
            std::cout << "attached to publish points shared memory" << std::endl;
        }
        sem_post(mPublishSemaphore);

        /* connect to updates FIFO */
        mUpdatesFifo = open(UPDATES_FIFO, O_WRONLY | O_NONBLOCK);
        if (-1 == mUpdatesFifo)
        {
            std::cout << "Fatal: Unable to open UpdatePoints FIFO  at " << UPDATES_FIFO << " for non-blocked writing" << std::endl;
            exit(EXIT_ERROR);
        }
    }

    // Must return "ACK=tag1,tag2,..." or "ERR=<error message>"
    std::string query()
    {
        if (mDebug) { std::cout << "BennuSimulinkProvider::query ---- received query request" << std::endl; }
        std::scoped_lock<std::shared_mutex> lock(mLock);
        sem_wait(mPublishSemaphore);
        std::string result = "ACK=";
        char* shmPtr = mPublishPointsShmAddress;
        for (int i = 0; i < mNumPublishPoints; i++)
        {
            Dto dto{shmPtr};
            std::string tag = dto.tag + "." + dto.field;
            if (dto.field == "processModelIO") {
                tag = dto.tag;
            }
            result += tag + ",";
            shmPtr += MAX_MSG_LEN;
        }
        sem_post(mPublishSemaphore);
        return result;
    }

    // Must return "ACK=<value>" or "ERR=<error message>"
    std::string read(const std::string& tag)
    {
        if (mDebug) { std::cout << "BennuSimulinkProvider::read ---- received read for tag: " << tag << std::endl; }
        std::scoped_lock<std::shared_mutex> lock(mLock);
        sem_wait(mPublishSemaphore);
        std::string result{""};
        char* shmPtr = mPublishPointsShmAddress;
        for (int i = 0; i < mNumPublishPoints; i++)
        {
            Dto dto{shmPtr};
            std::string name = dto.tag + "." + dto.field;
            if (dto.field == "processModelIO") {
                name = dto.tag;
            }
            if (name == tag)
            {
                result += "ACK=";
                result += dto.getDataString();
                break;
            }
            else
            {
                shmPtr += MAX_MSG_LEN;
            }
        }
        sem_post(mPublishSemaphore);

        if (result == "")
        {
            result += "ERR=Tag not found";
        }
        return result;
    }

    // Must return "ACK=<success message>" or "ERR=<error message>"
    std::string write(const std::unordered_map<std::string,std::string>& tags)
    {
        std::string result;

        sem_wait(mUpdatesSemaphore);
        for (auto &it : tags)
        {
            std::string tag{it.first};
            std::string value{it.second};

            // recieve write and format newDto
            if (mDebug) { std::cout << "BennuSimulinkProvider::write ---- received write for tag: " << tag << " -- " << value << std::endl; }
            std::scoped_lock<std::shared_mutex> lock(mLock);
            std::string dataStr, dataType;
            if(value == "true" || value == "false")
            {
                dataStr = value == "true" ? "1" : "0";
                dataType = "BOOLEAN";
            }
            else
            {
                dataStr = value;
                dataType = "DOUBLE";
            }

            char* newDto = (char*)calloc(MAX_MSG_LEN, sizeof(char));
            sprintf(newDto, "%s:%s:%s", tag.data(), dataType.data(), dataStr.data());
            if (mDebug) { std::cout << "BennuSimulinkProvider::write ---- updating: " << newDto << std::endl; }

            // write to updatesFifo - need to check appropriate return values to make sure it gets written
            int bytes = ::write(mUpdatesFifo, newDto, MAX_MSG_LEN); // Need :: here so we don't conflict with the write() provider method
            if (bytes < 0)
            {
                std::cout << "Error: Problem sending update message to FIFO" << std::endl;
                std::cout << "\tMsg: " << newDto << std::endl;
                result += "ERR=Problem writing tag in simulink provider";
                break;
            } else if (bytes == 0) {
                std::cout << "Error: No bytes were written to FIFO" << std::endl;
                std::cout << "\tMsg: " << newDto << std::endl;
                result += "ERR=Nothing was written to the simulink provider";
                break;
            }
        }
        sem_post(mUpdatesSemaphore);

        if (result.empty())
        {
            result += "ACK=Updated tags in Simulink provider";
        }

        return result;
    }

    [[ noreturn ]] void periodicPublish()
    {
        while (1)
        {
            publishData();
            std::cout << std::flush;
            std::this_thread::sleep_for(durationToDuration(this->publishRate_setting));
        }
    }


    void publishData()
    {
        std::scoped_lock<std::shared_mutex> lock(mLock);
        sem_wait(mPublishSemaphore);
        std::string message;
        char* shmPtr = mPublishPointsShmAddress;
        for (int i = 0; i < mNumPublishPoints; i++)
        {
            Dto dto{shmPtr};
            std::string tag = dto.tag + "." + dto.field;
            if (dto.field == "processModelIO") {
                tag = dto.tag;
            }
            message += tag + ":" + dto.getDataString() + ",";
            shmPtr += MAX_MSG_LEN;
        }
        sem_post(mPublishSemaphore);
        if (mDebug) { std::cout << "BennuSimulinkProvider::publishData ---- publishing: " << message << std::endl; }
        publish(message);
    }

private:
    int mNumPublishPoints;
    int mPublishPointsShmSize;
    int mPublishPointsShmId;
    int mUpdatesFifo;
    char* mPublishPointsShmAddress;
    sem_t* mPublishSemaphore;
    sem_t* mUpdatesSemaphore;
    std::shared_mutex mLock;
    bool mDebug;
};


int main(int argc, char** argv)
{
    std::cout.precision(PRECISION);
    bool debug{false};
    po::options_description desc("Simulink Provider");
    desc.add_options()
        ("help",  "show this help menu")
        ("debug", po::bool_switch(&debug), "print debugging information")
        ("server-endpoint", po::value<std::string>()->default_value("tcp://127.0.0.1:5555"), "server listening endpoint")
        ("publish-endpoint", po::value<std::string>()->default_value("udp://239.0.0.1:40000"), "publishing endpoint");
        ("publish-rate", po::value<double>()->default_value(0.1), "rate at which updates are published from the provider to the simulation");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help"))
    {
        std::cout << desc << '\n';
        return 0;
    }

    Endpoint sEndpoint, pEndpoint;
    sEndpoint.str = vm["server-endpoint"].as<std::string>();
    pEndpoint.str = vm["publish-endpoint"].as<std::string>();
    double publishRate = vm["publish-rate"].as<double>();
    BennuSimulinkProvider bsp(sEndpoint, pEndpoint, debug, publishRate);
    bsp.run();
    return 0;
}
