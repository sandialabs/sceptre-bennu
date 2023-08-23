# ~~~
# Copyright (c) 2017-2019, Battelle Memorial Institute; Lawrence Livermore
# National Security, LLC; Alliance for Sustainable Energy, LLC.
# See the top-level NOTICE for additional details.
# All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
# ~~~

set(
  concurrency_headers
  concurrency/TriggerVariable.hpp
  concurrency/DelayedObjects.hpp
  concurrency/TripWire.hpp
  concurrency/DelayedDestructor.hpp
  concurrency/SearchableObjectHolder.hpp
  concurrency/Barrier.hpp
  concurrency/Latch.hpp
  libguarded/atomic_guarded.hpp
  libguarded/cow_guarded.hpp
  libguarded/deferred_guarded.hpp
  libguarded/guarded.hpp
  libguarded/guarded_opt.hpp
  libguarded/handles.hpp
  libguarded/lr_guarded.hpp
  libguarded/ordered_guarded.hpp
  libguarded/rcu_guarded.hpp
  libguarded/rcu_list.hpp
  libguarded/shared_guarded.hpp
  libguarded/shared_guarded_opt.hpp
  libguarded/staged_guarded.hpp
  
)

set(
  concurrency_sources
  empty.cpp
)


add_library(concurrency STATIC ${concurrency_sources} ${concurrency_headers})

target_link_libraries(concurrency concurrency_base)
target_include_directories(concurrency INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

