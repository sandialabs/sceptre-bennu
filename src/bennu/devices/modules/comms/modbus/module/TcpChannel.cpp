#include "TcpChannel.hpp"

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


TcpChannel::TcpChannel(boost::asio::io_service& io_service) : mSocket(io_service)
{
}

TcpChannel::~TcpChannel()
{
}

void TcpChannel::close()
{
    boost::system::error_code ec;

    if (mSocket.is_open())
    {
        mSocket.close(ec);
    }

    if (ec)
    {
        std::cerr << "There was a problem closing a connection: " << std::endl;
    }
}

void TcpChannel::manageSocket(std::shared_ptr<protocol_stack> protocolStack)
{
    mProtocolStack = protocolStack;

    mData.resize(1024);
    boost::asio::async_read(mSocket, boost::asio::buffer(mData, mData.size()), boost::asio::transfer_exactly(4), boost::bind(&TcpChannel::readHandler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));

}

void TcpChannel::readHandler(const boost::system::error_code& error, size_t bytesTransferred)
{
    if (!error)
    {
        if (bytesTransferred == 4)
        {
            boost::system::error_code ec;
            // Because this is a TCP connection, we should be able to synchronously read message contents after initial read
            bytesTransferred += boost::asio::read(mSocket, boost::asio::buffer(&mData[4], 2), boost::asio::transfer_exactly(2), ec);

            uint16_t length = 0;
            if (!ec)
            {
                uint16_t readData;
                std::memcpy(&readData, &mData[4], 2);
                length = ntohs(readData);

                // The MBAP length counts the unit id and the PDU (so at least 2) and the body must fit in mData
                if (length < 2 || length > mData.size() - 6)
                {
                    ec = boost::asio::error::message_size;
                }
            }

            if (!ec)
            {
                bytesTransferred += boost::asio::read(mSocket, boost::asio::buffer(&mData[6], length), boost::asio::transfer_exactly(length), ec);
            }

            if (ec)
            {
                // A truncated or invalid request must not be processed, and the stream is out of sync: drop the connection
                std::cerr << "Modbus TCP receive message of length " << length << " failed with error: " << ec.message() << std::endl;
                close();
                mProtocolStack.reset();
                return;
            }
            mProtocolStack->data_receive_signal(&mData[0], bytesTransferred);

        }

        boost::asio::async_read(mSocket, boost::asio::buffer(mData, mData.size()), boost::asio::transfer_exactly(4), boost::bind(&TcpChannel::readHandler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
    }
    else
    {
        // If TCP connection is closed unexpectedly, we'll get this error, but we should be handling any additional requests
        std::cerr << "Modbus RTU receive message failed with error: " << error.message() << std::endl;
    }
}

void TcpChannel::transmit(std::uint8_t* buffer, size_t size)
{
    boost::system::error_code error;
    size_t bufferSize = boost::asio::write(mSocket, boost::asio::buffer(buffer, size), error);
    if (error)
    {
        std::cerr << "Modbus RTUs transmit response failed with sent buffer of " << bufferSize << " and error: " << error.message() << std::endl;
        close();
    }
}

void TcpChannel::reset(boost::asio::io_service& io_service)
{
    mSocket = boost::asio::ip::tcp::socket(io_service);
}

boost::asio::ip::tcp::socket TcpChannel::getHandle()
{
    return std::move(mSocket);
}

std::string TcpChannel::getChannelType()
{
    return "tcp";
}

}
}
}
