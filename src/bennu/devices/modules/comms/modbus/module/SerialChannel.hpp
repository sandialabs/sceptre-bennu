#ifndef BENNU_FIELDDEVICE_COMMS_MODBUS_SERIAL_CHANNEL_HPP
#define BENNU_FIELDDEVICE_COMMS_MODBUS_SERIAL_CHANNEL_HPP

#include "Channel.hpp"

namespace bennu {
namespace comms {
namespace modbus {

class SerialChannel : public Channel
{
public:
    SerialChannel(boost::asio::io_service& io_service, std::string endpoint, unsigned int baudRate = 9600, unsigned int dataBits = 8, unsigned int stopBits = 1, char parity = 'n', char flowControl = 'h');

    ~SerialChannel() = default;

    void connect();

    void close();

    void manageSocket(std::shared_ptr<protocol_stack> protocolStack);

    void readHandler(const boost::system::error_code& error, size_t bytesTransferred);

    void readQueueInsert(uint8_t* data, size_t data_length);

    void readQueue();

    void transmit(std::uint8_t* buffer, size_t size);

    std::shared_ptr<boost::asio::serial_port> getHandle();

    std::string getChannelType(); 

    SerialChannel(const SerialChannel&);

    SerialChannel(SerialChannel*);
private:
    std::shared_ptr<protocol_stack> mProtocolStack;
    static const unsigned int dataSize = 1024;
    uint8_t mData[dataSize];
    char mPayloadData[1024];
    std::string mDevice;
    std::shared_ptr<boost::asio::serial_port> mSerialPort;
    boost::asio::serial_port_base::baud_rate mBaudRate;
    boost::asio::serial_port_base::character_size mDataBits;
    boost::asio::serial_port_base::stop_bits mStopBits;
    boost::asio::serial_port_base::parity mParity;
    boost::asio::serial_port_base::flow_control mFlowControl;
    std::mutex readQueueMutex;
    std::vector<uint8_t> readBuffer;

    void setStopBits(unsigned int stopBits);
    void setParity(char parity);
    void setFlowControl(char flowControl);
    SerialChannel& operator =(const SerialChannel&);
};

}
}
}

#endif // BENNU_FIELDDEVICE_COMMS_MODBUS_SERIAL_CHANNEL_HPP
