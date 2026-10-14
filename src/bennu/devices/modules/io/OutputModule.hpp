#ifndef BENNU_FIELDDEVICE_IO_OUTPUTMODULE_HPP
#define BENNU_FIELDDEVICE_IO_OUTPUTMODULE_HPP

#include <functional>
#include <memory>
#include <unordered_set>

#include "bennu/devices/modules/io/IOModule.hpp"
#include "bennu/distributed/Utils.hpp"
#include "bennu/distributed/Client.hpp"

namespace bennu {
namespace io {

class OutputModule : public IOModule
{
public:
    OutputModule();

    virtual ~OutputModule() = default;

    virtual void start(const distributed::Endpoint& endpoint);

    void scanOutputs();

    // test seam: send(payload) -> true iff ACKed; replied() -> any reply (ACK or ERR) received
    void setSender(std::function<bool(const std::string&)> send, std::function<bool()> replied)
    {
        mSend = std::move(send);
        mReplied = std::move(replied);
    }

protected:
    void onPoint(const std::string& point) override
    {
        mPoints.insert(point);
        mDataManager->addOutputPoint(point);
    }

    // sent by this module: its own points, plus points no OutputModule owns (e.g. input-only
    // points written by comms or logic), which were always written before per-module scoping
    bool sends(const std::string& point) const
    {
        return mPoints.count(point) || !mDataManager->isOutputPoint(point);
    }

private:
    std::unordered_set<std::string> mPoints; // points owned by this module
    bool mWriteFailed = false;               // log the error once per outage
    std::shared_ptr<distributed::Client> mClient;
    std::function<bool(const std::string&)> mSend;
    std::function<bool()> mReplied;
};

} // namespace io
} // namespace bennu

#endif // BENNU_FIELDDEVICE_IO_OUTPUTMODULE_HPP
