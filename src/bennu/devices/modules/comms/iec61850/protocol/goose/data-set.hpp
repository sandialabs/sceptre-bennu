/**
   @brief This file describes the Data Set Class
   as defined in IEC61850-7-2 (Table 24)
*/
#ifndef __IEC61850_GOOSE_DATA_SET_HPP__
#define __IEC61850_GOOSE_DATA_SET_HPP__

/* stl includes */
#include <cstdint>
#include <string>
#include <vector>

#include "bennu/devices/modules/comms/iec61850/protocol/copy-policies.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/basic-types/basic-types.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/basic-types/basic-types-traits.hpp"
#include "bennu/devices/modules/comms/iec61850/protocol/exception.hpp"

namespace ccss_protocols {
  namespace iec61850 {
    namespace goose {

      using std::string;
      using std::vector;
      using std::uint8_t;

      /**
          Data is stored internally in host byte order.
      */
      class data_set {
      public:
        data_set() {}
        data_set( string name ) : name_(name), reference_(), state_change_(false) {}
        data_set( string name, string ref ) : name_(name), reference_(ref), state_change_(false) {}

        void name( string name ) { name_ = name; }
        string name( void ) { return name_; }

        void reference( string ref ) { reference_ = ref; }
        string reference( void ) { return reference_; }

        /** @brief Add a new attribute to the data-set

            As you add attributes to the data-set, they will be listed
            (top to bottom) in the order that you add them.  Their
            index will correstpond to the order in which they were added.

            For example, if you add a Boolean and then an INT32U, the index
            for the Boolean will be 0 and the index for the INT32U will be 1.
        */
        template <typename IEC_BASIC_T>
        void add_attribute();

        /** @brief Add a new attribute to the data-set and set its default value

            @param value Value of the attribute to be written to the data-set
        */
        template <typename IEC_BASIC_T, typename COPY_POLICY = cpolicy::hbo_copy>
        void add_attribute(typename IEC_BASIC_T::value_type value);

        /** @brief Synonymous with 'add_attribute( value )' but allows the
            user to forego the template parameter.

            If you pass in a constant value to this function without performing
            a static_cast to the expected bit-width, the function will assume
            that you mean a 32-bit integer.
        */
        template <typename T, typename COPY_POLICY = cpolicy::hbo_copy>
        void new_attribute(T value);

        /** @brief Return the value of an attribute in the data-set by index

            This function returns values in host byte order.

            The template parameter passed to this function must be one of the basic
            types defined in basic-types.hpp

            NOTE: This function throws an exception in two cases:
            1) The requested index is not valid
            2) The requested type does not align with the type stored
            in the dataset

            @param index Index in the data-set of the value to return
            @return Value of the attribute at the specified index.
        */
        template<typename IEC_BASIC_T>
        typename IEC_BASIC_T::value_type get_attribute( uint32_t index );

        /** @brief Returns the value of an attribute's type in the data-set by index

            @param index Index in the data-set containing the value type to be returned
            @return String representation of the IEC BASIC TYPE
         */
        std::string get_attribute_type( uint32_t index ) {
          return basic_type_registry_singleton()[data[index].type];
        }

        /** @brief Set the value of an attribute in the data-set by index

            This function stores values in host byte order.

            The template parameter passed to this function must be a C++ type.
            The type passed in must be the value_type of one of the IEC_BASIC_T
            types or the user will get compilation errors.

            If the user is passing in an integer value, she must static_cast that
            value to be of the correct bit-width or the value will be ambiguous
            to the templating system.  For example, don't pass in just "5", instead
            pass in "static_cast<uint32_t>(5)" or "static_cast<uint16_t>(5)"
            depending on what bit-width you want 5 represented.  However, if the
            fails to specify a bit-width using static_cast, the template will match
            a 32-bit integer.

            NOTE: This function throws an exception in two cases:
            1) The requested index is not valid
            2) The requested type does not align with the type stored
            in the dataset

            @param index Index in the data-set of the value to return

            @param value Value to set in the data attribute at 'index'
        */
        template <typename T>
        void set_attribute( uint32_t index, T value);

        /** @brief get the number of data attributes in the data_set */
        size_t num_entries( void ) { return data.size(); }

        /** @brief get the absolute size of the data set (in bytes) */
        size_t size( void ) {
          size_t size = 0;
          for ( basic_value_t bval : data ) {
            size+=2;               // type + length bytes
            size+=bval.val.size(); // add the size of the data
          }
          return size;
        }

