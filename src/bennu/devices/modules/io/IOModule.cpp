#include "IOModule.hpp"

#include <iostream>

namespace bennu {
namespace io {

using boost::property_tree::ptree;
using boost::property_tree::ptree_bad_path;
using boost::property_tree::ptree_error;

bool IOModule::handleTreeData(const ptree& tree)
{
    try
    {
        bool success = true;
        distributed::Endpoint ep{tree.get<std::string>("endpoint")};

        auto binaryAddresses = tree.equal_range("binary");
        for (auto binIter = binaryAddresses.first; binIter != binaryAddresses.second; ++binIter)
        {
            std::string id = binIter->second.get<std::string>("id");
            std::string point = binIter->second.get<std::string>("name");
            mDataManager->addExternalData<bool>(id, point);
            std::cout << "add binary " << id << std::endl;
            continue;
        }

        auto analogAddresses = tree.equal_range("analog");
        for (auto analIter = analogAddresses.first; analIter != analogAddresses.second; ++analIter)
        {
            std::string id = analIter->second.get<std::string>("id");
            std::string point = analIter->second.get<std::string>("name");
            mDataManager->addExternalData<double>(id, point);
            std::cout << "add analog " << id << std::endl;
            continue;
        }

        start(ep);

        return success;
    }
    catch (ptree_bad_path& e)
    {
        std::cerr << "ERROR: Format was incorrect IO module XML: " << e.what() << std::endl;
        return false;
    }
    catch (ptree_error& e)
    {
        std::cerr << "ERROR: There was a problem parsing IO module setup: " << e.what() << std::endl;
        return false;
    }
}

} // namespace io
} // namespace bennu
