/*
Copyright (c) 2017-2020,
Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance
for Sustainable Energy, LLC.  See the top-level NOTICE for additional details.
All rights reserved.
SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once

#include "timeRepresentation.hpp"

#include <string>

namespace gmlc {
namespace utilities {
    /** generate a time related unit,
@return a time_units enumeration value
@throw invalid_argument if the string is not a valid unit
*/
    time_units timeUnitsFromString(const std::string& unitString);

    /** get a double representing the time in seconds of a string
units are assumed to be defUnit
@param timeString a string containing a time value
@param defUnit the default units to use if none are given
@return a double representing the time
*/
    double getTimeValue(const std::string& timeString,
                        time_units defUnit = time_units::sec);

    /** generate a time from a string
@details the string can be a double or with units
for example "1.234",  or "1032ms" or "10423425 ns"
@return a helics time generated from the string
@throw invalid_argument if the string is not a valid time
*/
    template<class timeX>
    timeX loadTimeFromString(const std::string& timeString)
    {
        return timeX(getTimeValue(timeString));
    }

    /** generate a time from a string
@details the string can be a double or with units
for example "1.234"  or "1032ms"
@param timeString the string containing the time
@param defUnit the units to apply to a string with no other units specified
@return a helics time generated from the string
@throws invalid_argument if the string is not a valid time
*/
    template<class timeX>
    timeX loadTimeFromString(std::string timeString, time_units defUnit)
    {
        return timeX(getTimeValue(timeString, defUnit));
    }

}  // namespace utilities
}  // namespace gmlc
