#include "SerialChannel.hpp"

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

SerialChannel::SerialChannel(boost::asio::io_service& io_service, std::string endpoint, unsigned int baudRate, unsigned int dataBits, unsigned int stopBits, char parity, char flowControl) : mBaudRate(baudRate), mDataBits(dataBits)
{
    std::size_t findResult = endpoint.find("tcp://");
    if (findResult == std::string::npos)
    {
        mDevice = endpoint;
    }

    mSerialPort = std::make_shared<boost::asio::serial_port>(io_service);
    setStopBits(stopBits);
    setParity(parity);
    setFlowControl(flowControl);
}

void SerialChannel::connect()
{
    boost::system::error_code ec;

    try
    {
        if (mSerialPort->is_open())
        {
            mSerialPort->close(ec);
        }
 
        mSerialPort->open(mDevice, ec);
    }
    catch (boost::system::system_error& error)
    {
        std::cerr << "unable to open " << mDevice << std::endl;
        return;
    }

    // Assuming we don't need to get exclusive access of the serial port, we'll stop here
}

void SerialChannel::close()
{
    boost::system::error_code ec;

    if (mSerialPort->is_open())
    {
        mSerialPort->close();
    }

    if (ec)
    {
        std::cerr << "There was a problem closing a serial port: " << ec.message() << std::endl;
    }
}

void SerialChannel::manageSocket(std::shared_ptr<protocol_stack> protocolStack)
{
    mProtocolStack = protocolStack;

    mSerialPort->async_read_some(boost::asio::buffer(mData, dataSize), boost::bind(&SerialChannel::readHandler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}

// Using a queue to buffer read data due to lack of formal connection (i.e. not TCP)
void SerialChannel::readHandler(const boost::system::error_code& error, size_t bytesTransferred)
{
    std::lock_guard<std::mutex> l(readQueueMutex);
    if (error)
    {
        std::cerr << "There was a problem reading serial data." << error.message() << std::endl;
    }
    else
    {
        readQueueInsert(mData, bytesTransferred);
        // Should have received a full message now, let's process the data
        if (readBuffer.size() >= 6)
        {
            readQueue();
        }
        mSerialPort->async_read_some(boost::asio::buffer(mData, dataSize), boost::bind(&SerialChannel::readHandler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
    }
}

void SerialChannel::readQueueInsert(uint8_t* data, size_t data_length)
{
    readBuffer.insert(readBuffer.end(), data, data+data_length);
}

// Process the data we've received and stored in our queue
void SerialChannel::readQueue()
{
    boost::system::error_code error;

    if (!error)
    {
        boost::system::error_code ec;
        uint16_t d;
        std::vector<uint8_t>::iterator it = readBuffer.begin() + 4;
        std::memcpy(&d, &readBuffer[2], 2);

        uint16_t readData;
        it += 2;
        std::memcpy(&readData, &readBuffer[4], 2);

        uint16_t length = ntohs(readData);

        it += length;
        std::copy(readBuffer.begin()+6, it, mPayloadData);
        mProtocolStack->data_receive_signal(&readBuffer[0], length+6);
        readBuffer.erase(readBuffer.begin(), it); 
    }
    else
    {
        std::cerr << "Modbus RTU receive message failed with error: " << error.message() << std::endl;
    }
}

void SerialChannel::transmit(std::uint8_t* buffer, size_t size)
{
    boost::system::error_code error;
    size_t bufferSize = boost::asio::write(*mSerialPort, boost::asio::buffer(buffer, size), error);
    if (error)
    {
        std::cerr << "Modbus RTUs transmit response failed with sent buffer of " << bufferSize << " and error: " << error.message() << std::endl;
        close();
    }
}

std::shared_ptr<boost::asio::serial_port> SerialChannel::getHandle()
{
    return mSerialPort;
}

std::string SerialChannel::getChannelType()
{
    return "serial";
}

void SerialChannel::setStopBits(unsigned int stopBits)
{
    switch(stopBits)
    {
        case 1:
            mStopBits = boost::asio::serial_port_base::stop_bits(boost::asio::serial_port_base::stop_bits::one);
            break;
        case 2:
            mStopBits = boost::asio::serial_port_base::stop_bits(boost::asio::serial_port_base::stop_bits::two);
            break;
        default:
            // Default to 1 stop bit
            mStopBits = boost::asio::serial_port_base::stop_bits(boost::asio::serial_port_base::stop_bits::one);
    }
}

void SerialChannel::setParity(char parity)
{
    switch (parity)
    {
        case 'e':
        case 'E':
            mParity = boost::asio::serial_port_base::parity(boost::asio::serial_port_base::parity::even);
            break;
        case 'o':
        case 'O':
            mParity = boost::asio::serial_port_base::parity(boost::asio::serial_port_base::parity::odd);
            break;
        case 'n':
        case 'N':
            mParity = boost::asio::serial_port_base::parity(boost::asio::serial_port_base::parity::none);
            break;
        default:
            // Default to no parity bit
            mParity = boost::asio::serial_port_base::parity(boost::asio::serial_port_base::parity::none);
    }
}

void SerialChannel::setFlowControl(char flowControl)
{
    switch(flowControl)
    {
        case 'h':
        case 'H':
            mFlowControl = boost::asio::serial_port_base::flow_control(boost::asio::serial_port_base::flow_control::hardware);
            break;
        case 's':
        case 'S':
            mFlowControl = boost::asio::serial_port_base::flow_control(boost::asio::serial_port_base::flow_control::software);
            break;
        case 'n':
        case 'N':
            mFlowControl = boost::asio::serial_port_base::flow_control(boost::asio::serial_port_base::flow_control::none);
            break;
        default:
            // Default to hardware flow control
            mFlowControl = boost::asio::serial_port_base::flow_control(boost::asio::serial_port_base::flow_control::hardware);
    }
}

SerialChannel::SerialChannel(const SerialChannel& serialChannel)
{
    session_opts so;
    mProtocolStack = std::make_shared<protocol_stack>(so);
    mDevice = serialChannel.mDevice;
    mSerialPort = serialChannel.mSerialPort;
}

SerialChannel::SerialChannel(SerialChannel* serialChannel)
{
    session_opts so;
    mProtocolStack = std::make_shared<protocol_stack>(so);
    mDevice = serialChannel->mDevice;
    mSerialPort = serialChannel->mSerialPort;
}

} //namespace bennu 
} //namespace comms 
} //namespace modbus 
