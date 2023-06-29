#include "bennu/devices/modules/comms/dnp3/module/Server.hpp"
#include "bennu/devices/modules/comms/dnp3/module/ServerCommandHandler.hpp"

#include "opendnp3/channel/ChannelRetry.h"
#include "opendnp3/channel/IPEndpoint.h"
#include "opendnp3/channel/SerialSettings.h"
#include "opendnp3/ConsoleLogger.h"
#include "opendnp3/gen/ServerAcceptMode.h"
#include "opendnp3/logging/LogLevels.h"
#include "opendnp3/outstation/DefaultOutstationApplication.h"
#include "opendnp3/outstation/IOutstationApplication.h"
#include "opendnp3/outstation/UpdateBuilder.h"

namespace bennu {
namespace comms {
namespace dnp3 {

Server::Server(std::shared_ptr<field_device::DataManager> dm) :
    bennu::utility::DirectLoggable("dnp3-server")
{
    // Initialize outstation stack
    mManager.reset(new opendnp3::DNP3Manager(std::thread::hardware_concurrency(), opendnp3::ConsoleLogger::Create()));
    setDataManager(dm);
}

void Server::init(const std::string& endpoint, const std::uint16_t& address)
{
    // Add TCPServer channel
    std::string chan("bennu-dnp3-CHANNEL");

    // If endpoint starts with tcp://, parse ip/port and use TCP
    std::size_t findResult = endpoint.find("tcp://");

    // Use Serial
    if (findResult == std::string::npos)
    {
        opendnp3::SerialSettings serialSettings;
        serialSettings.deviceName = endpoint;
        try
        {
            mChannel = mManager->AddSerial(chan.data(),
                                          opendnp3::levels::NORMAL,
                                          opendnp3::ChannelRetry::Default(),
                                          serialSettings,
                                          nullptr);
        }
        catch (std::exception& e)
        {
            logEvent("dnp3 serial server init", "error", "failed to add serial server: " + std::string(e.what()));
        return;
        }
    }
    // Use TCP
    else
    {
        // Handle splitting out ip and port from endpoint
        std::string ipAndPort = endpoint.substr(findResult + 6);
        std::string ip = ipAndPort.substr(0,ipAndPort.find(":"));
        std::uint16_t port = static_cast<std::uint16_t>(stoi(ipAndPort.substr(ipAndPort.find(":") + 1)));
        try
        {
            mChannel = mManager->AddTCPServer(chan.data(),
                                              opendnp3::levels::NORMAL,
                                              opendnp3::ServerAcceptMode::CloseNew,
                                              opendnp3::IPEndpoint(ip, port),
                                              nullptr);
        }
        catch (std::exception& e)
        {
            logEvent("dnp3 TCP server init", "error", "failed to add TCPServer: " + std::string(e.what()));
            return;
        }
    }

    // Configure outstation stack
    //   1) Initialize db
    //   2) Initialize dnp3 binary/analog data; kv.first = XML config register address,
    //      so this supports noncontiguous addresses
    opendnp3::DatabaseConfig db = opendnp3::DatabaseConfig();

    for (const auto& kv : mBinaryPoints)
    {
        db.binary_input[kv.first] = {};
    }

    for (const auto& kv : mAnalogPoints)
    {
        db.analog_input[kv.first] = {};
    }

    opendnp3::OutstationStackConfig config(db);

    // Log data size
    std::ostringstream log_stream;
    log_stream << "Binary Size is " << config.database.binary_input.size() << " and ";
    log_stream << "Analog Size is " << config.database.analog_input.size() << ".";
    logEvent("dnp3 server init", "info", log_stream.str());
    std::cout << log_stream.str() << std::endl;
    fflush(stdout);

    // Initialize event buffer size
    config.outstation.eventBufferConfig = opendnp3::EventBufferConfig::AllTypes(100);
    config.link.LocalAddr = address;

    try
    {
        // Initialize command handler
        pHandler.reset(new ServerCommandHandler(CommandStatus::SUCCESS));
        // Configure dnp3 database points with group/variation (i.e. input/output)
        configurePoints(config.database);
        // Add DNP3 Outstation to channel
        std::string outstationName("bennu-dnp3-OUTSTATION");
        mOutstation = mChannel->AddOutstation(outstationName,
                                              pHandler,
                                              opendnp3::DefaultOutstationApplication::Create(),
                                              config);
        pHandler->setOutstation(mOutstation);
        pHandler->setRtu(shared_from_this());
    }
    catch (std::exception& e)
    {
        logEvent("dnp3 server init", "error", "failed to add DNP3 Outstation: " + std::string(e.what()));
        return;
    }

    // Start outstation
    start();
}

void Server::start()
{
    mOutstation->Enable();
    mUpdateThread.reset(new std::thread(std::bind(&Server::update, this)));
}

void Server::update()
{
    while (1)
    {
        for (const auto& kv : mBinaryPoints)
        {
            const std::string tag = kv.second.tag;
            if (mDataManager->hasTag(tag))
            {
                opendnp3::UpdateBuilder builder;
                builder.Update(opendnp3::Binary(mDataManager->getDataByTag<bool>(tag)), kv.first);
                mOutstation->Apply(builder.Build());
            }
        }
        for (const auto& kv : mAnalogPoints)
        {
            const std::string tag = kv.second.tag;
            if (mDataManager->hasTag(tag))
            {
                opendnp3::UpdateBuilder builder;
                double ts = mDataManager->getTimestampByTag(tag);
                builder.Update(opendnp3::Analog(mDataManager->getDataByTag<double>(tag),
                                                opendnp3::Flags(0x01),
                                                opendnp3::DNPTime(ts)), kv.first);
                mOutstation->Apply(builder.Build());
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

// Configure dnp3 points with group/variation (i.e. input/output)
void Server::configurePoints(opendnp3::DatabaseConfig& config)
{
    for (const auto& kv : mBinaryPoints)
    {
        config.binary_input[kv.first].svariation = kv.second.svariation;
        config.binary_input[kv.first].evariation = kv.second.evariation;
        config.binary_input[kv.first].clazz = kv.second.clazz;
    }

    for (const auto& kv : mAnalogPoints)
    {
        config.analog_input[kv.first].svariation = kv.second.svariation;
        config.analog_input[kv.first].evariation = kv.second.evariation;
        config.analog_input[kv.first].clazz = kv.second.clazz;
        config.analog_input[kv.first].deadband = kv.second.deadband;
    }
}

bool Server::addBinaryInput
(
    const std::uint16_t address,
    const std::string& tag,
    const std::string& sgvar,
    const std::string& egvar,
    const std::string& clazz
){
    if (mDataManager->hasTag(tag))
    {
        opendnp3::StaticBinaryVariation sgv;
        opendnp3::EventBinaryVariation egv;
        opendnp3::PointClass cls;

        if (sgvar.empty())
        {
            sgv = opendnp3::StaticBinaryVariation::Group1Var2;
        }
        else
        {
            try
            {
                sgv = opendnp3::StaticBinaryVariationSpec::from_string(sgvar);
            }
            catch(const std::invalid_argument&)
            {
                return false;
            }
        }

        if (egvar.empty())
        {
            egv = opendnp3::EventBinaryVariation::Group2Var2;
        }
        else
        {
            try
            {
                egv = opendnp3::EventBinaryVariationSpec::from_string(egvar);
            }
            catch(const std::invalid_argument&)
            {
                return false;
            }
        }

        if (clazz.empty())
        {
            cls = opendnp3::PointClass::Class1;
        }
        else
        {
            try
            {
                cls = opendnp3::PointClassSpec::from_string(clazz);
            }
            catch(const std::invalid_argument&)
            {
                return false;
            }
        }

        Point<opendnp3::StaticBinaryVariation, opendnp3::EventBinaryVariation> point;

        point.address = address;
        point.tag = tag;
        point.svariation = sgv;
        point.evariation = egv;
        point.clazz = cls;

        mBinaryPoints[address] = point;

        return true;
    }
    return false;
}

bool Server::addBinaryOutput(const std::uint16_t address, const std::string& tag, const bool sbo = false)
{
    if (mDataManager->hasTag(tag))
    {
        Point<opendnp3::StaticBinaryVariation, opendnp3::EventBinaryVariation> point;
        point.address = address;
        point.tag = tag;
        point.sbo = sbo;

        mBinaryPoints[address] = point;

        return true;
    }
    return false;
}

bool Server::addAnalogInput
(
    const std::uint16_t address,
    const std::string& tag,
    const std::string& sgvar,
    const std::string& egvar,
    const std::string& clazz,
    double deadband
){
    if (mDataManager->hasTag(tag))
    {
        opendnp3::StaticAnalogVariation sgv;
        opendnp3::EventAnalogVariation egv;
        opendnp3::PointClass cls;

        if (sgvar.empty())
        {
            sgv = opendnp3::StaticAnalogVariation::Group30Var6;
        }
        else
        {
            try
            {
                sgv = opendnp3::StaticAnalogVariationSpec::from_string(sgvar);
            }
            catch(const std::invalid_argument&)
            {
                return false;
            }
        }

        if (egvar.empty())
        {
            egv = opendnp3::EventAnalogVariation::Group32Var6;
        }
        else
        {
            try
            {
                egv = opendnp3::EventAnalogVariationSpec::from_string(egvar);
            }
            catch(const std::invalid_argument&)
            {
                return false;
            }
        }

        if (clazz.empty())
        {
            cls = opendnp3::PointClass::Class1;
        }
        else
        {
            try
            {
                cls = opendnp3::PointClassSpec::from_string(clazz);
            }
            catch(const std::invalid_argument&)
            {
                return false;
            }
        }

        Point<opendnp3::StaticAnalogVariation, opendnp3::EventAnalogVariation> point;

        point.address = address;
        point.tag = tag;
        point.svariation = sgv;
        point.evariation = egv;
        point.clazz = cls;
        point.deadband = deadband;

        mAnalogPoints[address] = point;

        return true;
    }

    return false;
}

bool Server::addAnalogOutput(const std::uint16_t address, const std::string& tag, const bool sbo = false)
{
    if (mDataManager->hasTag(tag))
    {
        Point<opendnp3::StaticAnalogVariation, opendnp3::EventAnalogVariation> point;
        point.address = address;
        point.tag = tag;
        point.sbo = sbo;

        mAnalogPoints[address] = point;

        return true;
    }
    return false;
}

void Server::writeBinary(std::uint16_t address, bool value)
{
    std::ostringstream log_stream;
    log_stream << "Binary point command at address " << address << " with value " << value << ".";
    logEvent("dnp3 Server writeBinary", "info", log_stream.str());
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
    mDataManager->addUpdatedBinaryTag(iter->second.tag, value);
    log_stream.str("");
    log_stream << "Data successfully written.";
    logEvent("write binary", "info", log_stream.str());
}

void Server::writeAnalog(std::uint16_t address, float value)
{
    std::ostringstream log_stream;
    log_stream << "Analog point command at address " << address << " with value " << value << ".";
    logEvent("dnp3 Server writeAnalog", "info", log_stream.str());
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
    mDataManager->addUpdatedAnalogTag(iter->second.tag, value);
    log_stream.str("");
    log_stream << "Data successfully written.";
    logEvent("write analog", "info", log_stream.str());
}

const Point<opendnp3::StaticBinaryVariation, opendnp3::EventBinaryVariation>*
Server::getBinaryPoint(const uint16_t address)
{
    auto iter = mBinaryPoints.find(address);
    if (iter == mBinaryPoints.end())
    {
        return NULL;
    }

    return &iter->second;
}

const Point<opendnp3::StaticAnalogVariation, opendnp3::EventAnalogVariation>*
Server::getAnalogPoint(const uint16_t address)
{
    auto iter = mAnalogPoints.find(address);
    if (iter == mAnalogPoints.end())
    {
        return NULL;
    }

    return &iter->second;
}

} // namespace dnp3
} // namespace comms
} // namespace bennu
