#ifndef BENNU_FIELDDEVICE_IO_OUTPUTMODULE_HPP
#define BENNU_FIELDDEVICE_IO_OUTPUTMODULE_HPP

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

private:
    std::shared_ptr<distributed::Client> mClient;

};

} // namespace io
} // namespace bennu

#endif // BENNU_FIELDDEVICE_IO_OUTPUTMODULE_HPP
