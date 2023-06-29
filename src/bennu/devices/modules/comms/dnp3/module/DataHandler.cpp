#include "DataHandler.hpp"

#include <cstdint>
#include <functional>
#include <string>

#include "bennu/devices/modules/comms/base/CommandInterface.hpp"
#include "bennu/devices/modules/comms/base/CommsModuleCreator.hpp"
#include "bennu/devices/modules/comms/dnp3/module/ClientConnection.hpp"
#include "bennu/distributed/Utils.hpp"
#include "bennu/parsers/Parser.hpp"

namespace bennu {
namespace comms {
namespace dnp3 {

using boost::property_tree::ptree_bad_path;
using boost::property_tree::ptree_error;
using boost::property_tree::ptree;

std::shared_ptr<CommsModule> DataHandler::handleServerTreeData(const ptree& tree, std::shared_ptr<field_device::DataManager> dm)
{
    auto servers = tree.equal_range("dnp3-server");
    for (auto iter = servers.first; iter != servers.second; ++iter)
    {
        std::shared_ptr<Server> server(new Server(dm));
        std::string log = iter->second.get<std::string>("event-logging", "dnp3-server.log");
        server->configureEventLogging(log);
        parseServerTree(server, iter->second);
        return server;
    }

    return std::shared_ptr<comms::CommsModule>();
}

std::shared_ptr<CommsModule> DataHandler::handleClientTreeData(const ptree& tree, std::shared_ptr<field_device::DataManager> dm)
{
    auto clients = tree.equal_range("dnp3-client");
    for (auto iter = clients.first; iter != clients.second; ++iter)
    {
        std::shared_ptr<Client> client(new Client);
        parseClientTree(client, iter->second);
        return client;
    }

    return std::shared_ptr<comms::CommsModule>();
}

void DataHandler::parseServerTree(std::shared_ptr<Server> server, const ptree& tree)
{
    try
    {
        auto binaryInputs = tree.equal_range("binary-input");
        for (auto iter = binaryInputs.first; iter != binaryInputs.second; ++iter)
        {
            std::uint16_t address = iter->second.get<std::uint16_t>("address");
            std::string tag = iter->second.get<std::string>("tag");

            std::string sgvar;
            std::string egvar;
            std::string clazz;

            try {
                sgvar = iter->second.get<std::string>("sgvar");
            } catch (ptree_bad_path&) {}

            try {
                egvar = iter->second.get<std::string>("egvar");
            } catch (ptree_bad_path&) {}

            try {
                clazz = iter->second.get<std::string>("class");
            } catch (ptree_bad_path&) {}

            server->addBinaryInput(address, tag, sgvar, egvar, clazz);
            std::cout << "add dnp3 binary-input " << tag << std::endl;
        }
        auto binaryOutputs = tree.equal_range("binary-output");
        for (auto iter = binaryOutputs.first; iter != binaryOutputs.second; ++iter)
        {
            std::uint16_t address = iter->second.get<std::uint16_t>("address");
            std::string tag = iter->second.get<std::string>("tag");

            bool sbo = iter->second.get<bool>("sbo", false);

            server->addBinaryOutput(address, tag, sbo);
            std::cout << "add dnp3 binary-output " << tag << std::endl;
        }
        auto analogInputs = tree.equal_range("analog-input");
        for (auto iter = analogInputs.first; iter != analogInputs.second; ++iter)
        {
            std::uint16_t address = iter->second.get<std::uint16_t>("address");
            std::string tag = iter->second.get<std::string>("tag");

            std::string sgvar;
            std::string egvar;
            std::string clazz;
            double deadband = 0.0;

            try {
                sgvar = iter->second.get<std::string>("sgvar");
            } catch (ptree_bad_path&) {}

            try {
                egvar = iter->second.get<std::string>("egvar");
            } catch (ptree_bad_path&) {}

            try {
                clazz = iter->second.get<std::string>("class");
            } catch (ptree_bad_path&) {}

            try {
                deadband = iter->second.get<double>("deadband");
            } catch (ptree_bad_path&) {}

            server->addAnalogInput(address, tag, sgvar, egvar, clazz, deadband);
            std::cout << "add dnp3 analog-input " << tag << std::endl;
        }
        auto analogOutputs = tree.equal_range("analog-output");
        for (auto iter = analogOutputs.first; iter != analogOutputs.second; ++iter)
        {
            std::uint16_t address = iter->second.get<std::uint16_t>("address");
            std::string tag = iter->second.get<std::string>("tag");

            bool sbo = iter->second.get<bool>("sbo", false);

            server->addAnalogOutput(address, tag, sbo);
            std::cout << "add dnp3 analog-output " << tag << std::endl;
        }
	std::string endpoint = tree.get<std::string>("endpoint");
        std::uint16_t address = tree.get<uint16_t>("address");
        // Initialize DNP3 server (outstation). Won't start server until enable() is called
        server->init(endpoint, address);
    }
    catch (ptree_bad_path& e)
    {
        std::cerr << "ERROR: Format was incorrect in dnp3 server setup: " << e.what() << std::endl;
    }
    catch (ptree_error& e)
    {
        std::cerr << "ERROR: There was a problem parsing dnp3 server setup: " << e.what() << std::endl;
    }
}

void DataHandler::parseClientTree(std::shared_ptr<Client> client, const ptree &tree)
{
    try
    {
        std::uint16_t address = tree.get<std::uint16_t>("address");
        std::uint32_t scanRate = tree.get<std::uint32_t>("scan-rate");
        auto connections = tree.equal_range("dnp3-connection");
        for (auto itr = connections.first; itr != connections.second; ++itr)
        {
            std::string endpoint = itr->second.get<std::string>("endpoint");
            std::uint16_t serverAddress = itr->second.get<std::uint16_t>("address");
            std::shared_ptr<ClientConnection> connection(new ClientConnection(client, address, endpoint, serverAddress));

            auto binaryInputs = itr->second.equal_range("binary-input");
            for (auto iter = binaryInputs.first; iter != binaryInputs.second; ++iter)
            {
                comms::RegisterDescriptor rd;
                rd.mRegisterType = comms::eStatusReadOnly;
                rd.mRegisterAddress = iter->second.get<ushort>("address");
                rd.mTag = iter->second.get<std::string>("tag");
                client->addTagConnection(rd.mTag, connection);
                connection->addBinary(rd.mTag, rd);
            }
            auto binaryOutputs = itr->second.equal_range("binary-output");
            for (auto iter = binaryOutputs.first; iter != binaryOutputs.second; ++iter)
            {
                comms::RegisterDescriptor rd;
                rd.mRegisterType = comms::eStatusReadWrite;
                rd.mRegisterAddress = iter->second.get<ushort>("address");
                rd.mTag = iter->second.get<std::string>("tag");

                bool sbo = iter->second.get<bool>("sbo", false);

                client->addTagConnection(rd.mTag, connection, sbo);
                connection->addBinary(rd.mTag, rd);
            }
            auto analogInputs = itr->second.equal_range("analog-input");
            for (auto iter = analogInputs.first; iter != analogInputs.second; ++iter)
            {
                comms::RegisterDescriptor rd;
                rd.mRegisterType = comms::eValueReadOnly;
                rd.mRegisterAddress = iter->second.get<ushort>("address");
                rd.mTag = iter->second.get<std::string>("tag");
                client->addTagConnection(rd.mTag, connection);
                connection->addAnalog(rd.mTag, rd);
            }
            auto analogOutputs = itr->second.equal_range("analog-output");
            for (auto iter = analogOutputs.first; iter != analogOutputs.second; ++iter)
            {
                comms::RegisterDescriptor rd;
                rd.mRegisterType = comms::eValueReadWrite;
                rd.mRegisterAddress = iter->second.get<ushort>("address");
                rd.mTag = iter->second.get<std::string>("tag");

                bool sbo = iter->second.get<bool>("sbo", false);

                client->addTagConnection(rd.mTag, connection, sbo);
                connection->addAnalog(rd.mTag, rd);
            }

            // default to global DNP3 client scan rate
            std::uint32_t allClasses = scanRate;
            std::uint32_t class0 = 0;
            std::uint32_t class1 = 0;
            std::uint32_t class2 = 0;
            std::uint32_t class3 = 0;

            try
            {
                const ptree& scanRates = itr->second.get_child("class-scan-rates");

                try
                {
                    allClasses = scanRates.get<std::uint32_t>("all");
                } catch (ptree_bad_path&) {}

                try
                {
                    class0 = scanRates.get<std::uint32_t>("class0");
                } catch (ptree_bad_path&) {}

                try
                {
                    class1 = scanRates.get<std::uint32_t>("class1");
                } catch (ptree_bad_path&) {}

                try
                {
                    class2 = scanRates.get<std::uint32_t>("class2");
                } catch (ptree_bad_path&) {}

                try
                {
                    class3 = scanRates.get<std::uint32_t>("class3");
                } catch (ptree_bad_path&) {}
            } catch (ptree_bad_path&) {}

            connection->start(allClasses, class0, class1, class2, class3);
        }

        if (tree.get_child_optional("command-interface"))
        {
            distributed::Endpoint ep;
            ep.str = tree.get<std::string>("command-interface");
            std::shared_ptr<comms::CommandInterface> ci(new comms::CommandInterface(ep, client));
            client->addCommandInterface(ci);
            ci->start();
        }
    }
    catch (ptree_bad_path& e)
    {
        std::cerr << "Invalid xml in dnp3 FEP's RTU setup file: " + std::string(e.what());
    }
    catch (ptree_error& e)
    {
        std::cerr << "There was a problem parsing dnp3 FEP's rtu setup file: " + std::string(e.what());
    }
}

static bool DataHandlerInit()
{
    using std::placeholders::_1;
    using std::placeholders::_2;
    std::shared_ptr<DataHandler> dh(new DataHandler);
    comms::CommsModuleCreator::the()->addCommsDataHandler(std::bind(&DataHandler::handleServerTreeData, dh, _1, _2));
    comms::CommsModuleCreator::the()->addCommsDataHandler(std::bind(&DataHandler::handleClientTreeData, dh, _1, _2));
    return true;
}

static bool result = DataHandlerInit();

} // namespace dnp3
} // namespace comms
} // namespace bennu
