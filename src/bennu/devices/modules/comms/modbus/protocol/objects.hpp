/**
   @brief objects.hpp defines the necessary data structures and algorithms
   to define Modbus data points
*/
#ifndef __MODBUS_OBJECTS_HPP__
#define __MODBUS_OBJECTS_HPP__

#include <cstdint>
#include <functional>
#include <map>

#include "bennu/devices/modules/comms/modbus/protocol/types.hpp"

namespace bennu {
namespace comms {
namespace modbus {
namespace object {

    template <typename Register_T>
    class Register_Container
    {
    public:
        Register_Container();

        void set_point( std::uint16_t register_address, typename Register_T::value_type value )
        {

        }

        typename Register_T::value_type get_point( std::uint16_t register_address )
        {

        }

        size_t size() const
        {
            return registers.size();
        }

    public:
        typedef std::map<std::uint16_t, typename Register_T::value_type> value_type;
        typedef typename Register_T::value_type register_type;

    private:
        Register_Container::value_type registers;


    };

    // typedefs for the container types inside the register_bank
    //
    //  These make returning the containers for the points easier.  Can just
    //  use the already defined iterator interface of the internal STL
    //  implementation.
    typedef Register_Container<Coil>::value_type Coil_Container_T;
    typedef Register_Container<Discrete_Input>::value_type Discrete_Input_Container_T;
    typedef Register_Container<Holding_Register>::value_type Holding_Register_Container_T;
    typedef Register_Container<Input_Register>::value_type Input_Register_Container_T;


class Register_Container_Collection
{
    public:
        Register_Container_Collection();

        Coil_Container_T get_coil_container() const
        {
            return coil_registers_;
        }

        Discrete_Input_Container_T get_discrete_input_container() const
        {
            return discrete_input_registers_;
        }

        Holding_Register_Container_T get_holding_register_container() const
        {
            return holding_registers_;
        }

        Input_Register_Container_T get_input_register_container() const
        {
            return input_registers_;
        }

    private:
        Coil_Container_T coil_registers_;
        Discrete_Input_Container_T discrete_input_registers_;
        Holding_Register_Container_T holding_registers_;
        Input_Register_Container_T input_registers_;
};

} // namespace object
} // namespace modbus
} // namespace comms
} // namespace ccss_protocols

#endif /* __MODBUS_OBJECTS_HPP__ */
