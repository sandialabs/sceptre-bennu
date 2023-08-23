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
modified to use handle object instead of unique_ptr deleters

modified to use a task_runner instead of pure packaged_task which was not
copyable and causing issues on some compilers
*/
#pragma once

#include "guarded.hpp"
#include "handles.hpp"

#include <atomic>
#include <future>
#include <memory>
#include <shared_mutex>
#include <vector>

namespace gmlc {
namespace libguarded {
    template<class T>
    typename std::add_lvalue_reference<T>::type declref();

    /**
   \headerfile deferred_guarded.hpp <libguarded/deferred_guarded.hpp>

   This templated
   class wraps an object and allows only one thread at a time to
   modify the protected object.

   This class will use std::shared_timed_mutex for the internal locking
   mechanism by default. In C++17, the class std::shared_mutex is available as
   well.

   The shared_handle returned by the various lock methods is moveable
   but not copyable.
*/

    /// container for packaging up different types of tasks
    template<typename T>
    class task_runner {
      public:
        virtual ~task_runner() {}
        virtual void run_task(T&) = 0;
    };
    /// class to contain a void packaged task
    template<typename T>
    class void_runner: public task_runner<T> {
      public:
        void_runner(std::packaged_task<void(T&)>&& tsk): task(std::move(tsk)) {}
        virtual void run_task(T& obj) { task.lock()->operator()(obj); }
        std::future<void> get_future() { return task.lock()->get_future(); }

      private:
        guarded<std::packaged_task<void(T&)>> task;
    };

    /// class to contain a typed return packaged task
    template<typename T, typename Ret>
    class type_runner: public task_runner<T> {
      public:
        type_runner(std::packaged_task<Ret(T&)>&& tsk): task(std::move(tsk)) {}
        virtual void run_task(T& obj) { task.lock()->operator()(obj); }
        std::future<Ret> get_future() { return task.lock()->get_future(); }

      private:
        guarded<std::packaged_task<Ret(T&)>> task;
    };

#ifdef LIBGUARDED_NO_DEFAULT
    template<typename T, typename M>
    class deferred_guarded
#else
    template<typename T, typename M = std::shared_timed_mutex>
    class deferred_guarded
#endif
    {
      public:
        using shared_handle = shared_lock_handle<T, M>;

        template<typename... Us>
        explicit deferred_guarded(Us&&... data);

        template<typename Func>
        void modify_detach(Func&& func);

        template<typename Func>
        auto modify_async(Func func) ->
            typename std::future<decltype(std::declval<Func>()(declref<T>()))>;

        shared_handle lock_shared() const;
        shared_handle try_lock_shared() const;

        template<class Duration>
        shared_handle try_lock_shared_for(const Duration& duration) const;

        template<class TimePoint>
        shared_handle try_lock_shared_until(const TimePoint& timepoint) const;

        /** generate a copy of the protected object
     */
        std::enable_if_t<std::is_copy_constructible<T>::value, T> load() const
        {
            auto handle = lock_shared();
            T newObj(*handle);
            return newObj;
        }

      private:
        void do_pending_writes() const;
        void do_pending_writes_internal() const;
        mutable T m_obj;
        mutable M m_mutex;

        mutable std::atomic<bool> m_pendingWrites;
        mutable guarded<std::vector<std::unique_ptr<task_runner<T>>>> m_pendingList;
    };

    template<typename T, typename M>
    template<typename... Us>
    deferred_guarded<T, M>::deferred_guarded(Us&&... data):
        m_obj(std::forward<Us>(data)...), m_pendingWrites(false)
    {
    }

    template<typename T, typename M>
    template<typename Func>
    void deferred_guarded<T, M>::modify_detach(Func&& func)
    {
        std::unique_lock<M> lock(m_mutex, std::try_to_lock);

        if (lock.owns_lock()) {
            do_pending_writes_internal();
            func(m_obj);
        } else {
            auto vtask = std::unique_ptr<void_runner<T>>(
                new void_runner<T>(std::packaged_task<void(T&)>(std::forward<Func>(func))));
            m_pendingList.lock()->emplace_back(std::move(vtask));
            m_pendingWrites.store(true);
        }
    }

