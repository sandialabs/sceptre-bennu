/*
 * LLNS Copyright Start
 * Copyright (c) 2014-2018, Lawrence Livermore National Security
 * This work was performed under the auspices of the U.S. Department
 * of Energy by Lawrence Livermore National Laboratory in part under
 * Contract W-7405-Eng-48 and in part under Contract DE-AC52-07NA27344.
 * Produced at the Lawrence Livermore National Laboratory.
 * All rights reserved.
 * For details, see the LICENSE file.
 * LLNS Copyright End
 */

#include "string_viewOps.h"

#include "generic_string_ops.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace gmlc {
namespace utilities {
    namespace string_viewOps {
        void trimString(string_view& input, string_view trimCharacters)
        {
            input.remove_suffix(
                input.length() -
                std::min(input.find_last_not_of(trimCharacters) + 1,
                         input.size()));
            input.remove_prefix(
                std::min(input.find_first_not_of(trimCharacters),
                         input.size()));
        }

        string_view trim(string_view input, string_view trimCharacters)
        {
            const auto strStart = input.find_first_not_of(trimCharacters);
            if (strStart == std::string::npos) {
                return string_view();  // no content
            }

            const auto strEnd = input.find_last_not_of(trimCharacters);

            return input.substr(strStart, strEnd - strStart + 1);
        }

        void trim(string_viewVector& input, string_view trimCharacters)
        {
            for (auto& istr : input) {
                istr = trim(istr, trimCharacters);
            }
        }

        string_view getTailString(string_view input,
                                  char separationCharacter) noexcept
        {
            auto tc = input.find_last_of(separationCharacter);
            if (tc != string_view::npos) {
                input.remove_prefix(tc + 1);
            }
            return input;
        }

        string_view getTailString(string_view input, string_view sep) noexcept
        {
            auto tc = input.rfind(sep);
            if (tc != string_view::npos) {
                input.remove_prefix(tc + sep.size());
            }
            return input;
        }

        string_view getTailString_any(string_view input,
                                      string_view separationCharacters) noexcept
        {
            auto tc = input.find_last_of(separationCharacters);
            if (tc != string_view::npos) {
                input.remove_prefix(tc + 1);
            }
            return input;
        }

        string_view removeQuotes(string_view str)
        {
            string_view ret = trim(str);
            if (!ret.empty()) {
                if ((ret.front() == '\"') || (ret.front() == '\'') ||
                    (ret.front() == '`')) {
                    if (ret.back() == ret.front()) {
                        return ret.substr(1, ret.size() - 2);
                    }
                }
            }
            return ret;
        }

        static const auto pmap = pairMapper();

        string_view removeBrackets(string_view str)
        {
            string_view ret = trim(str);
            if (!ret.empty()) {
                if ((ret.front() == '[') || (ret.front() == '(') ||
                    (ret.front() == '{') || (ret.front() == '<')) {
                    if (ret.back() == pmap[ret.front()]) {
                        return ret.substr(1, ret.size() - 2);
                    }
                }
            }
            return ret;
        }

        string_view merge(string_view string1, string_view string2)
        {
            ptrdiff_t diff = (string2.data() - string1.data()) -
                static_cast<ptrdiff_t>(string1.length());
            if ((diff >= 0) &&
                (diff < 24))  // maximum of 23 characters between the strings
            {
                return string_view(string1.data(),
                                   diff + string1.length() + string2.length());
            }
            if (string1.empty()) {
                return string2;
            }
            if (string2.empty()) {
                return string1;
            }
            throw(std::out_of_range("unable to merge string_views"));
        }

        string_viewVector split(string_view str,
                                string_view delimiters,
                                delimiter_compression compression)
        {
            return generalized_string_split(
                str, delimiters, (compression == delimiter_compression::on));
        }

        string_viewVector splitlineQuotes(string_view line,
                                          string_view delimiters,
                                          string_view quoteChars,
                                          delimiter_compression compression)
        {
            bool compress = (compression == delimiter_compression::on);
            return generalized_section_splitting(
                line, delimiters, quoteChars, pmap, compress);
        }

        string_viewVector splitlineBracket(string_view line,
                                           string_view delimiters,
                                           string_view bracketChars,
                                           delimiter_compression compression)
        {
            bool compress = (compression == delimiter_compression::on);
            return generalized_section_splitting(
                line, delimiters, bracketChars, pmap, compress);
        }

        int toIntSimple(string_view input)
        {
            int ret = 0;
            for (auto c : input) {
                if (c >= '0' && c <= '9') {
                    ret = 10 * ret + (c - '0');
                }
            }
            return ret;
        }

        static const string_view digits("0123456789");
        int trailingStringInt(string_view input,
                              string_view& output,
                              int defNum)
        {
            if (input.empty() || (input.back() < '0' || input.back() > '9')) {
                output = input;
                return defNum;
            }
            int num = defNum;
            auto pos1 = input.find_last_not_of(digits);
            if (pos1 ==
                string_view::npos)  // in case the whole thing is a number
            {
                if (input.length() <= 10) {
                    output = string_view{};
                    num = toIntSimple(input);
                    return num;
                }
                pos1 = input.length() - 10;
            }
            size_t length = input.length();
            if (pos1 == length - 2) {
                num = input.back() - '0';
            } else if (length <= 10 || pos1 >= length - 10) {
                num = toIntSimple(input.substr(pos1 + 1));
            } else {
                num = toIntSimple(input.substr(length - 9));
                pos1 = length - 10;
            }

            if (input[pos1] == '_' || input[pos1] == '#') {
                output = input.substr(0, pos1);
            } else {
                output = input.substr(0, pos1 + 1);
            }

            return num;
        }

        int trailingStringInt(string_view input, int defNum)
        {
            if (input.empty() || (input.back() < '0' || input.back() > '9')) {
                return defNum;
            }

            auto pos1 = input.find_last_not_of(digits);
            if (pos1 ==
                string_view::npos)  // in case the whole thing is a number
            {
                if (input.length() <= 10) {
                    return toIntSimple(input);
                }
                pos1 = input.length() - 10;
            }
            size_t length = input.length();
            if (pos1 == length - 2) {
                return input.back() - '0';
            }
            if ((length <= 10) || (pos1 >= length - 10)) {
                return toIntSimple(input.substr(pos1 + 1));
            }
            return toIntSimple(input.substr(length - 9));
        }

    }  // namespace string_viewOps
}  // namespace utilities
}  // namespace gmlc
