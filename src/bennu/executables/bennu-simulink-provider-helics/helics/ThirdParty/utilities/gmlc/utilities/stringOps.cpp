/*
 * LLNS Copyright Start
 * Copyright (c) 2017, Lawrence Livermore National Security
 * This work was performed under the auspices of the U.S. Department
 * of Energy by Lawrence Livermore National Laboratory in part under
 * Contract W-7405-Eng-48 and in part under Contract DE-AC52-07NA27344.
 * Produced at the Lawrence Livermore National Laboratory.
 * All rights reserved.
 * For details, see the LICENSE file.
 * LLNS Copyright End
 */

#include "stringOps.h"

#include "generic_string_ops.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <random>
#include <utility>

namespace gmlc {
namespace utilities {
    static auto lower = [](char x) -> char {
        return (x >= 'A' && x <= 'Z') ? (x - ('Z' - 'z')) : x;
    };

    static auto upper = [](char x) -> char {
        return (x >= 'a' && x <= 'z') ? (x + ('Z' - 'z')) : x;
    };

    std::string convertToLowerCase(const std::string& input)
    {
        std::string out(input);
        std::transform(out.begin(), out.end(), out.begin(), lower);
        return out;
    }

    std::string convertToUpperCase(const std::string& input)
    {
        std::string out(input);
        std::transform(out.begin(), out.end(), out.begin(), upper);
        return out;
    }

    void makeLowerCase(std::string& input)
    {
        std::transform(input.begin(), input.end(), input.begin(), lower);
    }

    void makeUpperCase(std::string& input)
    {
        std::transform(input.begin(), input.end(), input.begin(), upper);
    }

    namespace stringOps {
        stringVector splitline(const std::string& line,
                               const std::string& delimiters,
                               delimiter_compression compression)
        {
            return generalized_string_split(
                line, delimiters, (compression == delimiter_compression::on));
        }

        stringVector splitline(const std::string& line, char del)
        {
            return generalized_string_split(line, std::string{1, del}, false);
        }

        void splitline(const std::string& line,
                       stringVector& strVec,
                       const std::string& delimiters,
                       delimiter_compression compression)
        {
            strVec = generalized_string_split(
                line, delimiters, (compression == delimiter_compression::on));
        }

        void splitline(const std::string& line, stringVector& strVec, char del)
        {
            strVec = generalized_string_split(line, std::string{1, del}, false);
        }

        static const auto pmap = pairMapper();

        stringVector splitlineQuotes(const std::string& line,
                                     const std::string& delimiters,
                                     const std::string& quoteChars,
                                     delimiter_compression compression)
        {
            bool compress = (compression == delimiter_compression::on);
            return generalized_section_splitting(
                line, delimiters, quoteChars, pmap, compress);
        }

        stringVector splitlineBracket(const std::string& line,
                                      const std::string& delimiters,
                                      const std::string& bracketChars,
                                      delimiter_compression compression)
        {
            bool compress = (compression == delimiter_compression::on);
            return generalized_section_splitting(
                line, delimiters, bracketChars, pmap, compress);
        }

        void trimString(std::string& input, const std::string& whitespace)
        {
            input.erase(input.find_last_not_of(whitespace) + 1);
            input.erase(0, input.find_first_not_of(whitespace));
        }

        std::string trim(const std::string& input,
                         const std::string& whitespace)
        {
            const auto strStart = input.find_first_not_of(whitespace);
            if (strStart == std::string::npos) {
                return std::string();  // no content
            }

            const auto strEnd = input.find_last_not_of(whitespace);

            return input.substr(strStart, strEnd - strStart + 1);
        }

        void trim(stringVector& input, const std::string& whitespace)
        {
            for (auto& str : input) {
                trimString(str, whitespace);
            }
        }

        static const std::string digits("0123456789");
        int trailingStringInt(const std::string& input,
                              std::string& output,
                              int defNum) noexcept
        {
            if ((input.empty()) || (isdigit(input.back()) == 0)) {
                output = input;
                return defNum;
            }
            int num = defNum;
            auto pos1 = input.find_last_not_of(digits);
            if (pos1 ==
                std::string::npos)  // in case the whole thing is a number
            {
                if (input.length() <= 10) {
                    output.clear();
                    num = std::stol(input);
                    return num;
                }
                pos1 = input.length() - 10;
            }

            size_t length = input.length();
            if (pos1 == length - 2) {
                num = input.back() - '0';
            } else if (length <= 10 || pos1 >= length - 10) {
                num = std::stol(input.substr(pos1 + 1));
            } else {
                num = std::stol(input.substr(length - 9));
                pos1 = length - 10;
            }

            if (input[pos1] == '_' || input[pos1] == '#') {
                output = input.substr(0, pos1);
            } else {
                output = input.substr(0, pos1 + 1);
            }

            return num;
        }

