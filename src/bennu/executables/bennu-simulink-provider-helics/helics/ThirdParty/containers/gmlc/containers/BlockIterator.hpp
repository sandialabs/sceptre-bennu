/*
Copyright © 2017-2019,
Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance
for Sustainable Energy, LLC.  See the top-level NOTICE for additional details.
All rights reserved. SPDX-License-Identifier: BSD-3-Clause
*/

#pragma once
#include <cstddef>
#include <iterator>
#include <type_traits>

namespace gmlc {
namespace containers {
    template<typename X, int BLOCKSIZE, typename OUTER>
    /** helper class for iterating through a sequence of blocks*/
    class BlockIterator:
        public std::iterator<
            std::bidirectional_iterator_tag,
            X,
            X,
            const typename std::remove_const<X>::type*,
            const typename std::remove_const<X>::type&> {
      private:
        OUTER vec;
        X* ptr;
        int offset;

      public:
        using constref = const typename std::remove_const<X>::type;

        BlockIterator(OUTER& it, int startoffset):
            vec{it}, ptr{&((*it)[startoffset])}, offset{startoffset}
        {
            static_assert(
                std::is_same<std::remove_reference_t<decltype(*(*it))>, X>::value,
                "OUTER *it must be dereferencable to a type matching X");
        }

        X& operator*() { return *ptr; }
        constref& operator*() const { return *ptr; }
        X* operator->() { return ptr; }
        constref* operator->() const { return ptr; }
        /// explicit bool conversion
        explicit operator bool() const { return (ptr != nullptr); }
        /// Equality operator
        bool operator==(const BlockIterator& it) const
        {
            return ((vec == it.vec) && (offset == it.offset));
        }
        bool operator!=(const BlockIterator& it) const
        {
            return ((offset != it.offset) || (vec != it.vec));
        }

        template<typename X2, typename OUT2>
        bool operator==(const BlockIterator<X2, BLOCKSIZE, OUT2>& it) const
        {
            static_assert(
                std::is_same<std::remove_const_t<X2>, std::remove_const_t<X>>::value,
                "iterators must point to the same type");
            return it.checkEquivalence(vec, offset);
        }

        template<typename X2, typename OUT2>
        bool operator!=(const BlockIterator<X2, BLOCKSIZE, OUT2>& it) const
        {
            return !operator==(it);
        }

        BlockIterator& operator+=(const ptrdiff_t& movement)
        {
            ptr += movement;
            offset += static_cast<int>(movement);
            check();
            return (*this);
        }
        BlockIterator& operator++()
        {
            ++ptr;
            ++offset;
            check();
            return (*this);
        }
        BlockIterator operator++(int)
        {
            auto temp(*this);
            ++(*this);
            return temp;
        }
        BlockIterator operator+(const ptrdiff_t& movement) const
        {
            auto temp(*this);
            temp += movement;
            return temp;
        }
        BlockIterator& operator-=(const ptrdiff_t& movement)
        {
            ptr -= movement;
            offset -= static_cast<int>(movement);
            checkNeg();
            return (*this);
        }
        BlockIterator& operator--()
        {
            --ptr;
            --offset;
            checkNeg();
            return (*this);
        }
        BlockIterator operator--(int)
        {
            auto temp(*this);
            --(*this);
            return temp;
        }
        BlockIterator operator-(const ptrdiff_t& movement) const
        {
            auto temp(*this);
            temp -= movement;
            return temp;
        }
        template<typename OUTER2>
        bool checkEquivalence(OUTER2 testv, int testoffset) const
        {
            return (
                (testv == vec ||
                 ((testv != nullptr && *testv == nullptr) &&
                  (vec != nullptr && *vec == nullptr))) &&
                testoffset == offset);
        }

      private:
        /// check in the forward direction if the offset needs to increment the
        /// block or not
        void check()
        {
            if (offset >= BLOCKSIZE) {
                auto diff = offset - BLOCKSIZE;
                vec += 1 + static_cast<size_t>(diff / BLOCKSIZE);
                offset = diff % BLOCKSIZE;
                ptr = &((*vec)[offset]);
            }
        }
        /// check in the reverse direction if the offset needs to increment the
        /// block or not
        void checkNeg()
        {
            if (offset < 0) {
                auto diff = -offset - 1;
                vec -= (1 + static_cast<size_t>(diff / BLOCKSIZE));
                offset = BLOCKSIZE - 1 - (diff % BLOCKSIZE);
                ptr = &((*vec)[offset]);
            }
        }
    };

} // namespace containers
} // namespace gmlc
