/**
   @brief This file contains the get_utc_time function.  It is only compiled if
   the 'POSIX' directive is defined (compiling it only when posix is available).
*/
/* stl includes */
#ifndef __IEC61850_TIME_HPP__
#define __IEC61850_TIME_HPP__

#include <algorithm> // for reverse_copy()
#include <cstdint>
#include <math.h>

/* boost includes */
#include <boost/date_time/posix_time/posix_time.hpp>

#include "bennu/devices/modules/comms/iec61850/protocol/basic-types/basic-types.hpp"

namespace ccss_protocols {
  namespace iec61850 {

    using std::int32_t;
    using std::uint32_t;

    /**
       @brief Return the UTC time of the system

       In the GOOSE protocol the upper four bytes of the time in the header
       are the integer value of the number of seconds since the UNIX epoch.
       The lower four bytes are the number of fractional seconds since the
       UNIX epoch.

       Having the implementation is ok here because it is only included
       in one other file.

       @return System time in UTC format stored in a UTC_TIME_T type
     */
    UTC_TIME_T get_utc_time_posix( void ) {
      UTC_TIME_T utc_time =
        {0xBA, 0xAD, 0xBE, 0xEF, 0xFE, 0xED, 0xFA, 0xCE};

      namespace pt = boost::posix_time;

      // calculate the time-stamp
      pt::ptime now = pt::second_clock::universal_time();
      pt::ptime epoch(boost::gregorian::date(1970, 1, 1));

      pt::time_duration dur = now - epoch;
      int32_t tstamp = dur.total_seconds();

      long fs = dur.fractional_seconds();
      unsigned short fractional_digits = dur.num_fractional_digits();

      uint32_t fractional_tstamp =
        uint32_t((float(fs) / pow(10, fractional_digits)) * 0xFFFF);

      // convert into a UTC_TIME_T type
      std::copy(reinterpret_cast<uint8_t*>(&tstamp),
                reinterpret_cast<uint8_t*>(&tstamp) + 4,
                reinterpret_cast<uint8_t*>(&utc_time)+4);

      std::copy(reinterpret_cast<uint8_t*>(&fractional_tstamp),
                reinterpret_cast<uint8_t*>(&fractional_tstamp) + 4,
                reinterpret_cast<uint8_t*>(&utc_time));
      return utc_time;
    }

  } // namespace iec61850
} // namespace ccss_protocols

#endif /* __IEC61850_TIME_HPP__ */