        int trailingStringInt(const std::string& input, int defNum) noexcept
        {
            if ((input.empty()) || (isdigit(input.back()) == 0)) {
                return defNum;
            }

            auto pos1 = input.find_last_not_of(digits);
            if (pos1 ==
                std::string::npos)  // in case the whole thing is a number
            {
                if (input.length() <= 10) {
                    return std::stol(input);
                }
                pos1 = input.length() - 10;
            }

            size_t length = input.length();
            if (pos1 == length - 2) {
                return input.back() - '0';
            }
            if ((length <= 10) || (pos1 >= length - 10)) {
                return std::stol(input.substr(pos1 + 1));
            }
            return std::stol(input.substr(length - 9));
        }

        static const std::string quoteChars(R"raw("'`)raw");

        std::string removeQuotes(const std::string& str)
        {
            auto newString = trim(str);
            if (!newString.empty()) {
                if ((newString.front() == '\"') ||
                    (newString.front() == '\'') || (newString.front() == '`')) {
                    if (newString.back() == newString.front()) {
                        newString.pop_back();
                        newString.erase(0, 1);
                    }
                }
            }
            return newString;
        }

        std::string removeBrackets(const std::string& str)
        {
            std::string newString = trim(str);
            if (!newString.empty()) {
                if ((newString.front() == '[') || (newString.front() == '(') ||
                    (newString.front() == '{') || (newString.front() == '<')) {
                    if (newString.back() == pmap[newString.front()]) {
                        newString.pop_back();
                        newString.erase(0, 1);
                    }
                }
            }
            return newString;
        }

        std::string getTailString(const std::string& input, char sep) noexcept
        {
            auto tc = input.find_last_of(sep);
            std::string ret =
                (tc == std::string::npos) ? input : input.substr(tc + 1);
            return ret;
        }

        std::string getTailString(const std::string& input,
                                  const std::string& sep) noexcept
        {
            auto tc = input.rfind(sep);
            std::string ret = (tc == std::string::npos) ?
                input :
                input.substr(tc + sep.size());
            return ret;
        }

        std::string getTailString_any(const std::string& input,
                                      const std::string& sep) noexcept
        {
            auto tc = input.find_last_of(sep);
            std::string ret =
                (tc == std::string::npos) ? input : input.substr(tc + 1);
            return ret;
        }

        int findCloseStringMatch(const stringVector& testStrings,
                                 const stringVector& iStrings,
                                 string_match_type matchType)
        {
            std::string lct;  // lower case test string
            std::string lcis;  // lower case input string
            stringVector lciStrings = iStrings;
            // make all the input strings lower case
            for (auto& st : lciStrings) {
                makeLowerCase(st);
            }
            for (auto& ts : testStrings) {
                lct = convertToLowerCase(ts);
                for (int kk = 0; kk < static_cast<int>(lciStrings.size());
                     ++kk) {
                    lcis = lciStrings[kk];
                    switch (matchType) {
                        case string_match_type::exact:
                            if (lcis == lct) {
                                return kk;
                            }
                            break;
                        case string_match_type::begin:
                            if (lcis.compare(0, lct.length(), lct) == 0) {
                                return kk;
                            }
                            break;
                        case string_match_type::end:
                            if (lct.length() > lcis.length()) {
                                continue;
                            }
                            if (lcis.compare(lcis.length() - lct.length(),
                                             lct.length(),
                                             lct) == 0) {
                                return kk;
                            }
                            break;
                        case string_match_type::close:
                            if (lct.length() == 1)  // special case
                            {  // we are checking if the single character is
                               // isolated from
                                // other other alphanumeric characters
                                auto bf = lcis.find(lct);
                                while (bf != std::string::npos) {
                                    if (bf == 0) {
                                        if ((isspace(lcis[bf + 1]) != 0) ||
                                            (ispunct(lcis[bf + 1]) != 0)) {
                                            return kk;
                                        }
                                    } else if (bf == lcis.length() - 1) {
                                        if ((isspace(lcis[bf - 1]) != 0) ||
                                            (ispunct(lcis[bf - 1]) != 0)) {
                                            return kk;
                                        }
                                    } else {
                                        if ((isspace(lcis[bf - 1]) != 0) ||
                                            (ispunct(lcis[bf - 1]) != 0)) {
                                            if ((isspace(lcis[bf + 1]) != 0) ||
                                                (ispunct(lcis[bf + 1]) != 0)) {
                                                return kk;
                                            }
                                        }
                                    }
                                    bf = lcis.find(lct, bf + 1);
                                }
                            } else {
                                auto bf = lcis.find(lct);
                                if (bf != std::string::npos) {
                                    return kk;
                                }
                                auto nstr = removeChar(lct, '_');
                                if (lcis == nstr) {
                                    return kk;
                                }
                                auto nstr2 = removeChar(lcis, '_');
                                if ((lct == nstr2) || (nstr == nstr2)) {
                                    return kk;
                                }
                            }
                            break;
                    }
                }
            }
            return -1;
        }

        std::string removeChars(const std::string& source,
                                const std::string& remchars)
        {
            std::string result;
            result.reserve(source.length());
            std::remove_copy_if(source.begin(),
                                source.end(),
                                std::back_inserter(result),
                                [remchars](char in) {
                                    return (std::find(remchars.begin(),
                                                      remchars.end(),
                                                      in) != remchars.end());
                                });
            return result;
        }

        std::string removeChar(const std::string& source, char remchar)
        {
            std::string result;
            result.reserve(source.length());
            std::remove_copy(source.begin(),
                             source.end(),
                             std::back_inserter(result),
                             remchar);
            return result;
        }

        std::string characterReplace(const std::string& source,
                                     char key,
                                     const std::string& repStr)
        {
            std::string result;
            result.reserve(source.length());
            for (auto sc : source) {
                if (sc == key) {
                    result += repStr;
                } else {
                    result.push_back(sc);
                }
            }
            return result;
        }

        std::string xmlCharacterCodeReplace(std::string str)
        {
            std::string out = std::move(str);
            auto tt = out.find("&gt;");
            while (tt != std::string::npos) {
                out.replace(tt, 4, ">");
                tt = out.find("&gt;", tt + 1);
            }
            tt = out.find("&lt;");
            while (tt != std::string::npos) {
                out.replace(tt, 4, "<");
                tt = out.find("&lt;", tt + 1);
            }
            tt = out.find("&quot;");
            while (tt != std::string::npos) {
                out.replace(tt, 6, "\"");
                tt = out.find("&quot;", tt + 1);
            }
            tt = out.find("&apos;");
            while (tt != std::string::npos) {
                out.replace(tt, 6, "'");
                tt = out.find("&apos;", tt + 1);
            }
            //&amp; is last so it can't trigger other sequences
            tt = out.find("&amp;");
            while (tt != std::string::npos) {
                out.replace(tt, 5, "&");
                tt = out.find("&amp;", tt + 1);
            }
            return out;
        }
    }  // namespace stringOps