    template<typename Ret, typename Func, typename T>
    auto call_returning_future(Func& func, T& data) ->
        typename std::enable_if<!std::is_same<Ret, void>::value, std::future<Ret>>::type
    {
        std::promise<Ret> promise;

        try {
            promise.set_value(func(data));
        }
        catch (...) {
            promise.set_exception(std::current_exception());
        }

        return promise.get_future();
    }

    template<typename Ret, typename Func, typename T>
    auto call_returning_future(Func& func, T& data) ->
        typename std::enable_if<std::is_same<Ret, void>::value, std::future<Ret>>::type
    {
        std::promise<Ret> promise;

        try {
            func(data);
            promise.set_value();
        }
        catch (...) {
            promise.set_exception(std::current_exception());
        }

        return promise.get_future();
    }

    template<typename Ret, typename T, typename Func>
    auto package_task_void(Func&& func) -> typename std::enable_if<
        std::is_same<Ret, void>::value,
        std::pair<std::unique_ptr<task_runner<T>>, std::future<void>>>::type
    {
        auto vtask = std::unique_ptr<void_runner<T>>(
            new void_runner<T>(std::packaged_task<void(T&)>(std::forward<Func>(func))));
        std::future<void> task_future(vtask->get_future());
        return {std::move(vtask), std::move(task_future)};
    }

    template<typename Ret, typename T, typename Func>
    auto package_task_void(Func&& func) -> typename std::enable_if<
        !std::is_same<Ret, void>::value,
        std::pair<std::unique_ptr<task_runner<T>>, std::future<Ret>>>::type
    {
        auto ttask = std::unique_ptr<type_runner<T, Ret>>(
            new type_runner<T, Ret>(std::packaged_task<Ret(T&)>(std::forward<Func>(func))));
        std::future<Ret> task_future(ttask->get_future());
        return {std::move(ttask), std::move(task_future)};
    }

    template<typename T, typename M>
    template<typename Func>
    auto deferred_guarded<T, M>::modify_async(Func func) ->
        typename std::future<decltype(std::declval<Func>()(declref<T>()))>
    {
        using return_t = decltype(func(m_obj));
        using future_t = std::future<decltype(func(m_obj))>;
        future_t retval;

        std::unique_lock<M> lock(m_mutex, std::try_to_lock);

        if (lock.owns_lock()) {
            do_pending_writes_internal();
            retval = call_returning_future<return_t>(func, m_obj);
        } else {
            auto task_future = package_task_void<return_t, T>(std::move(func));

            retval = std::move(task_future.second);

            m_pendingList.lock()->push_back(std::move(task_future.first));
            m_pendingWrites.store(true);
        }

        return retval;
    }

    template<typename T, typename M>
    void deferred_guarded<T, M>::do_pending_writes() const
    {
        if (m_pendingWrites.load()) {
            std::unique_lock<M> lock(m_mutex, std::try_to_lock);

            if (lock.owns_lock()) {
                do_pending_writes_internal();
            }
        }
    }

    template<typename T, typename M>
    void deferred_guarded<T, M>::do_pending_writes_internal() const
    {
        if (m_pendingWrites.load()) {
            std::vector<std::unique_ptr<task_runner<T>>> localPending;

            m_pendingWrites.store(false);
            swap(localPending, *(m_pendingList.lock()));

            for (auto& f : localPending) {
                f->run_task(m_obj);
            }
        }
    }

    template<typename T, typename M>
    auto deferred_guarded<T, M>::lock_shared() const -> shared_handle
    {
        do_pending_writes();
        return shared_handle(&m_obj, m_mutex);
    }

    template<typename T, typename M>
    auto deferred_guarded<T, M>::try_lock_shared() const -> shared_handle
    {
        do_pending_writes();
        return try_lock_shared_handle(&m_obj, m_mutex);
    }

    template<typename T, typename M>
    template<typename Duration>
    auto deferred_guarded<T, M>::try_lock_shared_for(const Duration& d) const -> shared_handle
    {
        do_pending_writes();
        return try_lock_shared_handle_for(&m_obj, m_mutex, d);
    }

    template<typename T, typename M>
    template<typename TimePoint>
    auto deferred_guarded<T, M>::try_lock_shared_until(const TimePoint& tp) const -> shared_handle
    {
        do_pending_writes();
        return try_lock_shared_handle_until(&m_obj, m_mutex, tp);
    }
} // namespace libguarded

} // namespace gmlc
