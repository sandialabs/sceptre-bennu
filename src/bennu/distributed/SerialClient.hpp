#ifndef BENNU_UTILITY_SERIALCLIENT_HPP
#define BENNU_UTILITY_SERIALCLIENT_HPP

#include <iostream>
#include <string>
#include <vector>

#include <boost/asio.hpp>

#include "AbstractClient.hpp"

namespace bennu {
namespace utility {

class SerialClient : public AbstractClient
{
public:
    SerialClient(const std::string& endpoint, unsigned int baudRate = 9600, unsigned int dataBits = 8, size_t timeout = 1000) : mBaudRate(baudRate), mDataBits(dataBits), mService(), mTimer(mService), mTimeout(timeout)
    {
        std::size_t findResult = endpoint.find("tcp://");
        if (findResult == std::string::npos)
        {
            mDevice = endpoint;            
            setStopBits(1);
            setParity('n');
            setFlowControl('h');
            mSerialPort = std::make_shared<boost::asio::serial_port>(mService);
        }
    }

    bool connect()
    { 
        boost::system::error_code ec;

        if (mSerialPort && mSerialPort->is_open())
        {
            return true;
        }

        try
        {
            mSerialPort->open(mDevice, ec);
        }
        catch (boost::system::system_error& error)
        {
            std::cerr << "unable to open " << mDevice << std::endl;
            return false;
        }

        return true;
    }

    void disconnect()
    {
        if (mSerialPort && mSerialPort->is_open())
        {
            boost::system::error_code error;

            mSerialPort->close();
            if (error)
            {
                std::cerr << "ERROR: closing serial port failed: " << error.message() << std::endl;
            }
        }
    }

    void send(uint8_t* buffer, size_t size)
    {
        if (connect())
        {
            boost::system::error_code error;
            boost::asio::write(*mSerialPort, boost::asio::buffer(buffer, size), error);

            if (error)
            {
                std::cerr << "ERROR: serial data send error: " << error.message() << std::endl;
                disconnect();
            }
        }
    }

    void receive(uint8_t* buffer, size_t)
    {
        if (connect())
        {
            mService.reset();
            boost::asio::async_read(*mSerialPort, boost::asio::buffer(buffer, 6), boost::bind(&SerialClient::receiveHandler, this, buffer, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));

            // Setup deadline time
            mTimer.expires_from_now(boost::posix_time::milliseconds(mTimeout));
            mTimer.async_wait(boost::bind(&SerialClient::timeoutHandler, this, boost::asio::placeholders::error));

            // Block until read completes or timer expires
            mService.run();
        }
    }

    void receiveHandler(uint8_t* buffer, const boost::system::error_code& error, size_t bytesTransferred)
    {
        if (error)
        {
            std::cerr << "There was a problem reading serial data." << error.message() << std::endl;
        }
        else
        {
            if (bytesTransferred >= 6)
            {
                uint16_t readData;
                std::memcpy(&readData, &buffer[4], 2);

                uint16_t length = ntohs(readData);
                boost::system::error_code ec;
                boost::asio::read(*mSerialPort, boost::asio::buffer(&buffer[6], length), boost::asio::transfer_exactly(length), ec);
                if (error)
                {
                    std::cerr << "ERROR: receive message of length " << length << " failed with error: " << error.message() << std::endl;
                    disconnect();
                }
                
                // Read complete! Cancel our timer
                mTimer.cancel();

            }
        }
    }

    void timeoutHandler(const boost::system::error_code& error)
    {
        if (error)
        {
            // Timer was cancelled, just return
            return;
        }

        // Timer expired; let's cancel the read, which will enter the read handler with an error
        mSerialPort->cancel();
    }

    SerialClient(const SerialClient& serialClient) : mBaudRate(serialClient.mBaudRate), mDataBits(serialClient.mDataBits), mService(), mTimer(mService), mTimeout(serialClient.mTimeout)
    {
        mDevice = serialClient.mDevice;
        setStopBits(1);
        setParity('n');
        setFlowControl('h');
        mSerialPort = std::make_shared<boost::asio::serial_port>(mService);
    }


private:
    std::shared_ptr<boost::asio::serial_port> mSerialPort;
    boost::asio::serial_port_base::baud_rate mBaudRate;
    boost::asio::serial_port_base::character_size mDataBits;
    boost::asio::serial_port_base::stop_bits mStopBits;
    boost::asio::serial_port_base::parity mParity;
    boost::asio::serial_port_base::flow_control mFlowControl;
    std::string mDevice;
    boost::asio::io_service mService;
    boost::asio::deadline_timer mTimer;
    size_t mTimeout;                        // in milliseconds
    std::vector<uint8_t> receiveBuffer;
    static const unsigned int dataSize = 1024;
    uint8_t dataBuffer[dataSize];
    
    void setStopBits(unsigned int stopBits)
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

    void setParity(char parity)
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

    void setFlowControl(char flowControl)
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
};

}
}

#endif //BENNU_UTILITY_SERIALCLIENT_HPP

