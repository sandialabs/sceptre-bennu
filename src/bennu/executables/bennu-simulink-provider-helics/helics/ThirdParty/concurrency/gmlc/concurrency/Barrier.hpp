/*
Copyright (c) 2017-2019,
Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance
for Sustainable Energy, LLC.  See the top-level NOTICE for additional details.
All rights reserved. SPDX-License-Identifier: BSD-3-Clause
*/

#pragma once
#include <condition_variable>
#include <mutex>

namespace gmlc {
namespace concurrency {
    /** class implementing a synchronization barrier*/
    class Barrier {
      public:
        explicit Barrier(size_t count): threshold_(count), count_(count) {}
        /// wait on the barrier
        void wait()
        {
            std::unique_lock<std::mutex> lck(mtx);
            auto lGen = generation_;
            if (--count_ <= 0) {
                generation_++;
                count_ = threshold_;
                cv.notify_all();
            } else {
                cv.wait(lck, [this, lGen] { return lGen != generation_; });
            }
        }
        /// wait on the barrier and remove the object from barrier consideration
        void wait_and_drop()
        {
            std::unique_lock<std::mutex> lck(mtx);
            auto lGen = generation_;
            --threshold_;
            if (--count_ <= 0) {
                generation_++;
                count_ = threshold_;
                cv.notify_all();
            } else {
                cv.wait(lck, [this, lGen] { return lGen != generation_; });
            }
        }

      private:
        std::mutex mtx; //!< mutex for protecting count_
        std::condition_variable cv; //!< associated condition variable
        std::size_t threshold_;
        std::size_t count_;
        std::size_t generation_{0};
    };
} // namespace concurrency
} // namespace gmlc
