#include "CommsModuleCreator.hpp"

#include "bennu/parsers/Parser.hpp"

namespace bennu {
namespace comms {

void CommsModuleCreator::handleCommsTreeData(const boost::property_tree::ptree& tree,
    std::shared_ptr<field_device::DataManager> dm)
{
    for (auto handler : mCommsDataHandlers)
    {
        // there will be one handler, which maps to one CommsModule per tag. For example, there can only be one "modbus-server" per
        //  field device.
        std::shared_ptr<comms::CommsModule> module = handler(tree, dm);

        mCommsModules.push_back(module);
    }
}

static bool CommsModuleCreatorInit()
{
    bennu::parsers::Parser::the()->registerTagForDynamicLibrary("modbus-server", "bennu-modbus-tcp");
    bennu::parsers::Parser::the()->registerTagForDynamicLibrary("modbus-client", "bennu-modbus-tcp");
    bennu::parsers::Parser::the()->registerTagForDynamicLibrary("dnp3-server", "bennu-dnp3-module");
    bennu::parsers::Parser::the()->registerTagForDynamicLibrary("dnp3-client", "bennu-dnp3-module");
    bennu::parsers::Parser::the()->registerTagForDynamicLibrary("bacnet-server", "bennu-bacnet-module");
    bennu::parsers::Parser::the()->registerTagForDynamicLibrary("bacnet-client", "bennu-bacnet-module");
    bennu::parsers::Parser::the()->registerTagForDynamicLibrary("iec60870-5-104-server", "bennu-iec60870-5-module");
    bennu::parsers::Parser::the()->registerTagForDynamicLibrary("iec60870-5-104-client", "bennu-iec60870-5-module");

    return true;
}

static bool result = CommsModuleCreatorInit();

} // namespace comms
} // namespace bennu

