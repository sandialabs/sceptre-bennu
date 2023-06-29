#include "ClientConnection.hpp"

#include <cstdint>
#include <iostream>
#include <thread>

#include "opendnp3/app/AnalogOutput.h"
#include "opendnp3/app/ControlRelayOutputBlock.h"
#include "opendnp3/channel/ChannelRetry.h"
#include "opendnp3/channel/IPEndpoint.h"
#include "opendnp3/channel/SerialSettings.h"
#include "opendnp3/logging/LogLevels.h"
#include "opendnp3/master/DefaultMasterApplication.h"

namespace bennu {
namespace comms {
namespace dnp3 {

ClientConnection::ClientConnection(std::weak_ptr<Client> client,
                                   const std::uint16_t& address,
                                   const std::string& rtuEndpoint,
                                   const std::uint16_t& rtuAddress) :
    mManager(client.lock()->getManager()),
    mAddress(address),
    mRtuEndpoint(rtuEndpoint),
    mRtuAddress(rtuAddress)
{
}

void ClientConnection::init()
{
    // Initialize SOEHandler
    pHandler.reset(new ClientSoeHandler(shared_from_this()));

    //If endpoint starts with tcp://, parse ip/port and use TCP
    std::size_t findResult = mRtuEndpoint.find("tcp://");

    // Use Serial
    if(findResult == std::string::npos) {
        opendnp3::SerialSettings serialSettings;
        serialSettings.deviceName = mRtuEndpoint;
        try
        {
            mChannel = mManager->AddSerial("CLIENT",
                                          opendnp3::levels::NORMAL,
                                          opendnp3::ChannelRetry::Default(),
                                          serialSettings,
                                          nullptr);
        }
        catch (std::exception& e)
        {
            std::cout << "Failed to add Serial Client due to the error: " << std::string(e.what());
            return;
        }
    }

    // Use TCP
    else
    {
        // Handle splitting out ip and port from endpoint
        std::string ipAndPort = mRtuEndpoint.substr(findResult + 6);
        std::string ip = ipAndPort.substr(0,ipAndPort.find(":"));
        std::uint16_t port = static_cast<std::uint16_t>(stoi((ipAndPort.substr(ipAndPort.find(":") + 1))));
        try
        {
            mChannel = mManager->AddTCPClient("CLIENT",
                                              opendnp3::levels::NORMAL,
                                              opendnp3::ChannelRetry::Default(),
                                              std::vector<opendnp3::IPEndpoint>{opendnp3::IPEndpoint(ip, port)},
                                              "0.0.0.0",
                                              nullptr);
        }
        catch (std::exception& e)
        {
            std::cout << "Failed to add TCPClient due to the error: " << std::string(e.what());
            return;
        }
    }

    // Configure master stack
    mStackConfig.master.disableUnsolOnStartup = false;
    mStackConfig.master.startupIntegrityClassMask = opendnp3::ClassField(opendnp3::ClassField::CLASS_0);
    mStackConfig.master.unsolClassMask = opendnp3::ClassField(opendnp3::ClassField::CLASS_0);
    mStackConfig.master.integrityOnEventOverflowIIN = false;
    mStackConfig.link.LocalAddr = mAddress;
    mStackConfig.link.RemoteAddr = mRtuAddress;

    // Add DNP3 Master to channel
    try
    {
        mMaster = mChannel->AddMaster("MASTER",
                                      pHandler,
                                      opendnp3::DefaultMasterApplication::Create(),
                                      mStackConfig);
    }
    catch (std::exception& e)
    {
        std::cout << "Failed to add DNP3 Master due to the error: " << std::string(e.what()) ;
        return;
    }
}

StatusMessage ClientConnection::readRegisterByTag(const std::string& tag, comms::RegisterDescriptor& rd)
{
    auto status = getRegisterDescriptorByTag(tag, rd) ? STATUS_SUCCESS : STATUS_FAIL;
    StatusMessage sm = STATUS_INIT;
    sm.status = status;
    if (!status)
    {
        auto msg = "readRegisterByTag(): Unable to find tag -- " + tag;
        sm.message = &msg[0];
    }
    return sm;
}

StatusMessage ClientConnection::selectBinary(const std::string& tag, bool value)
{
    StatusMessage sm = STATUS_INIT;
    comms::RegisterDescriptor rd;
    auto status = getRegisterDescriptorByTag(tag, rd) ? STATUS_SUCCESS : STATUS_FAIL;
    if (!status)
    {
        std::string msg = "writeBinary(): Unable to find tag -- " + tag;
        sm.status = status;
        sm.message = &msg[0];
        return sm;
    }

    opendnp3::OperationType code = value ? opendnp3::OperationType::LATCH_ON : opendnp3::OperationType::LATCH_OFF;

    auto callback = [](const ICommandTaskResult& result) -> void
    {
        std::cout << "Summary: " << opendnp3::TaskCompletionSpec().to_string(result.summary) << std::endl;
        auto print = [](const CommandPointResult& res)
        {
            std::cout
                << "Header: " << res.headerIndex
                << " Index: " << res.index
                << " State: " << opendnp3::CommandPointStateSpec().to_string(res.state)
                << " Status: " << opendnp3::CommandStatusSpec().to_string(res.status)
                << std::endl;
        };
        result.ForeachItem(print);
    };

    ControlRelayOutputBlock crob(code);
    mMaster->SelectAndOperate(CommandSet({ WithIndex(crob, rd.mRegisterAddress) }), callback);
    // Update local data so we don't have to wait until the next poll
    updateBinary(rd.mRegisterAddress, value);
    return sm;
}

StatusMessage ClientConnection::writeBinary(const std::string& tag, bool value)
{
    StatusMessage sm = STATUS_INIT;
    comms::RegisterDescriptor rd;
    auto status = getRegisterDescriptorByTag(tag, rd) ? STATUS_SUCCESS : STATUS_FAIL;
    if (!status)
    {
        std::string msg = "writeBinary(): Unable to find tag -- " + tag;
        sm.status = status;
        sm.message = &msg[0];
        return sm;
    }
    opendnp3::OperationType code = value ? opendnp3::OperationType::LATCH_ON : opendnp3::OperationType::LATCH_OFF;
    auto callback = [](const ICommandTaskResult& result) -> void
    {
        std::cout << "Summary: " << opendnp3::TaskCompletionSpec().to_string(result.summary) << std::endl;
        auto print = [](const CommandPointResult& res)
        {
            std::cout
                << "Header: " << res.headerIndex
                << " Index: " << res.index
                << " State: " << opendnp3::CommandPointStateSpec().to_string(res.state)
                << " Status: " << opendnp3::CommandStatusSpec().to_string(res.status)
                << std::endl;
        };
        result.ForeachItem(print);
    };
    ControlRelayOutputBlock crob(code);
    mMaster->DirectOperate(CommandSet({ WithIndex(crob, rd.mRegisterAddress) }), callback);
    // Update local data so we don't have to wait until the next poll
    updateBinary(rd.mRegisterAddress, value);
    return sm;
}

StatusMessage ClientConnection::selectAnalog(const std::string& tag, double value)
{
    StatusMessage sm = STATUS_INIT;
    comms::RegisterDescriptor rd;
    auto status = getRegisterDescriptorByTag(tag, rd) ? STATUS_SUCCESS : STATUS_FAIL;
    if (!status)
    {
        std::string msg = "writeAnalog(): Unable to find tag -- " + tag;
        sm.status = status;
        sm.message = &msg[0];
        return sm;
    }
    auto val = static_cast<opendnp3::AnalogOutputFloat32>(value);
    auto callback = [](const ICommandTaskResult& result) -> void
    {
        std::cout << "Summary: " << opendnp3::TaskCompletionSpec().to_string(result.summary) << std::endl;
        auto print = [](const CommandPointResult& res)
        {
            std::cout
                << "Header: " << res.headerIndex
                << " Index: " << res.index
                << " State: " << opendnp3::CommandPointStateSpec().to_string(res.state)
                << " Status: " << opendnp3::CommandStatusSpec().to_string(res.status);
        };
        result.ForeachItem(print);
    };
    mMaster->SelectAndOperate(CommandSet({ WithIndex(val, rd.mRegisterAddress) }), callback);
    // Update local data so we don't have to wait until the next poll
    updateAnalog(rd.mRegisterAddress, value);
    return sm;
}

StatusMessage ClientConnection::writeAnalog(const std::string& tag, double value)
{
    StatusMessage sm = STATUS_INIT;
    comms::RegisterDescriptor rd;
    auto status = getRegisterDescriptorByTag(tag, rd) ? STATUS_SUCCESS : STATUS_FAIL;
    if (!status)
    {
        std::string msg = "writeAnalog(): Unable to find tag -- " + tag;
        sm.status = status;
        sm.message = &msg[0];
        return sm;
    }
    auto val = static_cast<opendnp3::AnalogOutputFloat32>(value);
    auto callback = [](const ICommandTaskResult& result) -> void
    {
        std::cout << "Summary: " << opendnp3::TaskCompletionSpec().to_string(result.summary) << std::endl;
        auto print = [](const CommandPointResult& res)
        {
            std::cout
                << "Header: " << res.headerIndex
                << " Index: " << res.index
                << " State: " << opendnp3::CommandPointStateSpec().to_string(res.state)
                << " Status: " << opendnp3::CommandStatusSpec().to_string(res.status);
        };
        result.ForeachItem(print);
    };
    mMaster->DirectOperate(CommandSet({ WithIndex(val, rd.mRegisterAddress) }), callback);
    // Update local data so we don't have to wait until the next poll
    updateAnalog(rd.mRegisterAddress, value);
    return sm;
}

void ClientConnection::start
(
    const std::uint32_t scanRateAll,
    const std::uint32_t scanRateClass0,
    const std::uint32_t scanRateClass1,
    const std::uint32_t scanRateClass2,
    const std::uint32_t scanRateClass3
){
    init();
    std::cout << "Initialized DNP3-CLIENT -- Address: " << mAddress << ", RTU Connection: " << mRtuEndpoint << std::endl;
    fflush(stdout);

    if (scanRateAll != 0)
    {
        // Add All Classes reccurring scan to master
        mMaster->AddClassScan(opendnp3::ClassField::AllClasses(), opendnp3::TimeDuration::Seconds(static_cast<std::uint64_t>(scanRateAll)), pHandler);
    }

    if (scanRateClass0 != 0)
    {
        mMaster->AddClassScan(opendnp3::ClassField(opendnp3::ClassField::CLASS_0), opendnp3::TimeDuration::Seconds(static_cast<std::uint64_t>(scanRateClass0)), pHandler);
    }

    if (scanRateClass1 != 0)
    {
        mMaster->AddClassScan(opendnp3::ClassField(opendnp3::ClassField::CLASS_1), opendnp3::TimeDuration::Seconds(static_cast<std::uint64_t>(scanRateClass1)), pHandler);
    }

    if (scanRateClass2 != 0)
    {
        mMaster->AddClassScan(opendnp3::ClassField(opendnp3::ClassField::CLASS_2), opendnp3::TimeDuration::Seconds(static_cast<std::uint64_t>(scanRateClass2)), pHandler);
    }

    if (scanRateClass3 != 0)
    {
        mMaster->AddClassScan(opendnp3::ClassField(opendnp3::ClassField::CLASS_3), opendnp3::TimeDuration::Seconds(static_cast<std::uint64_t>(scanRateClass3)), pHandler);
    }

    // All object scan for group30, var5 (analog input float)
    //mMaster->AddAllObjectsScan(opendnp3::GroupVariationID(30, 5), openpal::TimeDuration::Seconds(static_cast<uint64_t>(scanRate)));
    mMaster->Enable();
}

} // namespace dnp3
} // namespace comms
} // namespace bennu
