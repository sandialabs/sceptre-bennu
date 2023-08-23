/*
Copyright © 2017-2019,
Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance
for Sustainable Energy, LLC.  See the top-level NOTICE for additional details.
All rights reserved. SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once

#include "optionalDefinition.hpp"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <type_traits>
#include <vector>

namespace gmlc {
namespace containers {
    /** class for very simple thread safe queue
@details  uses two vectors for the operations,  once the pull vector is empty it
swaps the vectors and reverses it so it can pop from the back as well as an
atomic flag indicating the queue is empty
@tparam X the base class of the queue
@tparam MUTEX the type of lock to use*/
    template<class X, class MUTEX = std::mutex>
    class SimpleQueue {
      private:
        mutable MUTEX m_pushLock; //!< lock for operations on the pushElements vector
        mutable MUTEX m_pullLock; //!< lock for elements on the pullLock vector
        std::vector<X> pushElements; //!< vector of elements being added
        std::vector<X> pullElements; //!< vector of elements waiting extraction
        std::atomic<bool> queueEmptyFlag{true}; //!< flag indicating the queue is Empty
      public:
        /** default constructor */
        SimpleQueue() = default;
        /** destructor*/
        ~SimpleQueue()
        {
            // these locks are primarily for memory synchronization multiple access
            // in the destructor would be a bad thing
            std::lock_guard<MUTEX> pullLock(m_pullLock); // first pullLock
            std::lock_guard<MUTEX> pushLock(m_pushLock); // second pushLock
            /** clear the elements as part of the destruction while the locks are
         * engaged*/
            pushElements.clear();
            pullElements.clear();
        }
        /** constructor with a reservation size
    @param capacity  the initial storage capacity of the queue*/
        explicit SimpleQueue(size_t capacity)
        { // don't need to lock since we aren't out of the constructor yet
            pushElements.reserve(capacity);
            pullElements.reserve(capacity);
        }
        /** enable the move constructor not the copy constructor*/
        SimpleQueue(SimpleQueue&& sq) noexcept:
            pushElements(std::move(sq.pushElements)), pullElements(std::move(sq.pullElements))
        {
            queueEmptyFlag = pullElements.empty();
        }

        /** enable the move assignment not the copy assignment*/
        SimpleQueue& operator=(SimpleQueue&& sq) noexcept
        {
            std::lock_guard<MUTEX> pullLock(m_pullLock); // first pullLock
            std::lock_guard<MUTEX> pushLock(m_pushLock); // second pushLock
            pushElements = std::move(sq.pushElements);
            pullElements = std::move(sq.pullElements);
            queueEmptyFlag = pullElements.empty();
            return *this;
        }
        /** DISABLE_COPY_AND_ASSIGN */
        SimpleQueue(const SimpleQueue&) = delete;
        SimpleQueue& operator=(const SimpleQueue&) = delete;

        /** check whether there are any elements in the queue
    because this is meant for multi threaded applications this may or may not
    have any meaning depending on the number of consumers
    */
        bool empty() const
        {
            std::lock_guard<MUTEX> pullLock(m_pullLock); // first pullLock
            return pullElements.empty();
        }
        /** get the current size of the queue*/
        size_t size() const
        {
            std::lock_guard<MUTEX> pullLock(m_pullLock); // first pullLock
            std::lock_guard<MUTEX> pushLock(m_pushLock); // second pushLock
            return pullElements.size() + pushElements.size();
        }
        /** clear the queue*/
        void clear()
        {
            std::lock_guard<MUTEX> pullLock(m_pullLock); // first pullLock
            std::lock_guard<MUTEX> pushLock(m_pushLock); // second pushLock
            pullElements.clear();
            pushElements.clear();
            queueEmptyFlag = true;
        }
        /** set the capacity of the queue
    actually double the requested the size will be reserved due to the use of
    two vectors internally
    @param capacity  the capacity to reserve
    */
        void reserve(size_t capacity)
        {
            std::lock_guard<MUTEX> pullLock(m_pullLock); // first pullLock
            std::lock_guard<MUTEX> pushLock(m_pushLock); // second pushLock
            pullElements.reserve(capacity);
            pushElements.reserve(capacity);
        }

        /** push an element onto the queue
    val the value to push on the queue
    */
        template<class Z>
        void push(Z&& val) // forwarding reference
        {
            std::unique_lock<MUTEX> pushLock(m_pushLock); // only one lock on this branch
            if (pushElements.empty()) {
                bool expEmpty = true;
                if (queueEmptyFlag.compare_exchange_strong(expEmpty, false)) {
                    // release the push lock
                    pushLock.unlock();
                    std::unique_lock<MUTEX> pullLock(m_pullLock); // first pullLock
                    queueEmptyFlag = false; // set the flag to false again just in case
                    if (pullElements.empty()) {
                        pullElements.push_back(std::forward<Z>(val));
                        return;
                    }
                    // reengage the push lock so we can push next
                    // LCOV_EXCL_START
                    pushLock.lock();
                    // LCOV_EXCL_STOP
                }
            }
            pushElements.push_back(std::forward<Z>(val));
        }

        /** push a vector onto the queue
        val the vector of values to push on the queue
        */
        void pushVector(const std::vector<X>& val) // universal reference
        {
            std::unique_lock<MUTEX> pushLock(m_pushLock); // only one lock on this branch
            if (pushElements.empty()) {
                bool expEmpty = true;
                if (queueEmptyFlag.compare_exchange_strong(expEmpty, false)) {
                    // release the push lock
                    pushLock.unlock();
                    std::unique_lock<MUTEX> pullLock(m_pullLock); // first pullLock
                    queueEmptyFlag = false; // set the flag to false again just in case
                    if (pullElements.empty()) {
                        pullElements.insert(pullElements.end(), val.rbegin(), val.rend());
                        return;
                    }
                    // reengage the push lock so we can push next
                    // LCOV_EXCL_START
                    pushLock.lock();
                    // LCOV_EXCL_STOP
                }
            }
            pushElements.insert(pushElements.end(), val.begin(), val.end());
        }

        /** emplace an element onto the queue
    val the value to emplace on the queue
    */
        template<class... Args>
        void emplace(Args&&... args)
        {
            std::unique_lock<MUTEX> pushLock(m_pushLock); // only one lock on this branch
            if (pushElements.empty()) {
                bool expEmpty = true;
                if (queueEmptyFlag.compare_exchange_strong(expEmpty, false)) {
                    // release the push lock
                    pushLock.unlock();
                    std::unique_lock<MUTEX> pullLock(m_pullLock); // first pullLock
                    queueEmptyFlag = false;
                    if (pullElements.empty()) {
                        pullElements.emplace_back(std::forward<Args>(args)...);
                        return;
                    }
                    // reengage the push lock so we can push next
                    // LCOV_EXCL_START
                    pushLock.lock();
                    // LCOV_EXCL_STOP
                }
            }
            pushElements.emplace_back(std::forward<Args>(args)...);
        }
        /*make sure there is no path to lock the push first then the pull second
    as that would be a race condition
    we either lock the pull first then the push,  or lock just one at a time
    otherwise we have a potential deadlock condition
    */
        /** extract the first element from the queue
    @return an empty optional if there is no element otherwise the optional will
    contain a value
    */
        opt<X> pop()
        {
            std::lock_guard<MUTEX> pullLock(m_pullLock); // first pullLock
            checkPullandSwap();
            if (pullElements.empty()) {
                return {};
            }
            opt<X> val(
                std::move(pullElements.back())); // do it this way to allow moveable only types
            pullElements.pop_back();
            checkPullandSwap();
            return val;
        }

        /** try to peek at an object without popping it from the stack
    @details only available for copy assignable objects
    @return an optional object with an object of type T if available
    */
        template<typename = std::enable_if<std::is_copy_assignable<X>::value>>
        opt<X> peek() const
        {
            std::lock_guard<MUTEX> lock(m_pullLock);

            if (pullElements.empty()) {
                return {};
            }

            auto t = pullElements.back();
            return t;
        }

      private:
        /** If pullElements is empty check push and swap and reverse if needed
      assumes pullLock is active and pushLock is not
      */
        void checkPullandSwap()
        {
            if (pullElements.empty()) {
                std::unique_lock<MUTEX> pushLock(m_pushLock); // second pushLock
                if (!pushElements.empty()) { // this is the potential for slow operations
                    std::swap(pushElements, pullElements);
                    // we can free the push function to accept more elements after
                    // the swap call;
                    pushLock.unlock();
                    std::reverse(pullElements.begin(), pullElements.end());
                } else {
                    queueEmptyFlag = true;
                }
            }
        }
    };

} // namespace containers
} // namespace gmlc
