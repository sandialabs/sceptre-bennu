# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# Copyright (c) 2017-2019, Battelle Memorial Institute; Lawrence Livermore
# National Security, LLC; Alliance for Sustainable Energy, LLC.
# See the top-level NOTICE for additional details.
# All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

set(
  container_headers
  AirLock.hpp
  SimpleQueue.hpp
  BlockingQueue.hpp
  BlockingPriorityQueue.hpp
  MapTraits.hpp
  MappedVector.hpp
  DualMappedVector.hpp
  MappedPointerVector.hpp
  DualMappedPointerVector.hpp
  CircularBuffer.hpp
  StackBuffer.hpp
  StableBlockVector.hpp
  StableBlockDeque.hpp
  BlockIterator.hpp
  optionalDefinition.hpp
  mapOps.hpp
  WorkQueue.hpp
)

set(
  container_sources
  empty.cpp
)


add_library(containers STATIC ${container_sources} ${container_headers})

target_link_libraries(containers containers_base)
target_include_directories(containers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

