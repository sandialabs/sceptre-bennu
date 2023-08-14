/***********************************************************************
 *
 * Copyright (c) 2015-2018 Ansel Sermersheim
 * All rights reserved.
 *
 * This file is part of libguarded
 *
 * libguarded is free software, released under the BSD 2-Clause license.
 * For license details refer to LICENSE provided with this project.
 *
 ***********************************************************************/

/*
Copyright (c) 2017-2019,
Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance
for Sustainable Energy, LLC.  See the top-level NOTICE for additional details.
All rights reserved. SPDX-License-Identifier: BSD-3-Clause
*/
/*
this entire file is newly added to the library
*/
#pragma once

#include "handles.hpp"

#include <memory>
#include <mutex>
#include <type_traits>

namespace gmlc {
namespace libguarded {
    /**
   \headerfile guarded.hpp <libguarded/guarded.hpp>

   This templated class wraps an object and allows only one thread at a
   time to access the protected object.

   This class will use std::mutex for the internal locking mechanism by
   default. Other classes which are useful for the mutex type are
   std::recursive_mutex, std::timed_mutex, and
   std::recursive_timed_mutex.

   The handle returned by the various lock methods is moveable but not
   copyable.
*/
    template<typename T, typename M = std::mutex>
    class guarded_opt {
      private:
        using handle = lock_handle<T, M>;

      public:
        explicit guarded_opt(bool enableLocking): enabled(enableLocking){};
        /**
     Construct a guarded object. This constructor will accept any
     number of parameters, all of which are forwarded to the
     constructor of T.
    */
        template<typename... Us>
        guarded_opt(bool enableLocking, Us&&... data);

        /**
     Acquire a handle to the protected object. As a side effect, the
     protected object will be locked from access by any other
     thread. The lock will be automatically released when the handle
     is destroyed.
    */
        handle lock();

        /**
     Attempt to acquire a handle to the protected object. Returns a
     null handle if the object is already locked. As a side effect,
     the protected object will be locked from access by any other
     thread. The lock will be automatically released when the handle
     is destroyed.
    */
        handle try_lock();

        /**
     Attempt to acquire a handle to the protected object. As a side
     effect, the protected object will be locked from access by any
     other thread. The lock will be automatically released when the
     handle is destroyed.

     Returns a null handle if the object is already locked, and does
     not become available for locking before the time duration has
     elapsed.

     Calling this method requires that the underlying mutex type M
     supports the try_lock_for method.  This is not true if M is the
     default std::mutex.
    */
        template<class Duration>
        handle try_lock_for(const Duration& duration);

        /**
     Attempt to acquire a handle to the protected object.  As a side
     effect, the protected object will be locked from access by any other
     thread. The lock will be automatically released when the handle is
     destroyed.

     Returns a null handle if the object is already locked, and does not
     become available for locking before reaching the specified timepoint.

     Calling this method requires that the underlying mutex type M
     supports the try_lock_until method.  This is not true if M is the
     default std::mutex.
    */
        template<class TimePoint>
        handle try_lock_until(const TimePoint& timepoint);

        /** generate a copy of the protected object
     */
        std::conditional_t<std::is_copy_assignable<T>::value, T, void> load()
        {
            std::lock_guard<M> glock(m_mutex);
            auto retObj = std::conditional_t<std::is_copy_assignable<T>::value, T, void>(m_obj);
            return retObj;
        }

        /** store an updated value into the object*/
        template<typename objType>
        void store(objType&& newObj)
        { // uses a forwarding reference
            std::lock_guard<M> glock(m_mutex);
            m_obj = std::forward<objType>(newObj);
        }

        /** store an updated value into the object*/
        template<typename objType>
        guarded_opt& operator=(objType&& newObj)
        { // uses a forwarding reference
            std::lock_guard<M> glock(m_mutex);
            m_obj = std::forward<objType>(newObj);
            return *this;
        }

        /** cast operator so the class can work like T newT=Obj*/
        operator T() const
        {
            std::lock_guard<M> glock(m_mutex);
            return m_obj;
        }

      private:
        T m_obj;
        M m_mutex;
        const bool enabled{true};
    };

    template<typename T, typename M>
    template<typename... Us>
    guarded_opt<T, M>::guarded_opt(bool enableLocking, Us&&... data):
        m_obj(std::forward<Us>(data)...), enabled(enableLocking)
    {
    }

    template<typename T, typename M>
    auto guarded_opt<T, M>::lock() -> handle
    {
        return (enabled) ? handle(&m_obj, m_mutex) : handle(&m_obj, std::unique_lock<M>());
    }

    template<typename T, typename M>
    auto guarded_opt<T, M>::try_lock() -> handle
    {
        return (enabled) ? try_lock_handle(&m_obj, m_mutex) : handle(&m_obj, std::unique_lock<M>());
    }

    template<typename T, typename M>
    template<typename Duration>
    auto guarded_opt<T, M>::try_lock_for(const Duration& d) -> handle
    {
        return (enabled) ? try_lock_handle_for(&m_obj, m_mutex, d) :
                           handle(&m_obj, std::unique_lock<M>());
    }

    template<typename T, typename M>
    template<typename TimePoint>
    auto guarded_opt<T, M>::try_lock_until(const TimePoint& tp) -> handle
    {
        return (enabled) ? try_lock_handle_until(&m_obj, m_mutex, tp) :
                           handle(&m_obj, std::unique_lock<M>());
    }
} // namespace libguarded

} // namespace gmlc
