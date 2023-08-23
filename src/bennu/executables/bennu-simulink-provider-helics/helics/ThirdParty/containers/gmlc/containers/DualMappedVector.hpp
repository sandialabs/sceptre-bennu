/*
Copyright © 2017-2019,
Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance
for Sustainable Energy, LLC.  See the top-level NOTICE for additional details.
All rights reserved. SPDX-License-Identifier: BSD-3-Clause
*/

#pragma once
#include "MapTraits.hpp"
#include "StableBlockVector.hpp"
#include "optionalDefinition.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace gmlc {
namespace containers {
    /** class to create a searchable vector by defined unique indices.
The result object can be indexed multiple ways both by searching using indices
or by numerical index
*/
    template<
        class VType,
        class searchType1,
        class searchType2,
        reference_stability STABILITY = reference_stability::unstable,
        int BLOCK_ORDER = 5>
    class DualMappedVector {
      public:
        static_assert(
            !std::is_same<searchType1, searchType2>::value,
            "searchType1 and searchType2 cannot be the same type");

        /** insert a new element into the vector
    @param searchValue1 the primary unique index of the vector
    @param searchValue2 the secondary unique index of the vector
    @param data the parameters necessary to create a new object
    @return an optional value that indicates if the insertion was successful and
    if so contain the index of the insertion
    */
        template<typename... Us>
        opt<size_t>
            insert(const searchType1& searchValue1, const searchType2& searchValue2, Us&&... data)
        {
            auto fnd = lookup1.find(searchValue1);
            if (fnd != lookup1.end()) {
                auto fnd2 = lookup2.find(searchValue2);
                if (fnd2 != lookup2.end()) {
                    return {};
                }
            }
            auto index = dataStorage.size();
            dataStorage.emplace_back(std::forward<Us>(data)...);
            lookup1[searchValue1] = index;
            lookup2[searchValue2] = index;
            return index;
        }

        /** insert a new element into the vector
    @param searchValue1 the primary unique index of the vector
   @param data the parameters necessary to create a new object
    @return an optional value that indicates if the insertion was successful and
   if so contain the index of the insertion
    */
        template<typename... Us>
        opt<size_t>
            insert(const searchType1& searchValue1, no_search_type /*searchValue2*/, Us&&... data)
        {
            auto fnd = lookup1.find(searchValue1);
            if (fnd != lookup1.end()) {
                return {};
            }
            auto index = dataStorage.size();
            dataStorage.emplace_back(std::forward<Us>(data)...);
            lookup1.emplace(searchValue1, index);
            return index;
        }

        /** insert a new element into the vector
    @param searchValue2 the secondary unique index of the vector
    @param data the parameters necessary to create a new object
    @return an optional value that indicates if the insertion was successful and
    if so contain the index of the insertion
    */
        template<typename... Us>
        opt<size_t>
            insert(no_search_type /*searchValue1*/, const searchType2& searchValue2, Us&&... data)
        {
            auto fnd = lookup2.find(searchValue2);
            if (fnd != lookup2.end()) {
                return {};
            }
            auto index = dataStorage.size();
            dataStorage.emplace_back(std::forward<Us>(data)...);
            lookup2.emplace(searchValue2, index);
            return index;
        }

        /** insert a new element into the vector
    @param data the parameters necessary to create a new object
    @return an optional value that indicates if the insertion was successful and
    if so contain the index of the insertion*/
        template<typename... Us>
        opt<size_t>
            insert(no_search_type /*searchValue1*/, no_search_type /*searchValue2*/, Us&&... data)
        {
            auto index = dataStorage.size();
            dataStorage.emplace_back(std::forward<Us>(data)...);
            return index;
        }

        /** insert a new element into the vector
    @param searchValue1 the primary unique index of the vector
    @param searchValue2 the secondary unique index of the vector
    @param data the parameters and values necessary to create a new object
    @return the index of the created or assigned value*/
        template<typename... Us>
        size_t insert_or_assign(
            const searchType1& searchValue1,
            const searchType2& searchValue2,
            Us&&... data)
        {
            auto fnd = lookup1.find(searchValue1);
            if (fnd != lookup1.end()) {
                dataStorage[fnd->second] = VType(std::forward<Us>(data)...);
                lookup2[searchValue2] = fnd->second;
                return fnd->second;
            }
            auto index = dataStorage.size();
            dataStorage.emplace_back(std::forward<Us>(data)...);
            lookup1[searchValue1] = index;
            lookup2[searchValue2] = index;
            return index;
        }

