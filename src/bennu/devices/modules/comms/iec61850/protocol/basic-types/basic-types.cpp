/**
    @brief This file contains the object instance for the basic type registrar
*/
/* stl includes */
#include <cstdint>
#include <map>
#include <string>

#define TYPEMAP_INSTANTIATION_SINGLETON 1

#include "bennu/devices/modules/comms/iec61850/protocol/basic-types/basic-types.hpp"

namespace ccss_protocols {
  namespace iec61850 {

    std::map<std::uint8_t, std::string>&
    basic_type_registry_singleton()
    {
      static std::map<std::uint8_t, std::string> basic_type_registry;
      return (basic_type_registry);
    }

  } // namespace iec61850

} // namespace ccss_protocols
