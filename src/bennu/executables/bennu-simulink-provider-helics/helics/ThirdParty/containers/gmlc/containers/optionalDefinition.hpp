/*
Copyright © 2017-2019,
Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance
for Sustainable Energy, LLC.  See the top-level NOTICE for additional details.
All rights reserved. SPDX-License-Identifier: BSD-3-Clause
*/

#pragma once
#ifdef USE_STD_OPTIONAL
#    include <optional>
template<class T>
using opt = std::optional<T>;
#elif defined(USE_BOOST_OPTIONAL)
#    include <boost/optional.hpp>
template<class T>
using opt = boost::optional<T>;
#else
#    include "extra/optional.hpp"
template<class T>
using opt = stx::optional<T>;
#endif
