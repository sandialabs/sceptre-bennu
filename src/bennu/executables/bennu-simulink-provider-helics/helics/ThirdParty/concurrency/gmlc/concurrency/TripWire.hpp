/*
Copyright (c) 2017-2019,
Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance
for Sustainable Energy, LLC.  See the top-level NOTICE for additional details.
All rights reserved. SPDX-License-Identifier: BSD-3-Clause
*/

#pragma once
#include <atomic>
#include <memory>
#include <stdexcept>
#include <vector>

namespace gmlc {
namespace concurrency {
    /** namespace for the global variable in tripwire*/

    /** the actual tripwire type*/
    using TriplineType = std::shared_ptr<std::atomic<bool>>;

    /** singleton class containing the actual trip line*/
    class TripWire {
      private:
        /** get the tripwire*/
        static TriplineType getLine();
        static TriplineType getIndexedLine(unsigned int index);
        friend class TripWireDetector;
        friend class TripWireTrigger;
    };

    inline TriplineType make_tripline() { return std::make_shared<std::atomic<bool>>(false); }

    inline std::vector<TriplineType> make_triplines(int count)
    {
        std::vector<TriplineType> lines(count);
        for (auto& line : lines) {
            line = std::make_shared<std::atomic<bool>>(false);
        }
        return lines;
    }

#define DECLARE_TRIPLINE()                                                                         \
    namespace gmlc {                                                                               \
        namespace concurrency {                                                                    \
            TriplineType TripWire::getLine()                                                       \
            {                                                                                      \
                static TriplineType staticline = make_tripline();                                  \
                return staticline;                                                                 \
            }                                                                                      \
        } /*namespace concurrency*/                                                                \
    } /*namespace gmlc */

#define DECLARE_INDEXED_TRIPLINES(COUNT)                                                           \
    namespace gmlc {                                                                               \
        namespace concurrency {                                                                    \
            TriplineType TripWire::getIndexedLine(unsigned int index)                              \
            {                                                                                      \
                static const std::vector<TriplineType> triplines = make_triplines(COUNT);          \
                return (index < COUNT) ?                                                           \
                    triplines[index] :                                                             \
                    (throw(std::out_of_range("index exceeds the number of generated lines")),      \
                     nullptr);                                                                     \
            }                                                                                      \
        } /*namespace concurrency*/                                                                \
    } /*namespace gmlc */

    /** class to check if a trip line was tripped*/
    class TripWireDetector {
      public:
        TripWireDetector(): lineDetector(TripWire::getLine()) {}
        explicit TripWireDetector(unsigned int index): lineDetector(TripWire::getIndexedLine(index))
        {
        }
        explicit TripWireDetector(TriplineType line): lineDetector(std::move(line)) {}
        /** check if the line was tripped*/
        bool isTripped() const noexcept { return lineDetector->load(std::memory_order_acquire); }

      private:
        std::shared_ptr<const std::atomic<bool>> lineDetector; //!< const pointer to the tripwire
    };

    /** class to trigger a tripline on destruction */
    class TripWireTrigger {
      public:
        /** default constructor*/
        TripWireTrigger(): lineTrigger(TripWire::getLine()) {}
        explicit TripWireTrigger(unsigned int index): lineTrigger(TripWire::getIndexedLine(index))
        {
        }
        explicit TripWireTrigger(TriplineType line): lineTrigger(std::move(line)) {}
        /** destructor*/
        ~TripWireTrigger() { lineTrigger->store(true, std::memory_order_release); }
        /** move constructor*/
        TripWireTrigger(TripWireTrigger&& twt) = default;
        /** deleted copy constructor*/
        TripWireTrigger(const TripWireTrigger& twt) = delete;
        /** move assignment*/
        TripWireTrigger& operator=(TripWireTrigger&& twt) = default;
        /** deleted copy assignment*/
        TripWireTrigger& operator=(const TripWireTrigger& twt) = delete;

      private:
        TriplineType lineTrigger; //!< the tripwire
    };
} // namespace concurrency
} // namespace gmlc
