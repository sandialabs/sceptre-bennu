#ifndef BENNU_UTILITY_TCPCLIENT_HPP
#define BENNU_UTILITY_TCPCLIENT_HPP

#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

#include <boost/asio.hpp>

#include "AbstractClient.hpp"

namespace bennu {
namespace utility {

class TcpClient : public AbstractClient
{
public:
    TcpClient(const std::string& endpoint) : mService()
    {
        std::size_t findResult = endpoint.find("tcp://"); 
        if (findResult != std::string::npos)
        {
            std::string ipAndPort = endpoint.substr(findResult + 6);
            mAddress = ipAndPort.substr(0, ipAndPort.find(":"));
            mPort = ipAndPort.substr(ipAndPort.find(":") + 1);            
        }
        else
        {
            return;
        }
    }

    bool connect()
    {
        if (mSocket && mSocket->is_open())
        {
            return true;
        }

        mSocket.reset(new boost::asio::ip::tcp::socket(mService));

        boost::asio::ip::tcp::resolver resolver(mService);
        boost::asio::ip::tcp::resolver::query query(boost::asio::ip::tcp::v4(), mAddress, mPort);

        // resolve the endpoint we're trying to talk to
        boost::asio::ip::tcp::resolver::iterator iterator = resolver.resolve(query);

        boost::system::error_code error;
        boost::asio::connect(*mSocket, iterator, error);

        if (!error && mSocket->is_open())
        {
            std::cout << "Successful connection to " << mAddress << " port: " << mPort << std::endl;
            return true;
        }
        else
        {
            std::cerr << "Connection error: \"" << error.message() << "\" at " << mAddress << " port: " << mPort << "!" << std::endl;
            mSocket.reset();
            return false;
        }
    }

    void disconnect()
    {
        if (mSocket && mSocket->is_open())
        {
            boost::system::error_code error;

            mSocket->shutdown(boost::asio::socket_base::shutdown_both, error);
            if (error)
            {
                std::cerr << "ERROR: socket shutdown failed: " << error.message() << std::endl;
            }

            mSocket->close(error);
            if (error)
            {
                std::cerr << "ERROR: socket close failed: " << error.message() << std::endl;
            }

        }
        mSocket.reset();
    }

    void send(uint8_t* buffer, size_t size)
    {
        if (connect())
        {
            boost::system::error_code error;
            boost::asio::write(*mSocket, boost::asio::buffer(buffer, size), error);

            if (error)
            {
                std::cerr << "ERROR: connection send error: " << error.message() << std::endl;
                disconnect();
            }
        }
    }

    void receive(uint8_t* buffer, size_t)
    {
        if (mSocket)
        {
            boost::system::error_code error;
            boost::asio::read(*mSocket, boost::asio::buffer(buffer, 4), error);

            boost::asio::read(*mSocket, boost::asio::buffer(&buffer[4], 2), boost::asio::transfer_exactly(2), error);
            if (error)
            {
                std::cerr << "ERROR: receive message length failed with error: " << error.message() << std::endl;
            }

            uint16_t readData;
            std::memcpy(&readData, &buffer[4], 2);

            uint16_t length = ntohs(readData);

            boost::asio::read(*mSocket, boost::asio::buffer(&buffer[6], length), boost::asio::transfer_exactly(length), error);
            if (error)
            {
                std::cerr << "ERROR: receive message of length " << length << " failed with error: " << error.message() << std::endl;
                disconnect();
            }
        }
    }

    TcpClient(const TcpClient& tcpClient) : mService()
    {
        mAddress = tcpClient.mAddress;
        mPort = tcpClient.mPort;
        mSocket = tcpClient.mSocket;
    }

private:
    std::string mAddress;
    std::string mPort;
    boost::asio::io_service mService;
    std::shared_ptr<boost::asio::ip::tcp::socket> mSocket;

};

} // namespace utility
} // namespace bennu

#endif // BENNU_UTILITY_TCPCLIENT_HPP
