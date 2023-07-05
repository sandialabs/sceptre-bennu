#include "ClientConnection.hpp"

#include <functional>
#include <sstream>

namespace bennu {
namespace comms {
namespace modbus {

ClientConnection::ClientConnection(const std::string& endpoint, uint8_t unitId) :
    mPersistConnection(true),
    mProtocolStack()
{
    std::size_t findResult = endpoint.find("tcp://");
    if (!mProtocolStack)
    {
        session_opts so;
        // If "tcp://" in endpoint string, create a TCP client. Otherwise, create a serial client
        if (findResult == std::string::npos)
        {
            mClient = std::make_shared<utility::SerialClient>(utility::SerialClient(endpoint));
            std::dynamic_pointer_cast<utility::SerialClient>(mClient)->connect();
            so.transmit_fn = std::bind(&utility::AbstractClient::send, mClient, std::placeholders::_1, std::placeholders::_2);
            so.receive_fn = std::bind(&utility::AbstractClient::receive, mClient, std::placeholders::_1, std::placeholders::_2);
        }
        else
        {
            mClient = std::make_shared<utility::TcpClient>(utility::TcpClient(endpoint));
            std::dynamic_pointer_cast<utility::TcpClient>(mClient)->connect();
            so.transmit_fn = std::bind(&utility::AbstractClient::send, mClient, std::placeholders::_1, std::placeholders::_2);
            so.receive_fn = std::bind(&utility::AbstractClient::receive, mClient, std::placeholders::_1, std::placeholders::_2);
        }
        mProtocolStack.reset(new protocol_stack(so));
        mProtocolStack->app_layer.set_unit_id(unitId);
    }
}

std::string ClientConnection::logError(error_code_t::type error, const std::string& type, std::uint16_t startAddress, size_t size)
{
    std::ostringstream os;
    switch (error)
    {
        case error_code_t::ILLEGAL_FUNCTION:
            os << "Invalid " << type << " request on " << mName << " - illegal function addresses starting at " << startAddress << " and reading " << size;
            break;
        case error_code_t::ILLEGAL_DATA_ADDRESS:
            os << "Invalid " << type << " request on " << mName << " - illegal data address starting at " << startAddress << " and reading " << size;
            break;
        case error_code_t::ILLEGAL_DATA_VALUE:
            os << "Invalid " << type << " request on " << mName << " - illegal data value starting at " << startAddress << " and reading " << size;
            break;
        case error_code_t::SLAVE_DEVICE_FAILURE:
            os << "Invalid " << type << " request on " << mName << " - slave device failure starting at " << startAddress << " and reading " << size;
            break;
        case error_code_t::ACKNOWLEDGE:
            os << "Invalid " << type << " request on " << mName << " - ack starting at " << startAddress << " and reading " << size;
            break;
        case error_code_t::SLAVE_DEVICE_BUSY:
            os << "Invalid " << type << " request on " << mName << " - slave device busy starting at " << startAddress << " and reading " << size;
            break;
        case error_code_t::MEMORY_PARITY_ERROR:
            os << "Invalid " << type << " request on " << mName << " - memory parity error starting at " << startAddress << " and reading " << size;
            break;
        default:
            os << "Invalid " << type << " request on " << mName << " - unknown error starting at " << startAddress << " and reading " << size;
            break;
    }
    return os.str();

}

StatusMessage ClientConnection::readRegisterByTag(const std::string& tag, comms::RegisterDescriptor& rd)
{
    StatusMessage sm = STATUS_INIT;
    auto status = getRegisterDescriptorByTag(tag, rd) ? STATUS_SUCCESS : STATUS_FAIL;
    if (!status)
    {
        std::string msg = "readRegisterByTag(): Unable to find tag -- " + tag;
        sm.status = status;
        sm.message = msg.data();
        return sm;
    }
    switch  (rd.mRegisterType)
    {
        case comms::eStatusReadWrite:
        {
            std::vector<bool> data;
            error_code_t::type error = mProtocolStack->app_layer.read_coils(rd.mRegisterAddress, 1, data);
            if (!error)
            {
                rd.mStatus = data[0];
                return sm;
            }
            break;
        }
        case comms::eStatusReadOnly:
        {
            std::vector<bool> data;
            error_code_t::type error = mProtocolStack->app_layer.read_discrete_inputs(rd.mRegisterAddress, 1, data);
            if (!error)
            {
                rd.mStatus = data[0];
                return sm;
            }
            break;
        }
        case comms::eValueReadWrite:
        {
            std::vector<uint16_t> data;
            error_code_t::type error = mProtocolStack->app_layer.read_holding_registers(rd.mRegisterAddress, 1, data);
            if (!error)
            {
                std::map<uint16_t, ScaledValue>::iterator svIter = mScaledValues.find(rd.mRegisterAddress);
                if (svIter != mScaledValues.end())
                {
                    rd.mFloatValue = (data[0] - svIter->second.mIntercept) / svIter->second.mSlope;
                }
                return sm;
            }
            break;
        }
        case comms::eValueReadOnly:
        {
            std::vector<uint16_t> data;
            error_code_t::type error = mProtocolStack->app_layer.read_input_registers(rd.mRegisterAddress, 1, data);
            if (!error)
            {
                std::map<uint16_t, ScaledValue>::iterator svIter = mScaledValues.find(rd.mRegisterAddress);
                if (svIter != mScaledValues.end())
                {
                    rd.mFloatValue = (data[0] - svIter->second.mIntercept) / svIter->second.mSlope;
                }
                return sm;
            }
            break;
        }
        default:
        {
            std::string msg = "readRegisterByTag(): Unknown reg type -- " + rd.mRegisterType;
            sm.message = msg.data();
            sm.status = STATUS_FAIL;
            return sm;
        }
    }
    std::string msg = "readRegisterByTag(): Failure";
    sm.message = msg.data();
    sm.status = STATUS_FAIL;
    return sm;
}

StatusMessage ClientConnection::readRegisters(const std::vector<ConnectionMessage>& messages, std::vector<comms::RegisterDescriptor>& responses, std::vector<comms::LogMessage>& logMessages)
{
    if (!mPersistConnection)
    {
        mClient->connect();
    }

    responses.clear();

    size_t expectedResponses = 0;

    for (size_t m = 0; m < messages.size(); ++m)
    {
        expectedResponses += messages[m].mRegisters.size();
        comms::LogMessage lm;
        std::ostringstream os;

        switch  (messages[m].mRegisterType)
        {
            case comms::eStatusReadWrite:
            {
                std::uint16_t startAddress = (*messages[m].mRegisters.begin()).mRegisterAddress;
                size_t size = messages[m].mRegisters.size();
                std::vector<bool> data;
                error_code_t::type error = mProtocolStack->app_layer.read_coils(startAddress, size, data);

                if (error == error_code_t::NO_ERROR)
                {
                    std::set<comms::RegisterDescriptor, comms::RegDescComp>::const_iterator regIter;
                    size_t c = 0;
                    for(regIter = messages[m].mRegisters.begin(); regIter != messages[m].mRegisters.end(); ++regIter, ++c)
                    {
                        comms::RegisterDescriptor response = *regIter;
                        response.mStatus = data[c];

                        responses.push_back(response);
                    }
                    os << "Read coils on " << mName << " from start address " << startAddress << " and read " << size << " registers.";
                    lm.mEvent = "read coils";
                    lm.mLevel = "info";
                    lm.mMessage = os.str();
                    logMessages.push_back(lm);
                }
                else
                {
                    lm.mEvent = "read coils";
                    lm.mLevel = "error";
                    lm.mMessage = logError(error, "read coils", startAddress, size);
                    logMessages.push_back(lm);
                }
            }
                break;

            case comms::eStatusReadOnly:
            {
                std::uint16_t startAddress = (*messages[m].mRegisters.begin()).mRegisterAddress;
                size_t size = messages[m].mRegisters.size();
                std::vector<bool> data;
                error_code_t::type error = mProtocolStack->app_layer.read_discrete_inputs(startAddress, size, data);

                if (error == error_code_t::NO_ERROR)
                {
                    std::set<comms::RegisterDescriptor, comms::RegDescComp>::const_iterator regIter;
                    size_t di = 0;
                    for(regIter = messages[m].mRegisters.begin(); regIter != messages[m].mRegisters.end(); ++regIter, ++di)
                    {
                        comms::RegisterDescriptor response = *regIter;
                        response.mStatus = data[di];

                        responses.push_back(response);
                    }
                    os << "Read discrete inputs on " << mName << " from start address " << startAddress << " and read " << size << " registers.";
                    lm.mEvent = "read discrete input";
                    lm.mLevel = "info";
                    lm.mMessage = os.str();
                    logMessages.push_back(lm);
                }
                else
                {
                    lm.mEvent = "read discrete inputs";
                    lm.mLevel = "error";
                    lm.mMessage = logError(error, "read discrete inputs", startAddress, size);
                    logMessages.push_back(lm);
                }

            }
                break;

            case comms::eValueReadWrite:
            {
                std::uint16_t startAddress = (*messages[m].mRegisters.begin()).mRegisterAddress;
                size_t size = messages[m].mRegisters.size();
                std::vector<uint16_t> data;
                error_code_t::type error = mProtocolStack->app_layer.read_holding_registers(startAddress, size, data);

                if (error == error_code_t::NO_ERROR)
                {
                    std::set<comms::RegisterDescriptor, comms::RegDescComp>::const_iterator regIter;
                    size_t hr = 0;
                    for(regIter = messages[m].mRegisters.begin(); regIter != messages[m].mRegisters.end(); ++regIter, ++hr)
                    {
                        comms::RegisterDescriptor response = *regIter;

                        std::map<uint16_t, ScaledValue>::iterator svIter = mScaledValues.find(response.mRegisterAddress);
                        if (svIter != mScaledValues.end())
                        {
                            response.mFloatValue = (data[hr] - svIter->second.mIntercept) / svIter->second.mSlope;
                        }

                        responses.push_back(response);
                    }
                    os << "Read holding registers on " << mName << " from start address " << startAddress << " and read " << size << " registers.";
                    lm.mEvent = "read holding register";
                    lm.mLevel = "info";
                    lm.mMessage = os.str();
                    logMessages.push_back(lm);
                }
                else
                {
                    lm.mEvent = "read coils";
                    lm.mLevel = "error";
                    lm.mMessage = logError(error, "read holding registers", startAddress, size);
                    logMessages.push_back(lm);
                }

            }
                break;

            case comms::eValueReadOnly:
            {
                std::uint16_t startAddress = (*messages[m].mRegisters.begin()).mRegisterAddress;
                size_t size = messages[m].mRegisters.size();
                std::vector<uint16_t> data;
                error_code_t::type error = mProtocolStack->app_layer.read_input_registers(startAddress, size, data);

                if (error == error_code_t::NO_ERROR)
                {
                    std::set<comms::RegisterDescriptor, comms::RegDescComp>::const_iterator regIter;
                    size_t ir = 0;
                    for(regIter = messages[m].mRegisters.begin(); regIter != messages[m].mRegisters.end(); ++regIter, ++ir)
                    {
                        comms::RegisterDescriptor response = *regIter;
                        std::map<uint16_t, ScaledValue>::iterator svIter = mScaledValues.find(response.mRegisterAddress);
                        if (svIter != mScaledValues.end())
                        {
                            response.mFloatValue = (data[ir] - svIter->second.mIntercept) / svIter->second.mSlope;
                        }

                        responses.push_back(response);
                    }
                    os << "Read input registers on " << mName << " from start address " << startAddress << " and read " << size << " registers.";
                    lm.mEvent = "read input register";
                    lm.mLevel = "info";
                    lm.mMessage = os.str();
                    logMessages.push_back(lm);

                }
                else
                {
                    lm.mEvent = "read input registers";
                    lm.mLevel = "error";
                    lm.mMessage = logError(error, "read registers", startAddress, size);
                    logMessages.push_back(lm);
                }

            }
                break;

            default:
            {
                lm.mEvent = "invalid read";
                lm.mLevel = "error";
                lm.mMessage = logError(error_code_t::ILLEGAL_FUNCTION, "read registers", 0, 0);
                logMessages.push_back(lm);
            }
        }
    }

    StatusMessage sm = STATUS_INIT;
    sm.status = expectedResponses == responses.size() ? STATUS_SUCCESS : STATUS_FAIL;
    if (!sm.status)
    {
        std::string msg = "Failed";
        sm.message = msg.data();
    }
    return sm;
}

StatusMessage ClientConnection::writeCoil(const std::string& tag, bool value)
{
    StatusMessage sm = STATUS_INIT;
    comms::RegisterDescriptor rd;
    auto status = getRegisterDescriptorByTag(tag, rd) ? STATUS_SUCCESS : STATUS_FAIL;
    if (!status)
    {
        std::string msg = "writeBinary(): Unable to find tag -- " + tag;
        sm.status = status;
        sm.message = msg.data();
        return sm;
    }
    error_code_t::type error = mProtocolStack->app_layer.write_coil(rd.mRegisterAddress, value);
    if (!error)
    {
        return sm;
    }

    std::string msg = "writeCoil(): Failed writing coil";
    sm.status = STATUS_FAIL;
    sm.message = msg.data();
    return sm;
}

StatusMessage ClientConnection::writeHoldingRegister(const std::string& tag, float value)
{
    StatusMessage sm = STATUS_INIT;
    comms::RegisterDescriptor rd;
    auto status = getRegisterDescriptorByTag(tag, rd) ? STATUS_SUCCESS : STATUS_FAIL;
    if (!status)
    {
        std::string msg = "writeAnalog(): Unable to find tag -- " + tag;
        sm.status = status;
        sm.message = msg.data();
        return sm;
    }
    std::uint16_t data = 0;
    std::map<uint16_t, ScaledValue>::iterator svIter = mScaledValues.find(rd.mRegisterAddress);
    if (svIter != mScaledValues.end())
    {
        data = (value * svIter->second.mSlope) + svIter->second.mIntercept;
    }

    error_code_t::type error = mProtocolStack->app_layer.write_register(rd.mRegisterAddress, data);
    if (!error)
    {
        return sm;
    }
    std::string msg = "writeCoil(): Failed writing coil";
    sm.status = STATUS_FAIL;
    sm.message = msg.data();
    return sm;
}

} // namespace modbus
} // namespace comms
} // namespace bennu
