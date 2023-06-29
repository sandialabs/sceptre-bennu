#include "DataHandler.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "bennu/devices/modules/comms/base/CommandInterface.hpp"
#include "bennu/devices/modules/comms/base/CommsModuleCreator.hpp"
#include "bennu/devices/modules/comms/iec60870-5/module/ClientConnection.hpp"
#include "bennu/distributed/Utils.hpp"
#include "bennu/parsers/Parser.hpp"

namespace bennu {
namespace comms {
namespace iec60870 {

using boost::property_tree::ptree_bad_path;
using boost::property_tree::ptree_error;
using boost::property_tree::ptree;

std::shared_ptr<CommsModule> DataHandler::handleServerTreeData(const ptree& tree, std::shared_ptr<field_device::DataManager> dm)
{
    auto serverTree = tree.get_child_optional("iec60870-5-104-server");
    if (serverTree)
    {
        std::shared_ptr<Server> server(new Server(dm));
        parseServerTree(server, serverTree.get());
        return server;
    }

    return std::shared_ptr<comms::CommsModule>();
}

std::shared_ptr<CommsModule> DataHandler::handleClientTreeData(const ptree& tree, std::shared_ptr<field_device::DataManager> dm)
{
    auto clientTree = tree.get_child_optional("iec60870-5-104-client");
    if (clientTree)
    {
        std::shared_ptr<Client> client(new Client);
        parseClientTree(client, clientTree.get());
        return client;
    }

    return std::shared_ptr<comms::CommsModule>();
}

void DataHandler::parseServerTree(std::shared_ptr<Server> server, const ptree& tree)
{
    try
    {
        std::uint32_t rPollRate = tree.get<std::uint32_t>("rpoll-rate"); // Server reverse-poll rate
        std::string endpoint = tree.get<std::string>("endpoint");
        std::string log = tree.get<std::string>("event-logging", "iec60870-5-104-server.log");
        server->configureEventLogging(log);
        auto binaryInputs = tree.equal_range("binary-input");
        for (auto iter = binaryInputs.first; iter != binaryInputs.second; ++iter)
        {
            std::uint16_t address = iter->second.get<std::uint16_t>("address");
            std::string tag = iter->second.get<std::string>("tag");
            server->addBinaryInput(address, tag);
            std::cout << "add iec60870-5-104 binary-input " << tag << std::endl;
        }
        auto binaryOutputs = tree.equal_range("binary-output");
        for (auto iter = binaryOutputs.first; iter != binaryOutputs.second; ++iter)
        {
            std::uint16_t address = iter->second.get<std::uint16_t>("address");
            std::string tag = iter->second.get<std::string>("tag");
            server->addBinaryOutput(address, tag);
            std::cout << "add iec60870-5-104 binary-output " << tag << std::endl;
        }
        auto analogInputs = tree.equal_range("analog-input");
        for (auto iter = analogInputs.first; iter != analogInputs.second; ++iter)
        {
            std::uint16_t address = iter->second.get<std::uint16_t>("address");
            std::string tag = iter->second.get<std::string>("tag");
            server->addAnalogInput(address, tag);
            std::cout << "add iec60870-5-104 analog-input " << tag << std::endl;
        }
        auto analogOutputs = tree.equal_range("analog-output");
        for (auto iter = analogOutputs.first; iter != analogOutputs.second; ++iter)
        {
            std::uint16_t address = iter->second.get<std::uint16_t>("address");
            std::string tag = iter->second.get<std::string>("tag");
            server->addAnalogOutput(address, tag);
            std::cout << "add iec60870-5-104 analog-output " << tag << std::endl;
        }
        // Initialize and start 104 server
        //   - Pass server pointer to Server::start() so Server::gServer can be set statically
        //     and used inside static 104 handlers
        server->start(endpoint, server, rPollRate);
    }
    catch (ptree_bad_path& e)
    {
        std::cerr << "ERROR: Format was incorrect in iec60870-5-104 server setup: " << e.what() << std::endl;
    }
    catch (ptree_error& e)
    {
        std::cerr << "ERROR: There was a problem parsing iec60870-5-104 server setup: " << e.what() << std::endl;
    }
}

void DataHandler::parseClientTree(std::shared_ptr<Client> client, const ptree &tree)
{
    try
    {
        auto connections = tree.equal_range("iec60870-5-104-connection");
        for (auto itr = connections.first; itr != connections.second; ++itr)
        {
            std::string serverEndpoint = itr->second.get<std::string>("endpoint");
            std::shared_ptr<ClientConnection> connection(new ClientConnection(serverEndpoint));

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
                client->addTagConnection(rd.mTag, connection);
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
                client->addTagConnection(rd.mTag, connection);
                connection->addAnalog(rd.mTag, rd);
            }

            // Initialize and start 104 client connection
            //   - Pass client connection pointer to ClientConnetion::start() so
            //     ClientConnection::gClientConnection can be set statically
            //     and used inside static 104 handlers
            connection->start(connection);
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
        std::cerr << "Invalid xml in iec60870-5-104 FEP's RTU setup file: " + std::string(e.what());
    }
    catch (ptree_error& e)
    {
        std::cerr << "There was a problem parsing iec60870-5-104 FEP's rtu setup file: " + std::string(e.what());
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

} // namespace iec60870
} // namespace comms
} // namespace bennu
