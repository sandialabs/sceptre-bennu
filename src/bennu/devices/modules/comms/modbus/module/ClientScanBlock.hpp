#ifndef BENNU_FIELDDEVICE_COMMS_MODBUS_TCP_SCANBLOCK_HPP
#define BENNU_FIELDDEVICE_COMMS_MODBUS_TCP_SCANBLOCK_HPP

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "bennu/devices/modules/comms/base/Common.hpp"
#include "bennu/devices/modules/comms/modbus/module/Client.hpp"
#include "bennu/devices/modules/comms/modbus/module/ClientConnection.hpp"

namespace bennu {
namespace comms {
namespace modbus {

class ClientScanBlock
{
public:
    typedef std::map<std::shared_ptr<ClientConnection>, std::vector<ConnectionMessage> > ReadRequests;

    ClientScanBlock();

    ~ClientScanBlock();

    void addReadRequest(std::shared_ptr<ClientConnection> rtu, const std::set<comms::RegisterDescriptor, comms::RegDescComp>& registers);

    void setClient(std::shared_ptr<Client> client)
    {
        mClient = client;
    }

    void run();

    void stop()
    {
        mIsRunning = false;
    }

    void setScanInterval(size_t scanIntervalInSeconds)
    {
        mScanInterval = scanIntervalInSeconds;
    }

    size_t getScanInterval() const
    {
        return mScanInterval;
    }

protected:
    virtual std::string specificScan(std::set<std::string>& tagsToRead);

    void scanConnectionRegisters(std::shared_ptr<ClientConnection> rtu, const std::vector<ConnectionMessage>& messages);

    ReadRequests mReadRequests;

    std::vector<comms::RegisterDescriptor> mRegisterResponses;

    std::weak_ptr<Client> mClient;

    std::timed_mutex mLock;

private:
    void scan();
    bool mIsRunning;
    size_t mScanInterval;

};

} // namespace modbus
} // namespace comms
} // namespace bennu

#endif // BENNU_FIELDDEVICE_COMMS_MODBUS_TCP_SCANBLOCK_HPP
