/*
Copyright (c) 2017-2019,
Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance
for Sustainable Energy, LLC.  See the top-level NOTICE for additional details.
All rights reserved. SPDX-License-Identifier: BSD-3-Clause
*/
#include "namecmp.h"
#include "string_viewDef.h"

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace gmlc {
namespace utilities {
    bool strIeq(string_view str1, string_view str2)
    {
        if (str1 == str2) {
            return true;
        }
        if (str1.size() != str2.size()) {
            return false;
        }
        return std::equal(str1.begin(),
                          str1.end(),
                          str2.begin(),
                          [](char c1, char c2) {
                              if (c1 == c2) {
                                  return true;
                              }
                              if (c1 - c2 == 'A' - 'a') {
                                  return (c1 >= 'A' && c1 <= 'Z');
                              }
                              if (c2 - c1 == 'A' - 'a') {
                                  return (c2 >= 'A' && c2 <= 'Z');
                              }
                              return false;
                          });
    }

    float jaro_winkler_comp(string_view s1, string_view s2)
    {
        float Result = 0.0F;
        char use_winkler = 1;
        const int enhance_first_i = 4;

        int i, j;
        int len_s1, len_s2;
        int half_length = 0;
        int len_max = 0, len_min = 0;
        char c1, c2;
        float wink_adj = 0.0f;
        int f = 0; /*the number of first character matches*/

        char str_large[26]; /*26 instead of 25 to accommodate a termination
                               Character*/
        char str_small[26];
        int j_max = 0;
        int j_min = 0;
        int c = 0; /*common characters*/
        int t = 0; /*common transpositions*/
        int last_match_i = 0;

        /*first check if we have an exact match*/
        if (strIeq(s1, s2)) {
            return 1.0F;
        }
        len_s1 = static_cast<int>(s1.size());
        len_s2 = static_cast<int>(s1.size());

        /*the original code checks for a NULL value which won't happen here*/
        if (len_s1 > len_s2) {
            strncpy(str_large, s1.data(), 25);
            strncpy(str_small, s2.data(), 25);
            len_s1 = (std::min)(len_s1, 25);
            len_s2 = (std::min)(len_s2, 25);
            len_max = len_s1;
            len_min = len_s2;
            half_length = len_s2 / 2 - 1;
        } else {
            strncpy(str_small, s1.data(), 25);
            strncpy(str_large, s2.data(), 25);
            len_s1 = (std::min)(len_s1, 25);
            len_s2 = (std::min)(len_s2, 25);
            len_max = len_s2;
            len_min = len_s1;
            half_length = len_s1 / 2 - 1;
        }
        /*convert to all lower case*/
        for (i = 0; i < len_min; i++) {
            str_small[i] = static_cast<char>(tolower(str_small[i]));
        }
        for (i = 0; i < len_max; i++) {
            str_large[i] = static_cast<char>(tolower(str_large[i]));
        }
        /*compute the jaro value*/
        for (i = 1; i < len_min; i++) {
            c1 = str_small[i];
            c2 = str_large[i];
            if (c1 == c2) {
                c++;
                last_match_i = i;
                if ((i < enhance_first_i + 1) && (use_winkler)) {
                    if (f == i - 1) {
                        f++;
                    }
                }
            } else {
                j_max = i + half_length;
                if (j_max > len_max - 1) {
                    j_max = len_max - 1;
                }
                j_min = i - half_length;
                if (j_min < 0) {
                    j_min = 0;
                }
                for (j = j_min; j <= j_max; j++) {
                    if (i != j) {
                        c2 = str_large[j];
                        if (c1 == c2) {
                            c++;
                            if (last_match_i > j) {
                                t++;
                            }
                            last_match_i = j;
                            break;
                        }
                    }
                }
            }
        }
        t = c - t;
        if (c == 0) {
            Result = 0.0F;
            return (Result);
        }
        // printf("c=%d, t=%d, l1=%d, l2=%d, f=%d, half_length=%d\n",c,t,len_s1,
        // len_s2,f,half_length);
        Result = ((static_cast<float>(c) / static_cast<float>(len_s1)) +
                  (static_cast<float>(c) / static_cast<float>(len_s2)) +
                  (static_cast<float>(t) / static_cast<float>(c))) /
            3.0F;

        if (use_winkler) {
            wink_adj = Result + f * 0.1F * (1.0F - Result);
            Result = wink_adj;
        }

        return (Result);
    }

}  // namespace utilities
}  // namespace gmlc
