/*
Copyright (c) 2017-2020,
Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance
for Sustainable Energy, LLC.  See the top-level NOTICE for additional details.
All rights reserved.
SPDX-License-Identifier: BSD-3-Clause
*/
#include "timeStringOps.hpp"

#include "stringOps.h"

#include <map>

namespace gmlc {
namespace utilities {
    const std::map<std::string, time_units> time_unitstrings{
        {"ps", time_units::ps},
        {"ns", time_units::ns},
        {"us", time_units::us},
        {"ms", time_units::ms},
        {"s", time_units::s},
        {"sec", time_units::sec},
        // don't want empty string to error default is sec
        {"", time_units::sec},
        {"seconds", time_units::sec},
        {"second", time_units::sec},
        {"min", time_units::minutes},
        {"minute", time_units::minutes},
        {"minutes", time_units::minutes},
        {"hr", time_units::hr},
        {"hour", time_units::hr},
        {"hours", time_units::hr},
        {"day", time_units::day},
        {"week", time_units::week},
        {"wk", time_units::week}};

    time_units timeUnitsFromString(const std::string& unitString)
    {
        auto fnd = time_unitstrings.find(unitString);
        if (fnd != time_unitstrings.end()) {
            return fnd->second;
        }
        auto lcUstring = convertToLowerCase(stringOps::trim(unitString));
        fnd = time_unitstrings.find(lcUstring);
        if (fnd != time_unitstrings.end()) {
            return fnd->second;
        }
        throw(std::invalid_argument(std::string("unit ") + unitString +
                                    " not recognized"));
    }

    double getTimeValue(const std::string& timeString, time_units defUnit)
    {
        size_t pos;
        double val = std::stod(timeString, &pos);
        if (pos >= timeString.size()) {
            return val * toSecondMultiplier(defUnit);
        }
        std::string units = stringOps::trim(timeString.substr(pos));
        return val * toSecondMultiplier(timeUnitsFromString(units));
    }

}  // namespace utilities
}  // namespace gmlc