      public:
        /** @brief set the state change indicator
            While this is a public function, it is not part of the user's API.
            Calling this function could cause a subscriber to reject data set
            updates when publishing this data-set.
         */
        bool state_change(void) { return state_change_; }
        void state_change__(bool change) { state_change_ = change; }

      public:
        /* Data */
        vector<basic_value_t> data;

      private:
        std::string name_;
        std::string reference_;

        bool state_change_;
      };

      // generic implementation of the 'add_attribute( void )' template
      template <typename IEC_BASIC_T>
      void data_set::add_attribute() {
        basic_value_t entry;
        entry.type = IEC_BASIC_T::tag_value;
        entry.val.resize(IEC_BASIC_T::fixed_length);

        data.push_back( entry );
      }

      // generic implementation of the 'add_attribute( value )' template
      template <typename IEC_BASIC_T, typename COPY_POLICY>
      void data_set::add_attribute(typename IEC_BASIC_T::value_type value) {
        basic_value_t entry;
        entry.type = IEC_BASIC_T::tag_value;

        size_t value_size =
          iec_basic_type_traits<IEC_BASIC_T>::size(value);
        entry.val.resize(value_size);

        COPY_POLICY::apply(value, &entry.val[0]);

        data.push_back( entry );
      }

      // generic implementation of the 'new_attribute( value )' template
      template <typename T, typename COPY_POLICY>
      void data_set::new_attribute(T value) {
        basic_value_t entry;
        entry.type = iec_basic_type<T>::type::tag_value;

        size_t value_size =
          iec_basic_type_traits<typename iec_basic_type<T>::type>::size(value);
        entry.val.resize(value_size);

        COPY_POLICY::apply(value, &entry.val[0]);

        data.push_back( entry );
      }

      template <>
      inline void data_set::new_attribute<char const*>(char const* value)  {
        string value_str(value);
        new_attribute<string>( value_str );
      }

      template <>
      inline void data_set::new_attribute<int>(int value) {
        new_attribute<uint32_t>( static_cast<uint32_t>(value) );
      }

      template <>
      inline void data_set::new_attribute<double>(double value) {
        new_attribute<float>( static_cast<float>(value) );
      }

      // generic implementation of the 'get_attribute()' template
      template<typename IEC_BASIC_T>
      typename IEC_BASIC_T::value_type data_set::get_attribute( uint32_t index ) {
        // ensure index is within a valid range
        if (index > data.size()) {
          throw ccss_protocols::Exception("Requested index is invalid.");
        }

        // ensure the requested type aligns with the dataset index value's type
        if (IEC_BASIC_T::tag_value == data[index].type) {
          return (iec_basic_type_traits<IEC_BASIC_T>::get(data[index]));
        } else {
          throw ccss_protocols::Exception("Requested type does not align with what is in the Data-Set");
        }
      }

      // generic implementation of the 'set_attribute()' template
      template <typename T>
      void data_set::set_attribute( uint32_t index, T value ) {
        // ensure index is within a valid range
        if (index > data.size()) {
          throw ccss_protocols::Exception("Requested index is invalid.");
        }

        // ensure the requested type aligns with the dataset index values's type
        if (iec_basic_type<T>::type::tag_value == data[index].type) {
          ///*reinterpret_cast<T*>(&data[index].val[0]) = value;
          iec_basic_type_traits<typename iec_basic_type<T>::type>::set(data[index], value);
        } else {
          throw ccss_protocols::Exception("Requested type does not align with what is in the Data-Set");
        }

        state_change_ = true;
      }

      template <>
      inline void data_set::set_attribute<char const*>( uint32_t index, char const* value ) {
        string value_str(value);
        set_attribute<string>(index, value_str);

        state_change_ = true;
      }

      template <>
      inline void data_set::set_attribute<double>( uint32_t index, double value) {
        set_attribute<float>( index, static_cast<float>(value) );

        state_change_ = true;
      }

      template <>
      inline void data_set::set_attribute<int>( uint32_t index, int value ) {
        set_attribute<uint32_t>( index, static_cast<uint32_t>(value) );

        state_change_ = true;
      }

    } // namespace goose

  } // namespace iec61850

} // namespace ccss_protocols

#endif /* __IEC61850_GOOSE_DATA_SET_HPP__ */