        /** insert a new element into the vector
    @param searchValue1 the primary unique index of the vector
    @param data the data required to create a new object
    @return the index of the inserted or assigned value*/
        template<typename... Us>
        size_t insert_or_assign(
            const searchType1& searchValue1,
            no_search_type /*searchValue2*/,
            Us&&... data)
        {
            auto fnd = lookup1.find(searchValue1);
            if (fnd != lookup1.end()) {
                dataStorage[fnd->second] = VType(std::forward<Us>(data)...);
                return fnd->second;
            }
            auto index = dataStorage.size();
            dataStorage.emplace_back(std::forward<Us>(data)...);
            lookup1.emplace(searchValue1, index);
            return index;
        }

        /** insert a new element into the vector
    @param searchValue2 the secondary unique index of the vector
    @param data the parameters and value necessary to create a new object
    @return the index of the inserted or assigned value*/
        template<typename... Us>
        size_t insert_or_assign(
            no_search_type /*searchValue1*/,
            const searchType2& searchValue2,
            Us&&... data)
        {
            auto fnd = lookup2.find(searchValue2);
            if (fnd != lookup2.end()) {
                dataStorage[fnd->second] = VType(std::forward<Us>(data)...);
                return fnd->second;
            }
            auto index = dataStorage.size();
            dataStorage.emplace_back(std::forward<Us>(data)...);
            lookup2.emplace(searchValue2, index);
            return index;
        }
        /** add an additional index term for searching
    this function does not override existing values*/
        bool addSearchTermForIndex(const searchType1& searchValue, size_t index)
        {
            if (index < dataStorage.size()) {
                auto res = lookup1.emplace(searchValue, index);
                return res.second;
            }
            return false;
        }

        /** add an additional index term for searching*/
        auto addSearchTerm(const searchType1& searchValue, const searchType1& existingValue)
        {
            auto fnd = lookup1.find(existingValue);
            if (fnd != lookup1.end()) {
                auto res = lookup1.emplace(searchValue, fnd->second);
                return res.second;
            }
            return false;
        }

        /** add an additional index term for searching*/
        bool addSearchTermForIndex(const searchType2& searchValue, size_t index)
        {
            if (index < dataStorage.size()) {
                auto res = lookup2.emplace(searchValue, index);
                return res.second;
            }
            return false;
        }

        /** add an additional index term for searching*/
        auto addSearchTerm(const searchType2& searchValue, const searchType2& existingValue)
        {
            auto fnd = lookup2.find(existingValue);
            if (fnd != lookup2.end()) {
                auto res = lookup2.emplace(searchValue, fnd->second);
                return res.second;
            }
            return false;
        }

        /** add an additional index term for searching*/
        auto addSearchTerm(const searchType2& searchValue, const searchType1& existingValue)
        {
            auto fnd = lookup1.find(existingValue);
            if (fnd != lookup1.end()) {
                auto res = lookup2.emplace(searchValue, fnd->second);
                return res.second;
            }
            return false;
        }

        /** add an additional index term for searching*/
        auto addSearchTerm(const searchType1& searchValue, const searchType2& existingValue)
        {
            auto fnd = lookup2.find(existingValue);
            if (fnd != lookup2.end()) {
                auto res = lookup1.emplace(searchValue, fnd->second);
                return res.second;
            }
            return false;
        }
        auto find(const searchType1& searchValue) const
        {
            auto fnd = lookup1.find(searchValue);
            if (fnd != lookup1.end()) {
                return dataStorage.begin() + fnd->second;
            }
            return dataStorage.end();
        }

        auto find(const searchType2& searchValue) const
        {
            auto fnd = lookup2.find(searchValue);
            if (fnd != lookup2.end()) {
                return dataStorage.begin() + fnd->second;
            }
            return dataStorage.end();
        }

