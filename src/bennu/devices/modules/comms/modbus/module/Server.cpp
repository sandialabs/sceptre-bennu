#include "Server.hpp"

#include <functional>

#include <boost/fusion/include/has_key.hpp>
#include <boost/fusion/sequence/intrinsic/has_key.hpp>

#ifdef __linux
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace bennu {
namespace comms {
namespace modbus {

Server::Server(std::shared_ptr<field_device::DataManager> dm) :
    bennu::utility::DirectLoggable("modbus-server"),
    mIOService()
{
    setAdditionalFilterInformation("modbus");
    setDataManager(dm);
}

Server::~Server()
{
    mOutstationThread->join();
    mAcceptor->close();

    mOutstationThread.reset();

    for (auto c : mConnections)
    {
        c.reset();
    }
    mConnections.clear();
}

void Server::accept(const std::string& endpoint)
{
    mEndpoint = endpoint;
    std::size_t findResult = endpoint.find("tcp://");
    boost::system::error_code error;
    // If "tcp://" is in endpoint string, create a TCP channel. Otherwisie, create a serial channel
    if (findResult == std::string::npos)
    {
        mChannel = std::make_shared<SerialChannel>(SerialChannel(mIOService, endpoint)); 
        if (mChannel)
        {
            std::dynamic_pointer_cast<SerialChannel>(mChannel)->connect();
            setupConnection(error);
        }
    }
    else
    {
        std::string ipAndPort = endpoint.substr(findResult + 6);
        mAddress = ipAndPort.substr(0, ipAndPort.find(":"));
        mPort = stoi(ipAndPort.substr(ipAndPort.find(":") + 1));
        try
        {
            if (!mAcceptor || !mAcceptor->is_open())
            {
                mAcceptor.reset(new boost::asio::ip::tcp::acceptor(mIOService, boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(mAddress), mPort)));
            }
        }
        catch (boost::system::system_error& error)
        {
            std::ostringstream os;
            os << "Unable to bind to " << mAddress << ":" <<  mPort;
            logDebug("error", os.str());
            return;
        }

        // Create a new TcpChannel object to handle additional/multiple connections
        mChannel.reset(new TcpChannel(mIOService));
        if (mChannel)
        {
            mAcceptor->async_accept(std::dynamic_pointer_cast<TcpChannel>(mChannel)->mSocket, boost::bind(&Server::acceptConnectionHandler, this, boost::asio::placeholders::error));
        }
    }
}

void Server::acceptConnectionHandler(const boost::system::error_code& error)
{
    if (!error)
    {
        setupConnection(error);
        std::cerr << "A new incoming connection started." << std::endl;  
    }
    else
    {
        std::cerr << "There was a problem with accepting an incoming connection: " << error.message() << std::endl;
    }

    accept(mEndpoint);
}


void Server::run(std::string endpoint)
{
    boost::system::error_code error;

    accept(endpoint);

    mIOService.run();
}

void Server::setupConnection(const boost::system::error_code& error)
{
    if (!error)
    {
        session_opts so;
        so.transmit_fn = std::bind(&Channel::transmit, mChannel, std::placeholders::_1, std::placeholders::_2);

        std::shared_ptr<protocol_stack> protocolStack(new protocol_stack(so));
        // register modbus application layer callbacks
        boost::fusion::at_key<READ_COILS>(protocolStack->app_layer.callbacks) = std::bind(&Server::readCoils, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
        boost::fusion::at_key<READ_DISCRETE_INPUTS>(protocolStack->app_layer.callbacks) = std::bind(&Server::readDiscreteInputs, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
        boost::fusion::at_key<READ_HOLDING_REGS>(protocolStack->app_layer.callbacks) = std::bind(&Server::readHoldingRegisters, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
        boost::fusion::at_key<READ_INPUT_REGS>(protocolStack->app_layer.callbacks) = std::bind(&Server::readInputRegisters, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

        boost::fusion::at_key<WRITE_SINGLE_COIL>(protocolStack->app_layer.callbacks) = std::bind(&Server::writeCoils, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
        boost::fusion::at_key<WRITE_SINGLE_REG>(protocolStack->app_layer.callbacks) = std::bind(&Server::writeHoldingRegisters, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

        boost::fusion::at_key<WRITE_MULTI_COIL>(protocolStack->app_layer.callbacks) = std::bind(&Server::writeCoils, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
        boost::fusion::at_key<WRITE_MULTI_REG>(protocolStack->app_layer.callbacks) = std::bind(&Server::writeHoldingRegisters, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

        // run the connection in a new thread
        mChannel->manageSocket(protocolStack);
        mConnections.push_back(mChannel);
    }
    else
    {
        logDebug("error", "There was a problem with accepting an incoming connection: " + error.message());
    }
}

void Server::start(std::string endpoint)
{
    // Allow only one OutstationThread to be running at a time.
    if (!mOutstationThread)
    {
        mOutstationThread.reset(new std::thread(std::bind(&Server::run, this, endpoint)));
    }
}

bool Server::addCoil(const uint16_t address, const std::string& tag)
{
    if (mDataManager->hasTag(tag))
    {
        mCoils[address] = tag;
        return true;
    }

    return false;

}

bool Server::addDiscreteInput(const uint16_t address, const std::string& tag)
{
    if (mDataManager->hasTag(tag))
    {
        mDiscreteInputs[address] = tag;
        return true;
    }

    return false;
}

bool Server::addHoldingRegister(const uint16_t address, const std::string& tag, const std::pair<double, double>& range)
{
    if (mDataManager->hasTag(tag))
    {
        mHoldingRegisters[address] = tag;
        ScaledValue sv;
        sv.mRange = range;
        sv.mSlope = c16bitScale / (range.first - range.second);
        sv.mIntercept = -(sv.mSlope * range.second);
        mScaledValues[address] = sv;
        return true;
    }


    return false;
}

bool Server::addInputRegister(const uint16_t address, const std::string& tag, const std::pair<double, double>& range)
{
    if (mDataManager->hasTag(tag))
    {
        mInputRegisters[address] = tag;
        ScaledValue sv;
        sv.mRange = range;
        sv.mSlope = c16bitScale / (range.first - range.second);
        sv.mIntercept = -(sv.mSlope * range.second);
        mScaledValues[address] = sv;
        return true;
    }
    return false;
}

error_code_t::type Server::readCoils(uint16_t startAddress, uint16_t size, std::vector<bool>& values)
{
    std::ostringstream os;
    os << "start address for read coils request is " << startAddress << " and read " << size << " coils.";
    if (!mDataManager)
    {
        os.str("");
        os << "There was an error with the data module";
        logEvent("read coils", "error", os.str());
        return error_code_t::SLAVE_DEVICE_FAILURE;
    }

    for (size_t i = startAddress; i < startAddress + size; ++i)
    {
        auto iter = mCoils.find(i);
        if (iter == mCoils.end())
        {
            os.str("");
            os << "Invalid read coils request address " << i;
            logEvent("read coils", "error", os.str());
            return error_code_t::ILLEGAL_DATA_VALUE;
        }
        if (mDataManager->hasTag(iter->second))
        {
            values.push_back(mDataManager->getDataByTag<bool>(iter->second));
        }
    }

    // We weren't able to find/return all the data asked for, so this is an error.
    if (values.size() < size)
    {
        os.str("");
        os << "Invalid read coils request - too many addresses starting at " << startAddress << " and reading " << size;
        logEvent("read coils", "error", os.str());
        return error_code_t::ILLEGAL_DATA_VALUE;
    }

    logEvent("read coils", "info", os.str());

    return error_code_t::NO_ERROR;
}

error_code_t::type Server::writeCoils(uint16_t startAddress, uint16_t size, const std::vector<bool>& value)
{
    std::ostringstream os;
    os << "start address for write coils request is " << startAddress << " and read " << size << " coils.";
    if (!mDataManager)
    {
        os.str("");
        os << "There was an error with the data module";
        logEvent("write coils", "error", os.str());
        return error_code_t::SLAVE_DEVICE_FAILURE;
    }

    // Cycle through all addresses that we want to write
    for (auto i = startAddress; i < startAddress + size; ++i)
    {
        // If the coil address isn't a "data" value, check to see if it's a configuration value.
        // If not, output an illegal address error.
        auto iter = mCoils.find(i);
        if (iter == mCoils.end())
        {
            // If it's not a data or configuration value, then the address does not exist on device.
            os.str("");
            os << "Invalid write coils request address: " << i;
            logEvent("write coils", "error", os.str());
            return error_code_t::ILLEGAL_DATA_ADDRESS;
        }

        mDataManager->addUpdatedBinaryTag(iter->second, value[i - startAddress]);
        os.str("");
        os << "Data successfully written.";
        logEvent("write coils", "info", os.str());

    }

    return error_code_t::NO_ERROR;
}

error_code_t::type Server::readDiscreteInputs(uint16_t startAddress, uint16_t size, std::vector<bool>& values)
{
    std::ostringstream os;
    os << "start address for read discrete inputs request is " << startAddress << " and read " << size << " registers.";
    if (!mDataManager)
    {
        os.str("");
        os << "There was an error with the data module";
        logEvent("read discrete inputs", "error", os.str());
        return error_code_t::SLAVE_DEVICE_FAILURE;
    }

    for (size_t i = startAddress; i < startAddress + size; ++i)
    {
        auto iter = mDiscreteInputs.find(i);
        if (iter == mDiscreteInputs.end())
        {
            os.str("");
            os << "Invalid read discrete inputs request address: " << i;
            logEvent("read discrete inputs", "error", os.str());
            return error_code_t::ILLEGAL_DATA_ADDRESS;
        }
        std::string t = iter->second;
        if (mDataManager->hasTag(iter->second))
        {
            values.push_back(mDataManager->getDataByTag<bool>(iter->second));
        }

    }

    // We weren't able to find/return all the data asked for, so this is an error.
    if (values.size() < size)
    {
        os.str("");
        os << "Invalid read discete inputs request - too many addresses starting at " << startAddress << " and reading " << size;
        logEvent("read discrete inputs", "error", os.str());
        return error_code_t::ILLEGAL_DATA_VALUE;
    }

    logEvent("read discrete inputs", "info", os.str());

    return error_code_t::NO_ERROR;
}

error_code_t::type Server::readHoldingRegisters(uint16_t startAddress, uint16_t size, std::vector<uint16_t>& values)
{
    std::ostringstream os;
    os << "start address for read holding registers request is " << startAddress << " and read " << size << " holding registers.";
    if (!mDataManager)
    {
        os.str("");
        os << "There was an error with the data module";
        logEvent("read holding registers", "error", os.str());
        return error_code_t::SLAVE_DEVICE_FAILURE;
    }

    for (size_t i = startAddress; i < startAddress + size; ++i)
    {
        auto iter = mHoldingRegisters.find(i);
        if (iter == mHoldingRegisters.end())
        {
            os.str("");
            os << "Invalid read holding registers request address: " << i;
            logEvent("read holding registers", "error", os.str());
            return error_code_t::ILLEGAL_DATA_ADDRESS;
        }

        if (mDataManager->hasTag(iter->second))
        {
            double value = mDataManager->getDataByTag<double>(iter->second);
            auto svIter = mScaledValues.find(i);
            if (svIter != mScaledValues.end())
            {
                values.push_back((svIter->second.mSlope * value) + svIter->second.mIntercept);
            }
            else
            {
                values.push_back(value);
            }
        }
    }

    // We weren't able to find/return all the data asked for, so this is an error.
    if (values.size() < size)
    {
        os.str("");
        os << "Invalid read holding registers request - too many addresses starting at " << startAddress << " and reading " << size;
        logEvent("read holding registers", "error", os.str());
        return error_code_t::ILLEGAL_DATA_VALUE;
    }

    logEvent("read holding registers", "info", os.str());

    return error_code_t::NO_ERROR;
}

error_code_t::type Server::writeHoldingRegisters(uint16_t startAddress, uint16_t size, const std::vector<uint16_t>& value)
{
    std::ostringstream os;
    os << "start address for write holding registers request is " << startAddress << " and write " << size << " holding registers.";
    if (!mDataManager)
    {
        os.str("");
        os << "There was an error with the data module";
        logEvent("write holding registers", "error", os.str());
        return error_code_t::SLAVE_DEVICE_FAILURE;
    }

    // Cycle through all addresses that we want to write
    for (auto i = startAddress; i < startAddress + size; ++i)
    {
        auto iter = mHoldingRegisters.find(i);
        if (iter == mHoldingRegisters.end())
        {
            // If it's not a data or configuration value, then the address does not exist on device.
            os.str("");
            os << "Invalid write holding registers request address: " << i;
            logEvent("write holding registers", "error", os.str());
            return error_code_t::ILLEGAL_DATA_ADDRESS;

        }

        double newValue = value[i - startAddress];
        auto svIter = mScaledValues.find(i);
        if (svIter != mScaledValues.end())
        {
            newValue = (value[i - startAddress] - svIter->second.mIntercept) / svIter->second.mSlope;
        }

        mDataManager->addUpdatedAnalogTag(iter->second, newValue);
        os.str("");
        os << "Data successfully written.";
        logEvent("write holding registers", "info", os.str());

    }

    return error_code_t::NO_ERROR;
}

error_code_t::type Server::readInputRegisters(uint16_t startAddress, uint16_t size, std::vector<uint16_t>& values)
{
    std::ostringstream os;
    os << "start address for read input registers request is " << startAddress << " and read " << size << " input registers.";
    if (!mDataManager)
    {
        os.str("");
        os << "There was an error with the IO module";
        logEvent("read input registers", "error", os.str());
        return error_code_t::SLAVE_DEVICE_FAILURE;
    }

    for (size_t i = startAddress; i < startAddress + size; ++i)
    {
        auto iter = mInputRegisters.find(i);
        if (iter == mInputRegisters.end())
        {
            os.str("");
            os << "Invalid read input registers request address: " << i;
            logEvent("read input registers", "error", os.str());
            return error_code_t::ILLEGAL_DATA_ADDRESS;
        }

        if (mDataManager->hasTag(iter->second))
        {
            double value = mDataManager->getDataByTag<double>(iter->second);
            auto svIter = mScaledValues.find(i);
            if (svIter != mScaledValues.end())
            {
                values.push_back((svIter->second.mSlope * value) + svIter->second.mIntercept);
            }
            else
            {
                values.push_back(value);
            }
        }

    }

    // We weren't able to find/return all the data asked for, so this is an error.
    if (values.size() < size)
    {
        os.str("");
        os << mName << " -- invalid read input registers request - too many addresses starting at " << startAddress << " and reading " << size;
        logEvent("read input registers", "error", os.str());
        return error_code_t::ILLEGAL_DATA_VALUE;
    }

    logEvent("read input registers", "info", os.str());

    return error_code_t::NO_ERROR;
}

Server::Server(const Server& server) : 
    bennu::utility::DirectLoggable("modbus-server"),
    mIOService()
{
    mAcceptor = server.mAcceptor;
    mChannel = server.mChannel;
}

} // namespace modbus
} // namespace comms
} // namespace bennu
