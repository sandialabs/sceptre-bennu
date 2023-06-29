#include "Server.hpp"

#include "bacnet/bacenum.h"
#include "bacnet/basic/object/bi.h"
#include "bacnet/basic/object/bo.h"
#include "bacnet/basic/object/ai.h"
#include "bacnet/basic/object/ao.h"
#include "bacnet/basic/object/bacnet.h"
#include "ports/linux/bacport.h"

namespace bennu {
namespace comms {
namespace bacnet {

Server::Server(std::shared_ptr<field_device::DataManager> dm) :
    bennu::utility::DirectLoggable("bacnet-server")
{
    setDataManager(dm);
}

void Server::start(const std::string& endpoint, const std::uint32_t& instance)
{
    // If endpoint starts with udp://, parse ip/port and use UDP
    std::size_t findResult = endpoint.find("udp://");

    if (findResult != std::string::npos)
    {
        // Handle splitting out ip and port from endpoint
        std::string ipAndPort = endpoint.substr(findResult + 6);
        std::string ip = ipAndPort.substr(0, ipAndPort.find(":"));
    }
    else
    {
        logEvent("bacnet server init", "error", "Unknown endpoint protocol (" + endpoint + ")");
        return;
    }

    // Log data size
    std::ostringstream log_stream;
    log_stream << "Initializing BACnet server: " << endpoint << " -- " << instance;
    logEvent("bacnet server init", "info", log_stream.str());
    std::cout << log_stream.str() << std::endl;
    fflush(stdout);

    // Set up initial comms
    BacnetPrepareComm(instance);
    // Initialize server-specific handlers
    BacnetServerInit();
    // Start server and update threads
    pServerThread.reset(new std::thread(std::bind(&Server::run, this)));
    pUpdateThread.reset(new std::thread(std::bind(&Server::update, this)));
}

void Server::run()
{
    while (1)
    {
        BacnetServerTask();
    }
}

/*
 * Update loop that syncs local bennu datastore with protocol datastore.
 * This handles data changes that come from a provider (inputs).
 */
void Server::update()
{
    while (1)
    {
        // kv = {<address>: {<tag>, eInput}}
        for (const auto& kv : mBinaryPoints)
        {
            const std::string tag = kv.second.first;
            if (mDataManager->hasTag(tag))
            {
                bool status = mDataManager->getDataByTag<bool>(tag);
                BACNET_BINARY_PV val = status == true ? BINARY_ACTIVE : BINARY_INACTIVE;
                if (kv.second.second == PointType::eInput)
                {
                    Binary_Input_Present_Value_Set(kv.first, val);
                }
                else if (kv.second.second == PointType::eOutput)
                {
                    Binary_Output_Present_Value_Set(kv.first, val, 0);
                }
            }

        }
        for (const auto& kv : mAnalogPoints)
        {
            const std::string tag = kv.second.first;
            if (mDataManager->hasTag(tag))
            {
                if (kv.second.second == PointType::eInput)
                {
                    Analog_Input_Present_Value_Set(kv.first, mDataManager->getDataByTag<double>(tag));
                }
                else if (kv.second.second == PointType::eOutput)
                {
                    Analog_Output_Present_Value_Set(kv.first, mDataManager->getDataByTag<double>(tag), 16);
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

/*
 * Write binary value to datastore.
 */
void Server::writeBinary(std::uint16_t address, bool value)
{
    std::ostringstream log_stream;
    log_stream << "Binary point command at address " << address << " with value " << value << ".";
    logEvent("bacnet Server writeBinary", "info", log_stream.str());
    if (!mDataManager)
    {
        log_stream.str("");
        log_stream << "There was an error with the data module";
        logEvent("write binary", "error", log_stream.str());
        return;
    }
    auto iter = mBinaryPoints.find(address);
    if (iter == mBinaryPoints.end())
    {
        log_stream.str("");
        log_stream << "Invalid binary point command request address: " << address;
        logEvent("binary point command", "error", log_stream.str());
        return;
    }
    mDataManager->addUpdatedBinaryTag(iter->second.first, value);
    log_stream.str("");
    log_stream << "Data successfully written.";
    logEvent("write binary", "info", log_stream.str());
}

/*
 * Write analog value to datastore.
 */
void Server::writeAnalog(std::uint16_t address, float value)
{
    std::ostringstream log_stream;
    log_stream << "Analog point command at address " << address << " with value " << value << ".";
    logEvent("bacnet Server writeAnalog", "info", log_stream.str());
    if (!mDataManager)
    {
        log_stream.str("");
        log_stream << "There was an error with the data module";
        logEvent("write binary", "error", log_stream.str());
        return;
    }
    auto iter = mAnalogPoints.find(address);
    if (iter == mAnalogPoints.end())
    {
        log_stream.str("");
        log_stream << "Invalid analog point command request address: " << address;
        logEvent("analog point command", "error", log_stream.str());
        return;
    }
    mDataManager->addUpdatedAnalogTag(iter->second.first, value);
    log_stream.str("");
    log_stream << "Data successfully written.";
    logEvent("write analog", "info", log_stream.str());
}

bool Server::addBinaryInput(const std::uint16_t address, const std::string& tag)
{
    if (mDataManager->hasTag(tag))
    {
        mBinaryPoints[address] = std::make_pair(tag, PointType::eInput);
        return true;
    }
    return false;
}

bool Server::addBinaryOutput(const std::uint16_t address, const std::string& tag)
{
    if (mDataManager->hasTag(tag))
    {
        mBinaryPoints[address] = std::make_pair(tag, PointType::eOutput);
        return true;
    }
    return false;
}

bool Server::addAnalogInput(const std::uint16_t address, const std::string& tag)
{
    if (mDataManager->hasTag(tag))
    {
        mAnalogPoints[address] = std::make_pair(tag, PointType::eInput);
        return true;
    }
    return false;
}

bool Server::addAnalogOutput(const std::uint16_t address, const std::string& tag)
{
    if (mDataManager->hasTag(tag))
    {
        mAnalogPoints[address] = std::make_pair(tag, PointType::eOutput);
        return true;
    }
    return false;
}

} // namespace bacnet
} // namespace comms
} // namespace bennu
