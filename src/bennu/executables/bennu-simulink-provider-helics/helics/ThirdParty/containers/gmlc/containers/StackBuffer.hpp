/*
Copyright © 2017-2019,
Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance
for Sustainable Energy, LLC.  See the top-level NOTICE for additional details.
All rights reserved. SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace gmlc {
namespace containers {
    struct dataIndex {
        int32_t offset;
        int32_t dataSize;
    };

    /// Helper for size information on the data index
    constexpr int diSize = static_cast<int>(sizeof(dataIndex));

    /** class containing the raw StackBuffer implementation
@details the StackBufferRaw class operates on raw memory
it is given a memory location and uses that for the life of the queue, it does
not own the memory so care must be taken for memory management  It operates on
blocks of raw data
*/
    class StackBufferRaw {
      private:
        unsigned char* origin = nullptr;
        unsigned char* next = nullptr;
        dataIndex* nextIndex = nullptr;
        int dataSize = 0;
        int dataCount = 0;

      public:
        StackBufferRaw(unsigned char* newBlock, int blockSize):
            origin(newBlock), next(newBlock), dataSize(blockSize)
        {
            nextIndex = reinterpret_cast<dataIndex*>(origin + dataSize - diSize);
        }

        void swap(StackBufferRaw& other) noexcept
        {
            std::swap(origin, other.origin);
            std::swap(next, other.next);
            std::swap(dataSize, other.dataSize);
            std::swap(nextIndex, other.nextIndex);
            std::swap(dataCount, other.dataCount);
        }

        int capacity() const { return dataSize; };
        int getCurrentCount() const { return dataCount; }
        bool isSpaceAvailable(int sz) const
        {
            return (dataSize - (next - origin) - (dataCount + 1) * diSize) >=
                static_cast<ptrdiff_t>(sz);
        }
        bool empty() const { return (dataCount == 0); }

        bool push(const unsigned char* block, int blockSize)
        {
            if (blockSize <= 0 || block == nullptr) {
                return false;
            }
            if (!isSpaceAvailable(blockSize)) {
                return false;
            }
            memcpy(next, block, blockSize);
            nextIndex->offset = static_cast<int>(next - origin);
            nextIndex->dataSize = blockSize;
            next += blockSize;
            --nextIndex;
            ++dataCount;
            return true;
        }

        int nextDataSize() const { return (dataCount > 0) ? nextIndex[1].dataSize : 0; }

        int pop(unsigned char* block, int maxSize)
        {
            if (dataCount > 0) {
                int blkSize = nextIndex[1].dataSize;
                if (maxSize >= blkSize) {
                    memcpy(block, origin + nextIndex[1].offset, blkSize);
                    if (nextIndex[1].offset + blkSize == static_cast<int>(next - origin)) {
                        next -= blkSize;
                    }
                    ++nextIndex;
                    --dataCount;
                    if (dataCount == 0) {
                        next = origin;
                        nextIndex = reinterpret_cast<dataIndex*>(origin + dataSize - diSize);
                    }
                    return blkSize;
                }
            }
            return 0;
        }

        /** reverse the order in which the data will be extracted*/
        void reverse()
        {
            if (dataCount > 1) {
                std::reverse(nextIndex + 1, nextIndex + dataCount + 1);
            }
        }
        /** clear all data from the StackBufferRaw*/
        void clear()
        {
            next = origin;
            dataCount = 0;
            nextIndex = reinterpret_cast<dataIndex*>(origin + dataSize - diSize);
        }

      private:
        friend class StackBuffer;
    };

    /** StackBuffer manages memory for a StackBufferRaw and adds some convenience
 * functions */
    class StackBuffer {
      public:
        StackBuffer() noexcept: stack(nullptr, 0) {}
        explicit StackBuffer(int size):
            data(reinterpret_cast<unsigned char*>(std::malloc(size))), statedSize{size},
            actualCapacity{size}, stack(data, size)
        {
        }

        ~StackBuffer()
        {
            if (actualCapacity > 0) {
                free(data);
            }
        }

        StackBuffer(StackBuffer&& sq) noexcept:
            data(sq.data), statedSize(sq.statedSize), actualCapacity(sq.actualCapacity),
            stack(std::move(sq.stack))
        {
            sq.data = nullptr;
            sq.statedSize = 0;
            sq.actualCapacity = 0;
            sq.stack.dataSize = 0;
            sq.stack.origin = nullptr;
            sq.stack.next = nullptr;
            sq.stack.dataCount = 0;
            sq.stack.nextIndex = nullptr;
        }
        StackBuffer(const StackBuffer& sq):
            data{reinterpret_cast<unsigned char*>(std::malloc(sq.statedSize))},
            statedSize{sq.statedSize}, actualCapacity{sq.statedSize}, stack(sq.stack)
        {
            if (data != nullptr && sq.data != nullptr) {
                memcpy(data, sq.data, static_cast<size_t>(statedSize));
                auto offset = stack.next - stack.origin;
                stack.origin = data;
                stack.next = stack.origin + offset;
                stack.nextIndex =
                    reinterpret_cast<dataIndex*>(stack.origin + stack.dataSize - diSize);
                stack.nextIndex -= stack.dataCount;
            } else {
                free(data);
                statedSize = 0;
                actualCapacity = 0;
            }
        }

        StackBuffer& operator=(StackBuffer&& sq) noexcept
        {
            if (data != nullptr) {
                std::free(data);
            }
            data = sq.data;
            statedSize = sq.statedSize;
            actualCapacity = sq.actualCapacity;
            stack = std::move(sq.stack);
            sq.data = nullptr;
            sq.statedSize = 0;
            sq.stack.dataSize = 0;
            sq.stack.origin = nullptr;
            sq.stack.next = nullptr;
            sq.stack.dataCount = 0;
            sq.stack.nextIndex = nullptr;
            return *this;
        }
        StackBuffer& operator=(const StackBuffer& sq)
        {
            stack = sq.stack;
            resizeMemory(sq.statedSize, false);
            std::memcpy(data, sq.data, sq.statedSize);

            auto offset = stack.next - stack.origin;
            stack.origin = data;
            stack.next = stack.origin + offset;
            stack.nextIndex = reinterpret_cast<dataIndex*>(stack.origin + stack.dataSize - diSize);
            stack.nextIndex -= stack.dataCount;
            return *this;
        }

        bool resize(int newsize)
        {
            if (newsize < 0) {
                return false;
            }
            if (newsize == stack.dataSize) {
                return true;
            }
            if (stack.dataCount == 0) {
                resizeMemory(newsize);
                stack = StackBufferRaw(data, newsize);
            } else if (newsize > statedSize) {
                resizeMemory(newsize);
                int indexOffset = stack.dataSize - diSize * stack.dataCount;
                int newOffset = newsize - diSize * stack.dataCount;
                memmove(
                    data + newOffset,
                    data + indexOffset,
                    static_cast<size_t>(diSize) * static_cast<size_t>(stack.dataCount));
                stack.dataSize = newsize;
                stack.origin = data;
                stack.next = stack.origin + newsize;
                stack.nextIndex =
                    reinterpret_cast<dataIndex*>(stack.origin + stack.dataSize - diSize);
                stack.nextIndex -= stack.dataCount;
            } else // smaller size
            {
                int indexOffset = stack.dataSize - diSize * stack.dataCount;
                int newOffset = newsize - diSize * stack.dataCount;
                int dataOffset = static_cast<int>(stack.next - stack.origin);
                if (newsize < dataOffset + diSize * stack.dataCount) {
                    throw(std::runtime_error("unable to resize, current data exceeds new "
                                             "size, please empty stack before resizing"));
                }
                memmove(
                    data + newOffset,
                    data + indexOffset,
                    static_cast<size_t>(diSize) * static_cast<size_t>(stack.dataCount));
                stack.dataSize = newsize;
                stack.origin = data;
                stack.next = stack.origin + newsize;
                stack.nextIndex =
                    reinterpret_cast<dataIndex*>(stack.origin + stack.dataSize - diSize);
                stack.nextIndex -= stack.dataCount;
                statedSize = newsize;
            }
            return (statedSize == newsize);
        }
        /** get the number of data blocks in the stack*/
        int getCurrentCount() const { return stack.getCurrentCount(); }
        /** get the capacity of the stack*/
        int capacity() const { return stack.capacity(); }
        /** get the actual capacity of the underlying data block which could be
     * different than the stack capacity this would be the limit that a resize
     * operation could handle without allocation*/
        int rawBlockCapacity() const { return actualCapacity; }
        /** check if space is available in the stack for a block of size sz*/
        bool isSpaceAvailable(int sz) const { return stack.isSpaceAvailable(sz); }
        /** check if the stack is empty*/
        bool empty() const { return stack.empty(); }

        bool push(const unsigned char* block, int blockSize)
        {
            return stack.push(block, blockSize);
        }

        int nextDataSize() const { return stack.nextDataSize(); }

        int pop(unsigned char* block, int maxSize) { return stack.pop(block, maxSize); }
        /** reverse the order of the stack*/
        void reverse() { stack.reverse(); }
        /** clear all data from a stack*/
        void clear() { stack.clear(); }

        void swap(StackBuffer& other) noexcept
        {
            stack.swap(other.stack);
            std::swap(data, other.data);
            std::swap(actualCapacity, other.actualCapacity);
            std::swap(statedSize, other.statedSize);
        }

      private:
        void resizeMemory(int newsize, bool copyData = true)
        {
            if (newsize == statedSize) {
                return;
            }
            if (newsize > actualCapacity) {
                auto buf = reinterpret_cast<unsigned char*>(malloc(newsize));
                if (actualCapacity > 0) {
                    if (copyData) {
                        memcpy(buf, data, statedSize);
                    }

                    free(data);
                }
                data = buf;
                actualCapacity = newsize;
            }
            statedSize = newsize;
        }

      private:
        unsigned char* data = nullptr; //!< pointer to the memory data block
        int statedSize = 0; //!< the stated size of the memory block
        int actualCapacity = 0; //!< the actual size of the memory block
        StackBufferRaw stack; //!< The actual stack controller
    };

} // namespace containers
} // namespace gmlc
