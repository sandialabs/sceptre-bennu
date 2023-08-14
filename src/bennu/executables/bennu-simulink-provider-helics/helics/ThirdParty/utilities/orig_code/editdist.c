/*
Copyright (c) 2017-2020,
Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance
for Sustainable Energy, LLC.  See the top-level NOTICE for additional details.
All rights reserved. SPDX-License-Identifier: BSD-3-Clause
*/
#include "namecmp.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

int edit_distace(char* n1, char* n2)
{
    int result = 888;
    int edit[26][26];
    int ii = 0, jj = 0;
    int len_s1 = 0, len_s2 = 0;
    char s1_loc[25];
    char s2_loc[25];

    /*do a null check*/
    /*this test is not relevent in C*/

    /*first check if we have an exact match*/
    if (strcasecmp(n1, n2) == 0) {
        result = 0;
        return result;
    }
    len_s1 = strlen(n1);
    len_s2 = strlen(n2);
    /*more null checks if one string is a null*/
    /*also not relevent to C*/
    for (ii = 0; ii < MIN(25, len_s1); ii++) {
        s1_loc[ii] = toupper(n1[ii]);
    }
    len_s1 = ii;
    for (ii = 0; ii < MIN(25, len_s2); ii++) {
        s2_loc[ii] = toupper(n2[ii]);
    }
    len_s2 = ii;
    memset(edit, 0, sizeof(int) * 26 * 26);
    for (ii = 0; ii < len_s1; ii++) {
        edit[ii][0] = ii;
    }
    for (ii = 0; ii < len_s2; ii++) {
        edit[0][ii] = ii;
    }
    for (ii = 1; ii <= len_s1; ii++) {
        for (jj = 1; jj <= len_s2; jj++) {
            if (s1_loc[ii - 1] == s2_loc[jj - 1]) {
                edit[ii][jj] =
                    MIN(MIN(edit[ii - 1][jj] + 1, edit[ii][jj - 1] + 1),
                        edit[ii - 1][jj - 1]);
            } else {
                edit[ii][jj] =
                    MIN(MIN(edit[ii - 1][jj] + 1, edit[ii][jj - 1] + 1),
                        edit[ii - 1][jj - 1] + 1);
            }
        }
    }
    return (edit[len_s1][len_s2]);
}
