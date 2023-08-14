/*
Copyright © 2017-2019,
Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance
for Sustainable Energy, LLC.  See the top-level NOTICE for additional details.
All rights reserved. SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once

#include "BlockIterator.hpp"

#include <algorithm>
#include <memory>
#include <type_traits>

namespace gmlc {
namespace containers {
    /** class to function as a vector in with push back and pop_back
But the memory is not contiguous and the elements are stable
no erase or insert
@tparam X the type to store in the vector
@tparam N the blocksize exponent store 2^N objects in an allocation block
@tparam Allocator and allocator object to get the blocks*/
    template<typename X, unsigned int N, class Allocator = std::allocator<X>>
    class StableBlockVector {
        static_assert(N < 32, "N should be between 0 and 31 data will allocated in block 2^N");
        static_assert(
            std::is_default_constructible<X>::value,
            " used type must be default constructible");

      public:
        static constexpr unsigned int blockSize{1u << N};
        using iterator = BlockIterator<X, (1u << N), X**>;
        using const_iterator = BlockIterator<const X, (1u << N), const X* const*>;

      private:
        static constexpr unsigned int cntmask{blockSize - 1};

      public:
        /** default constructor*/
        StableBlockVector() noexcept {}

        /** construct with a specified size*/
        explicit StableBlockVector(size_t startSize, const X& init = X{}) noexcept(
            std::is_nothrow_copy_constructible<X>::value):
            csize(startSize),
            dataptr(new X*[std::max((startSize >> N) + 1, size_t{64})]),
            dataSlotsAvailable(std::max(static_cast<int>(startSize >> N) + 1, 64)),
            dataSlotIndex(static_cast<int>(startSize >> N)),
            bsize(static_cast<int>(startSize & cntmask))
        {
            Allocator a;
            if (startSize == 0) {
                dataptr[0] = a.allocate(blockSize);
            } else {
                for (int ii = 0; ii <= dataSlotIndex; ++ii) {
                    dataptr[ii] = a.allocate(blockSize);
                    if (ii < dataSlotIndex) {
                        for (int jj = 0; jj < blockSize; ++jj) {
                            new (&dataptr[ii][jj]) X{init};
                        }
                    } else {
                        for (int jj = 0; jj < bsize; ++jj) {
                            new (&dataptr[ii][jj]) X{init};
                        }
                    }
                }
            }
        }

        StableBlockVector(const StableBlockVector& sbv)
        {
            if (sbv.dataptr != nullptr) {
                dataptr = new X*[sbv.dataSlotsAvailable];
                dataSlotsAvailable = sbv.dataSlotsAvailable;
                dataSlotIndex = -1;
                assign(sbv.begin(), sbv.end());
            }
        }

        StableBlockVector(StableBlockVector&& sbv) noexcept:
            csize(sbv.csize), dataptr(sbv.dataptr), dataSlotsAvailable(sbv.dataSlotsAvailable),
            dataSlotIndex(sbv.dataSlotIndex), bsize(sbv.bsize),
            freeSlotsAvailable(sbv.freeSlotsAvailable), freeIndex(sbv.freeIndex),
            freeblocks(sbv.freeblocks)
        {
            sbv.freeblocks = nullptr;
            sbv.freeSlotsAvailable = 0;
            sbv.dataSlotsAvailable = 0;
            sbv.dataptr = nullptr;
            sbv.bsize = 0;
            sbv.dataSlotIndex = -1;
            sbv.csize = 0;
        }

        StableBlockVector& operator=(const StableBlockVector& sbv)
        {
            assign(sbv.begin(), sbv.end());
            return *this;
        }
        StableBlockVector& operator=(StableBlockVector&& sbv) noexcept
        {
            freeAll();
            csize = sbv.csize;
            dataptr = sbv.dataptr;
            dataSlotsAvailable = sbv.dataSlotsAvailable;
            dataSlotIndex = sbv.dataSlotIndex;
            bsize = sbv.bsize;
            freeSlotsAvailable = sbv.freeSlotsAvailable;
            freeIndex = sbv.freeIndex;
            freeblocks = sbv.freeblocks;

            sbv.freeblocks = nullptr;
            sbv.freeSlotsAvailable = 0;
            sbv.dataSlotsAvailable = 0;
            sbv.dataptr = nullptr;

            return *this;
        }
        /** destructor*/
        ~StableBlockVector() { freeAll(); }

        template<class... Args>
        void emplace_back(Args&&... args)
        {
            blockCheck();
            new (&(dataptr[dataSlotIndex][bsize++])) X(std::forward<Args>(args)...);
            ++csize;
        }

        void push_back(const X& val) noexcept(std::is_nothrow_copy_constructible<X>::value)
        {
            blockCheck();
            new (&(dataptr[dataSlotIndex][bsize++])) X{val};
            ++csize;
        }
        void push_back(X&& val) noexcept(std::is_nothrow_move_constructible<X>::value)
        {
            blockCheck();
            new (&(dataptr[dataSlotIndex][bsize++])) X{std::move(val)};
            ++csize;
        }

        void pop_back() noexcept(std::is_nothrow_destructible<X>::value)
        {
            if (csize == 0) {
                return;
            }
            --csize;
            if (bsize > 0) {
                --bsize;
            } else {
                moveBlocktoAvailable(dataptr[dataSlotIndex--]);
                bsize = blockSize - 1;
            }
            dataptr[dataSlotIndex][bsize].~X();
        }

        template<class InputIt>
        void assign(InputIt first, InputIt last)
        {
            int ii = 0;
            InputIt cur = first;
            while (ii < csize && cur != last) {
                operator[](ii++) = *cur;
                ++cur;
            }
            if (cur == last) {
                while (csize > ii) {
                    pop_back();
                }
            } else {
                while (cur != last) {
                    push_back(*cur);
                    ++cur;
                }
            }
        }

        template<class InputIt>
        void move_assign(InputIt first, InputIt last)
        {
            int ii = 0;
            InputIt cur = first;
            while (ii < csize && cur != last) {
                operator[](ii++) = std::move(*cur);
                ++cur;
            }
            if (cur == last) {
                while (csize > ii) {
                    pop_back();
                }
            } else {
                while (cur != last) {
                    emplace_back(std::move(*cur));
                    ++cur;
                }
            }
        }

        void clear() noexcept(std::is_nothrow_destructible<X>::value)
        {
            if (dataSlotsAvailable <= 0) {
                return;
            }
            for (int jj = bsize - 1; jj >= 0; --jj) { // call destructors on the last block
                dataptr[dataSlotIndex][jj].~X();
            }
            if (dataSlotIndex > 0) {
                moveBlocktoAvailable(dataptr[dataSlotIndex]);
            }
            // start at 1 to leave the first slot in use like the constructor
            // go in reverse order
            for (int ii = dataSlotIndex - 1; ii >= 1; --ii) {
                for (int jj = blockSize - 1; jj >= 0;
                     --jj) { // call destructors on the middle blocks
                    dataptr[ii][jj].~X();
                }
                moveBlocktoAvailable(dataptr[ii]);
            }
            if (dataSlotIndex > 0) {
                for (int jj = blockSize - 1; jj >= 0; --jj) { // call destructors on the first block
                    dataptr[0][jj].~X();
                }
                dataSlotIndex = 0;
            }
            csize = 0;
            bsize = 0;
        }

        void shrink_to_fit() noexcept
        {
            Allocator a;
            for (int ii = 0; ii < freeIndex; ++ii) {
                a.deallocate(freeblocks[ii], blockSize);
            }
            freeIndex = 0;
        }
        /** get the last element*/
        X& back()
        {
            return (bsize == 0) ? dataptr[(csize >> N) - 1][blockSize - 1] :
                                  dataptr[dataSlotIndex][bsize - 1];
        }
        /** const ref to last element*/
        const X& back() const
        {
            return (bsize == 0) ? dataptr[(csize >> N) - 1][blockSize - 1] :
                                  dataptr[dataSlotIndex][bsize - 1];
        }

        X& front() { return dataptr[0][0]; }

        const X& front() const { return dataptr[0][0]; }

        X& operator[](size_t n) { return dataptr[n >> N][n & cntmask]; }

        const X& operator[](size_t n) const { return dataptr[n >> N][n & cntmask]; }

        size_t size() const { return csize; }

        bool empty() const { return (csize == 0); }
        /** define an iterator*/

        iterator begin()
        {
            if (csize == 0) {
                return end();
            }
            return {dataptr, 0};
        }

        iterator end()
        {
            static X* emptyValue{nullptr};
            if (bsize == blockSize) {
                X** ptr = (dataptr != nullptr) ? &(dataptr[dataSlotIndex + 1]) : &emptyValue;
                return {ptr, 0};
            }
            X** ptr = &(dataptr[dataSlotIndex]);
            return {ptr, bsize};
        }

        const_iterator begin() const
        {
            if (csize == 0) {
                return end();
            }
            const X* const* ptr = dataptr;
            return {ptr, 0};
        }

        const_iterator end() const
        {
            static const X* const emptyValue{nullptr};
            if (bsize == blockSize) {
                const X* const* ptr =
                    (dataptr != nullptr) ? &(dataptr[dataSlotIndex + 1]) : &emptyValue;
                return {ptr, 0};
            }
            const X* const* ptr = &(dataptr[dataSlotIndex]);
            return {ptr, bsize};
        }

        const_iterator cend() const { return end(); }

        const_iterator cbegin() const { return begin(); }

      private:
        void freeAll()
        {
            if (dataptr != nullptr) {
                Allocator a;
                for (int jj = bsize - 1; jj >= 0; --jj) { // call destructors on the last block
                    dataptr[dataSlotIndex][jj].~X();
                }
                if (dataSlotIndex > 0) {
                    a.deallocate(dataptr[dataSlotIndex], blockSize);
                }
                // go in reverse order
                for (int ii = dataSlotIndex - 1; ii >= 0; --ii) {
                    for (int jj = blockSize - 1; jj >= 0;
                         --jj) { // call destructors on the middle blocks
                        dataptr[ii][jj].~X();
                    }
                    a.deallocate(dataptr[ii], blockSize);
                }
                if (dataSlotIndex == 0) {
                    a.deallocate(dataptr[0], blockSize);
                }

                for (int ii = 0; ii < freeIndex; ++ii) {
                    a.deallocate(freeblocks[ii], blockSize);
                }
                // delete can handle a nullptr
                delete[] freeblocks;
                delete[] dataptr;
            }
        }
        void blockCheck()
        {
            if (bsize >= static_cast<int>(blockSize)) {
                if (0 == dataSlotsAvailable) {
                    dataptr = new X*[64];
                    dataSlotsAvailable = 64;
                    dataSlotIndex = -1;
                } else if (dataSlotIndex >= dataSlotsAvailable - 1) {
                    auto mem = new X*[static_cast<size_t>(dataSlotsAvailable) * 2];
                    std::copy(dataptr, dataptr + dataSlotsAvailable, mem);
                    delete[] dataptr;
                    dataptr = mem;
                    dataSlotsAvailable *= 2;
                }
                dataptr[++dataSlotIndex] = getNewBlock();
                bsize = 0;
            }
        }

        void moveBlocktoAvailable(X* ptr)
        {
            if (freeIndex >= freeSlotsAvailable) {
                if (0 == freeSlotsAvailable) {
                    freeblocks = new X*[64];
                    freeSlotsAvailable = 64;
                } else {
                    auto mem = new X*[static_cast<size_t>(freeSlotsAvailable) * 2];
                    std::copy(freeblocks, freeblocks + freeSlotsAvailable, mem);
                    delete[] freeblocks;
                    freeblocks = mem;
                    freeSlotsAvailable *= 2;
                }
            }
            freeblocks[freeIndex++] = ptr;
        }

        X* getNewBlock()
        {
            if (freeIndex == 0) {
                Allocator a;
                return a.allocate(blockSize);
            }
            return freeblocks[--freeIndex];
        }

      private:
        size_t csize{0}; // 8
        X** dataptr{nullptr}; // 16
        int dataSlotsAvailable{0}; // 20
        int dataSlotIndex{0}; // 24
        int bsize{blockSize}; // 28
        int freeSlotsAvailable{0}; // 32
        int freeIndex{0}; // 36 +4 byte gap
        X** freeblocks{nullptr}; // 48
    };

} // namespace containers
} // namespace gmlc