    std::string randomString(std::string::size_type length)
    {
        static constexpr auto chars =
            "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

#ifndef __apple_build_version__
        thread_local static std::mt19937 rg{
            std::random_device{}() +
            static_cast<uint32_t>(
                reinterpret_cast<uint64_t>(&length) &  // NOLINT
                0xFFFFFFFFU)};
        thread_local static std::uniform_int_distribution<
            std::string::size_type>
            pick(0, 61);
#else
#if __clang_major__ >= 8
        thread_local static std::mt19937 rg{
            std::random_device{}() +
            static_cast<uint32_t>(reinterpret_cast<uint64_t>(&length) &
                                  0xFFFFFFFFU)};
        thread_local static std::uniform_int_distribution<
            std::string::size_type>
            pick(0, 61);
#else
        // this will leak on thread termination,  older apple clang does not
        // have proper thread_local variables so there really isn't any option

        static __thread std::mt19937* genPtr = nullptr;
        if (genPtr == nullptr) {
            genPtr = new std::mt19937(
                std::random_device{}() + std::random_device{}() +
                static_cast<uint32_t>(reinterpret_cast<uint64_t>(&length) &
                                      0xFFFFFFFFU));
        }
        auto& rg = *genPtr;
        static __thread std::uniform_int_distribution<std::string::size_type>*
            pickPtr = nullptr;
        if (pickPtr == nullptr) {
            pickPtr =
                new std::uniform_int_distribution<std::string::size_type>(0,
                                                                          61);
        }
        auto& pick = *pickPtr;

#endif
#endif

        std::string s;

        s.reserve(length);

        while (length-- != 0U) {
            s.push_back(chars[pick(rg)]);
        }

        return s;
    }

}  // namespace utilities
}  // namespace gmlc
