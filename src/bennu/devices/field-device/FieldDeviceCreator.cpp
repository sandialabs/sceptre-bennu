#include "FieldDeviceCreator.hpp"

#include "bennu/devices/field-device/FieldDevice.hpp"
#include "bennu/parsers/Parser.hpp"

namespace bennu {
namespace field_device {

using boost::property_tree::ptree_bad_path;
using boost::property_tree::ptree_error;

bool FieldDeviceCreator::handleTreeData(const ptree& tree)
{
    try
    {
        std::string name = tree.get<std::string>("name");
        mFieldDevice.reset(new FieldDevice(name));

        return mFieldDevice->handleTreeData(tree);

    }
    catch (ptree_bad_path& e)
    {
        std::cerr << "ERROR: Invalid xml in base field device setup file: " << e.what() << std::endl;
        return false;
    }
    catch (ptree_error& e)
    {
        std::cerr << "ERROR: There was a problem parsing base field device setup file: " << e.what() << std::endl;
        return false;
    }
}

static bool FieldDeviceCreatorInit()
{
    bennu::parsers::Parser::the()->registerTreeDataHandler("field-device", std::bind(&FieldDeviceCreator::handleTreeData, FieldDeviceCreator::the(), std::placeholders::_2));
    return true;
}

static bool result = FieldDeviceCreatorInit();

} // namespace field_device
} // namespace bennu
