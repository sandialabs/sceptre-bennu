#ifndef BENNU_FIELDDEVICE_COMMS_COMMSCLIENT_HPP
#define BENNU_FIELDDEVICE_COMMS_COMMSCLIENT_HPP

#include <memory>
#include <set>
#include <string>

#include "bennu/devices/modules/comms/base/Common.hpp"
#include "bennu/devices/modules/comms/base/CommsModule.hpp"

namespace bennu {
namespace comms {

class CommandInterface;

class CommsClient : public CommsModule
{
public:

    CommsClient() {}

    ~CommsClient() {}

    virtual std::set<std::string> getTags() const = 0;
    virtual bool isValidTag(const std::string& tag) const = 0;
    virtual StatusMessage readTag(const std::string& tag, comms::RegisterDescriptor& rd) const = 0;
    virtual StatusMessage writeBinaryTag(const std::string& tag, bool status) = 0;
    virtual StatusMessage writeAnalogTag(const std::string& tag, double value) = 0;

    void addCommandInterface(std::shared_ptr<CommandInterface> ci)
    {
        mCommandInterface = ci;
    }

protected:
    std::shared_ptr<CommandInterface> mCommandInterface;

};

} // namespace comms
} // namespace bennu

#endif // BENNU_FIELDDEVICE_COMMS_COMMSCLIENT_HPP
