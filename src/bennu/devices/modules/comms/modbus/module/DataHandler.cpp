#include "DataHandler.hpp"

#include <functional>
#include <string>

#include "bennu/distributed/Utils.hpp"
#include "bennu/devices/modules/comms/base/CommandInterface.hpp"
#include "bennu/devices/modules/comms/base/CommsModuleCreator.hpp"
#include "bennu/devices/modules/comms/modbus/module/ClientConnection.hpp"
#include "bennu/parsers/Parser.hpp"

namespace bennu {
namespace comms {
namespace modbus {

using boost::property_tree::ptree_bad_path;
using boost::property_tree::ptree_error;
using boost::property_tree::ptree;

std::shared_ptr<comms::CommsModule> DataHandler::handleServerTreeData(const ptree& tree, std::shared_ptr<field_device::DataManager> dm)
{
    auto servers = tree.equal_range("modbus-server");
    for (auto iter = servers.first; iter != servers.second; ++iter)
    {
        std::shared_ptr<Server> server(new Server(dm));
        std::string log = iter->second.get<std::string>("event-logging", "modbus-server.log");
        server->configureEventLogging(log);
        parseServerTree(server, iter->second);
        return server;
    }

    return std::shared_ptr<comms::CommsModule>();
}

std::shared_ptr<comms::CommsModule> DataHandler::handleClientTreeData(const ptree& tree, std::shared_ptr<field_device::DataManager> dm)
{
    auto clients = tree.equal_range("modbus-client");
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
        auto coils = tree.equal_range("coil");
        for (auto cIter = coils.first; cIter != coils.second; ++cIter)
        {
            size_t address = cIter->second.get<size_t>("address");
            std::string tag = cIter->second.get<std::string>("tag");
            server->addCoil(address, tag);
        }
        auto discretes = tree.equal_range("discrete-input");
        for (auto diIter = discretes.first; diIter != discretes.second; ++diIter)
        {
            size_t address = diIter->second.get<size_t>("address");
            std::string tag = diIter->second.get<std::string>("tag");
            server->addDiscreteInput(address, tag);
        }
        auto holdings = tree.equal_range("holding-register");
        for (auto hrIter = holdings.first; hrIter != holdings.second; ++hrIter)
        {
            size_t address = hrIter->second.get<size_t>("address");
            std::pair<double, double> range = std::make_pair(2000.0, -1000.0);
            if (hrIter->second.get_child_optional("max-value") && hrIter->second.get_child_optional("min-value"))
            {
                range.first = hrIter->second.get<double>("max-value");
                range.second = hrIter->second.get<double>("min-value");
            }
            std::string tag = hrIter->second.get<std::string>("tag");

            server->addHoldingRegister(address, tag, range);
        }
        auto inputs = tree.equal_range("input-register");
        for (auto irIter = inputs.first; irIter != inputs.second; ++irIter)
        {
            size_t address = irIter->second.get<size_t>("address");
            std::pair<double, double> range = std::make_pair(2000.0, -1000.0);
            if (irIter->second.get_child_optional("max-value") && irIter->second.get_child_optional("min-value"))
            {
                range.first = irIter->second.get<double>("max-value");
                range.second = irIter->second.get<double>("min-value");
            }
            std::string tag = irIter->second.get<std::string>("tag");
            server->addInputRegister(address, tag, range);
        }

        std::string endpoint = tree.get<std::string>("endpoint");
        server->start(endpoint);
    }
    catch (ptree_bad_path& e)
    {
        std::cerr << "ERROR: Format was incorrect in modbus tcp server setup: " << e.what() << std::endl;
        return;
    }
    catch (ptree_error& e)
    {
        std::cerr << "ERROR: There was a problem parsing modbus tcp server setup: " << e.what() << std::endl;
        return;
    }
}

void DataHandler::parseClientTree(std::shared_ptr<Client> client, const ptree &tree)
{
    try
    {
        auto connections = tree.equal_range("modbus-connection");
        for (auto iter = connections.first; iter != connections.second; ++iter)
        {
            std::string endpoint = iter->second.get<std::string>("endpoint");
            uint8_t unitId = iter->second.get<uint8_t>("unit-id", 0);

            std::shared_ptr<ClientConnection> connection(new ClientConnection(endpoint, unitId));

            auto coils = iter->second.equal_range("coil");
            for (auto cIter = coils.first; cIter != coils.second; ++cIter)
            {
                comms::RegisterDescriptor rd;
                rd.mRegisterType = comms::eStatusReadWrite;
                rd.mRegisterAddress = cIter->second.get<size_t>("address");
                rd.mTag = cIter->second.get<std::string>("tag");

                client->addTagConnection(rd.mTag, connection);

                connection->addRegister(rd.mTag, rd);
            }

            auto discretes = iter->second.equal_range("discrete-input");
            for (auto diIter = discretes.first; diIter != discretes.second; ++diIter)
            {
                comms::RegisterDescriptor rd;
                rd.mRegisterType = comms::eStatusReadOnly;
                rd.mRegisterAddress = diIter->second.get<size_t>("address");
                rd.mTag = diIter->second.get<std::string>("tag");

                client->addTagConnection(rd.mTag, connection);

                connection->addRegister(rd.mTag, rd);
            }

            auto holdings = iter->second.equal_range("holding-register");
            for (auto hrIter = holdings.first; hrIter != holdings.second; ++hrIter)
            {
                std::pair<double, double> range = std::make_pair(2000.0, -1000.0);
                if (hrIter->second.get_child_optional("max-value") && hrIter->second.get_child_optional("min-value"))
                {
                    range.first = hrIter->second.get<double>("max-value");
                    range.second = hrIter->second.get<double>("min-value");
                }

                comms::RegisterDescriptor rd;
                rd.mRegisterType = comms::eValueReadWrite;
                rd.mRegisterAddress = hrIter->second.get<size_t>("address");
                rd.mTag = hrIter->second.get<std::string>("tag");

                client->addTagConnection(rd.mTag, connection);

                connection->addRegister(rd.mTag, rd);

                connection->setRange(rd.mRegisterAddress, range);
            }

            auto inputs = iter->second.equal_range("input-register");
            for (auto irIter = inputs.first; irIter != inputs.second; ++irIter)
            {
                std::pair<double, double> range = std::make_pair(2000.0, -1000.0);
                if (irIter->second.get_child_optional("max-value") && irIter->second.get_child_optional("min-value"))
                {
                    range.first = irIter->second.get<double>("max-value");
                    range.second = irIter->second.get<double>("min-value");
                }

                comms::RegisterDescriptor rd;
                rd.mRegisterType = comms::eValueReadOnly;
                rd.mRegisterAddress = irIter->second.get<size_t>("address");
                rd.mTag = irIter->second.get<std::string>("tag");

                client->addTagConnection(rd.mTag, connection);

                connection->addRegister(rd.mTag, rd);
                connection->setRange(rd.mRegisterAddress, range);
            }
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
        std::cerr << "ERROR: Invalid xml in modbus tcp client setup: " + std::string(e.what()) << std::endl;
    }
    catch (ptree_error& e)
    {
        std::cerr << "ERROR: There was a problem parsing modbus tcp client setup: " << std::string(e.what()) << std::endl;
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

} // namespace modbus
} // namespace comms
} // namespace bennu
