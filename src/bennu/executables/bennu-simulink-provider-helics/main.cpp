#include "HelicsHelper.hpp"

#include <iomanip>
#include <iostream>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <unistd.h>

#include <fcntl.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>

#include "helics/core/helicsCLI11.hpp"
#include "helics/helics.hpp"

using namespace bennu::executables;

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

        return ret.str();
    }
};

class BennuSimulinkProviderHelics : public HelicsFederate
{
public:
    BennuSimulinkProviderHelics(std::string& config) :
        HelicsFederate(config),
        mLock()
    {
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

    void tag(const std::string& tag, const std::string& strval)
    {
        if (mLogicVars.map().count(tag))
        {
            mLogicVars[tag] = strval;
        }
        else
        {
            // receive write and format newDto
            std::cout << "BennuSimulinkProviderHelics::tag ---- received write for tag: " << tag << " -- " << strval << std::endl;
            std::scoped_lock<std::shared_mutex> lock(mLock);
            sem_wait(mUpdatesSemaphore);
            std::string dataStr, dataType;
            if(strval == "true" || strval == "false")
            {
                dataStr = strval == "true" ? "1" : "0";
                dataType = "BOOLEAN";
            }
            else
            {
                dataStr = strval;
                dataType = "DOUBLE";
            }

            char* newDto = (char*)calloc(MAX_MSG_LEN, sizeof(char));
            sprintf(newDto, "%s:%s:%s", tag.data(), dataType.data(), dataStr.data());
            std::cout << "BennuSimulinkProviderHelics::tag ---- updating: " << newDto << std::endl;

            // write to updatesFifo - need to check appropriate return values to make sure it gets written
            int bytes = ::write(mUpdatesFifo, newDto, MAX_MSG_LEN); // Need :: here so we don't conflict with the write() provider method
            if (bytes < 0)
            {
                std::cout << "Error: Problem writing tag in simulink provider" << std::endl;
                std::cout << "\tCause: Could not send update message to FIFO" << std::endl;
                std::cout << "\tMsg: " << newDto << std::endl;
            }
            else if (bytes == 0)
            {
                std::cout << "Error: Problem writing tag in simulink provider" << std::endl;
                std::cout << "\tCause: No bytes were written to FIFO" << std::endl;
                std::cout << "\tMsg: " << newDto << std::endl;
            }

            sem_post(mUpdatesSemaphore);
        }
    }

    std::string tag(const std::string& tag)
    {
        if (mLogicVars.map().count(tag))
        {
            auto val = mLogicVars.find(tag)->asString();
            return val;
        }
        else
        {
            std::scoped_lock<std::shared_mutex> lock(mLock);
            sem_wait(mPublishSemaphore);
            std::string result{""};
            char* shmPtr = mPublishPointsShmAddress;
            for (int i=0; i<mNumPublishPoints; i++)
            {
                Dto dto{shmPtr};
                std::string name = dto.tag + "." + dto.field;
                if (dto.field == "processModelIO") {
                    name = dto.tag;
                }
                if (name == tag)
                {
                    result = dto.getDataString();
                    break;
                }
                else
                {
                    shmPtr += MAX_MSG_LEN;
                }
            }
            sem_post(mPublishSemaphore);

            return result;
        }
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

};

int main(int argc, char** argv)
{
    std::string config{"helics.json"};

    CLI::App app("Bennu Simulink Provider (HELICS)", "bennu-simulink-provider-helics");
    app.add_option("-c,--config", config, "json configuration filepath", true)
            ->required()
            ->check(CLI::ExistingFile);

    // parse initial app args
    CLI11_PARSE(app, argc, argv);

    auto bsph{BennuSimulinkProviderHelics(config)};
    bsph.run();

    return 0;
}
