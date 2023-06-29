#ifndef BENNU_FIELDDEVICE_IO_INPUTMODULE_HPP
#define BENNU_FIELDDEVICE_IO_INPUTMODULE_HPP

#include "bennu/devices/modules/io/IOModule.hpp"
#include "bennu/distributed/Utils.hpp"
#include "bennu/distributed/Subscriber.hpp"

namespace bennu {
namespace io {

class InputModule : public IOModule
{
public:
    InputModule();

    virtual ~InputModule() = default;

    virtual void start(const distributed::Endpoint& endpoint);

private:
    void subscriptionHandler(std::string& data);
    std::shared_ptr<distributed::Subscriber> mSubscriber;

};

} // namespace io
} // namespace bennu

#endif // BENNU_FIELDDEVICE_IO_INPUTMODULE_HPP