        auto find(const searchType1& searchValue)
        {
            auto fnd = lookup1.find(searchValue);
            if (fnd != lookup1.end()) {
                return dataStorage.begin() + fnd->second;
            }
            return dataStorage.end();
        }

        auto find(const searchType2& searchValue)
        {
            auto fnd = lookup2.find(searchValue);
            if (fnd != lookup2.end()) {
                return dataStorage.begin() + fnd->second;
            }
            return dataStorage.end();
        }

        void removeIndex(size_t index)
        {
            if (index >= dataStorage.size()) {
                return;
            }
            auto erased = localErase(dataStorage, index);

            std::vector<searchType1> ind1;
            for (auto& searchterm : lookup1) {
                if (erased && searchterm.second > index) {
                    searchterm.second -= 1;
                } else if (searchterm.second == index) {
                    ind1.push_back(searchterm.first);
                }
            }
            std::vector<searchType2> ind2;
            for (auto& searchterm : lookup2) {
                if (erased && searchterm.second > index) {
                    searchterm.second -= 1;
                } else if (searchterm.second == index) {
                    ind2.push_back(searchterm.first);
                }
            }

            for (auto& ind : ind1) {
                auto fnd1 = lookup1.find(ind);
                if (fnd1 != lookup1.end()) {
                    lookup1.erase(fnd1);
                }
            }

            for (auto& ind : ind2) {
                auto fnd2 = lookup2.find(ind);
                if (fnd2 != lookup2.end()) {
                    lookup2.erase(fnd2);
                }
            }
        }

        void remove(const searchType1& search)
        {
            auto el = lookup1.find(search);
            if (el == lookup1.end()) {
                return;
            }
            auto index = el->second;
            removeIndex(index);
        }

        void remove(const searchType2& search)
        {
            auto el = lookup2.find(search);
            if (el == lookup2.end()) {
                return;
            }
            auto index = el->second;
            removeIndex(index);
        }
        VType& operator[](size_t index) { return dataStorage[index]; }

        const VType& operator[](size_t index) const { return dataStorage[index]; }

        VType& back() { return dataStorage.back(); }

        const VType& back() const { return dataStorage.back(); }
        /** apply a function to all the values
    @param F must be a function with signature like void fun(const VType &a);*/
        template<class UnaryFunction>
        void apply(UnaryFunction F)
        {
            std::for_each(dataStorage.begin(), dataStorage.end(), F);
        }

        /** transform all the values
    F must be a function with signature like void VType(const VType &a);*/
        template<class UnaryFunction>
        void transform(UnaryFunction F)
        {
            std::transform(dataStorage.begin(), dataStorage.end(), dataStorage.begin(), F);
        }

        auto begin() const { return dataStorage.cbegin(); }
        auto end() const { return dataStorage.cend(); }

        auto size() const { return dataStorage.size(); }

        void clear()
        {
            dataStorage.clear();
            lookup1.clear();
            lookup2.clear();
        }

      private:
        bool localErase(std::vector<VType>& vect, size_t index)
        {
            vect.erase(vect.begin() + index);
            return true;
        }
        bool localErase(StableBlockVector<VType, BLOCK_ORDER>& vect, size_t index)
        {
            if (index == vect.size() - 1) {
                vect.pop_back();
                return true;
            }
            return false;
        }

      private:
        std::conditional_t<
            STABILITY == reference_stability::unstable,
            std::vector<VType>,
            StableBlockVector<VType, BLOCK_ORDER>>
            dataStorage; //!< primary storage for data
        std::conditional_t<
            is_easily_hashable<searchType1>::value,
            std::unordered_map<searchType1, size_t>,
            std::map<searchType1, size_t>>
            lookup1; //!< map to lookup the index
        std::conditional_t<
            is_easily_hashable<searchType2>::value,
            std::unordered_map<searchType2, size_t>,
            std::map<searchType2, size_t>>
            lookup2; //!< map to lookup the index
    };

} // namespace containers
} // namespace gmlc
