/**
   @brief This file contains the iec61850 session options data-struct ure definition
*/
#ifndef __GOOSE_SESSION_OPTIONS_HPP__
#define __GOOSE_SESSION_OPTIONS_HPP__

/* stl includes */
#include <string>
#include <cstdint>
#include <functional>

#include "bennu/devices/modules/comms/iec61850/protocol/goose/data-set.hpp"

namespace ccss_protocols {
  namespace iec61850 {
    namespace goose {

      using std::string;
      using std::size_t;
      using std::uint8_t;

      typedef std::function
      <void (uint8_t* buffer, size_t size, uint8_t* dst_addr)> low_level_interface_fn_t;

      struct session_opts {
        session_opts() :
          receive_fn(nullptr), transmit_fn(nullptr),
          update_dataset_callback(nullptr) {}
        low_level_interface_fn_t receive_fn;
        low_level_interface_fn_t transmit_fn;

        std::function
        <void (string goCBRef, data_set& ds)> update_dataset_callback;
      };

    } // namespace goose

  } // namespace iec61850

} // namespace ccss_protocols

#endif /* __GOOSE_SESSION_OPTIONS_HPP__ */
