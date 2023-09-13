/*
Copyright (c) 2017-2020,
Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance
for Sustainable Energy, LLC.  See the top-level NOTICE for additional details.
All rights reserved. SPDX-License-Identifier: BSD-3-Clause
*/
#include "namecmp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

float charcomp[28][28];

float dp_comp(char* n1, char* n2)
{
    int ii, jj, kk;
    int gapcnt = 0;
    int inspacei = 0;
    int inspacej = 0;
    float DV[100][100];

    double score;

    if (strcasecmp(n1, n2) == 0) {
        return 100.0;
    }

    DV[0][0] = charcomp[(int)n1[0]][(int)n2[0]];
    for (ii = 1; ii < strlen(n2); ii++) {
        DV[ii][0] = MAX(charcomp[(int)n1[0]][(int)n2[ii]], DV[ii - 1][0]);
        if (DV[ii][0] == 1.0) break;
    }
    for (jj = 1; jj < strlen(n1); jj++) {
        DV[0][jj] = MAX(charcomp[(int)n1[jj]][(int)n2[0]], DV[0][jj - 1]);
        if (DV[0][jj] == 1.0) break;
    }
    for (ii = 1; ii < strlen(n2); ii++) {
        for (jj = 1; jj < strlen(n1); jj++) {
            DV[ii][jj] =
                MAX(DV[ii - 1][jj - 1] + charcomp[(int)n1[jj]][(int)n2[ii]],
                    MAX(DV[ii][jj - 1], DV[ii - 1][jj]));
            if ((ii > 1) && (jj > 1)) {
                if ((DV[ii - 1][jj] == DV[ii][jj - 1]) &&
                    (DV[ii - 1][jj] > DV[ii - 1][jj - 1]) &&
                    (DV[ii - 1][jj - 1] == DV[ii - 2][jj - 2])) {
                    DV[ii - 1][jj - 1] = DV[ii - 1][jj];
                }
            }
        }
    }
#ifdef print
    printf(" |");
    for (ii = 0; ii < strlen(n1); ii++) {
        printf("  %c  |", n1[ii]);
    }
    printf("\n");
    for (ii = 0; ii < strlen(n2); ii++) {
        printf("%c|", n2[ii]);
        for (jj = 0; jj < strlen(n1); jj++) {
            printf("%5.2f|", DV[ii][jj]);
        }
        printf("\n");
    }
#endif
    /*Run the back trace algorithm and score simultaneously*/
    jj = strlen(n1) - 1;
    ii = strlen(n2) - 1;
    kk = 0;
    score = 0.0;
    while ((ii > 0) || (jj > 0)) {
        kk++;
        if (ii < 0) {
            inspacej = 0;
            score -= 1.0;
            if (inspacei == 0) {
                gapcnt += 2;
                inspacei = 2;
            }

            printf("%c %c\n", n1[jj], '*');
            jj--;
            continue;
        }
        if (jj < 0) {
            inspacei = 0;
            score -= 1.0;
            if (inspacej == 0) {
                gapcnt += 2;
                inspacej = 2;
            }

            printf("%c %c\n", '*', n2[ii]);
            ii--;
            continue;
        }

        if (n1[jj] == n2[ii]) {
            score += 1.0;
            printf("%c %c\n", n1[jj], n2[ii]);
            jj--;
            ii--;
        } else {
            if ((ii > 0) && (jj > 0)) {
                if ((DV[ii - 1][jj]) > DV[ii - 1][jj - 1]) {
                    if ((DV[ii - 1][jj]) >= (DV[ii][jj - 1])) {
                        inspacei = 0;
                        inspacej = 0;
                        score += charcomp[(int)'_'][(int)n2[ii]];
                        if (inspacej == 1) {
                            gapcnt++;
                            inspacej = 2;
                        } else if (inspacej == 0) {
                            inspacej = 1;
                            gapcnt++;
                        }
                        printf("%c %c\n", '*', n2[ii]);
                        ii--;
                    } else {
                        inspacej = 0;
                        score += charcomp[(int)'_'][(int)n1[jj]];
                        if (inspacei == 1) {
                            gapcnt++;
                            inspacei = 2;
                        } else if (inspacej == 0) {
                            inspacei = 1;
                            gapcnt++;
                        }
                        printf("%c %c\n", n1[jj], '*');
                        jj--;
                    }
                } else if ((DV[ii][jj - 1]) > DV[ii - 1][jj - 1]) {
                    // printf("(%d,%d);DV[%d][%d]=%f,
                    // DV[%d][%d]=%f\n",ii,jj,ii,jj-1,DV[ii][jj-1],ii-1,jj-1,DV[ii-1][jj-1]);
                    inspacej = 0;
                    score += charcomp[(int)'_'][(int)n1[jj]];
                    if (inspacei == 1) {
                        gapcnt++;
                        inspacei = 2;
                    } else if (inspacej == 0) {
                        inspacei = 1;
                        gapcnt++;
                    }
                    printf("%c %c\n", n1[jj], '*');
                    jj--;
                } else {
                    inspacei = 0;
                    inspacej = 0;
                    /*test for switched characters*/
                    if ((n1[jj] == n2[ii - 1]) && (n1[jj - 1] == n2[ii])) {
                        printf("%c %c\n", n1[jj], n2[ii]);
                        printf("%c %c\n", n1[jj - 1], n2[ii - 1]);
                        score += 1.0;
                        ii -= 2;
                        jj -= 2;
                        kk++;
                    } else {
                        score += charcomp[(int)n1[jj]][(int)n2[ii]];
                        printf("%c %c\n", n1[jj], n2[ii]);
                        ii--;
                        jj--;
                    }
                }
            } else if (jj > 0) {
                inspacej = 0;
                score += charcomp[(int)'_'][(int)n1[jj]];
                if (inspacei == 1) {
                    gapcnt++;
                    inspacei = 2;
                } else if (inspacej == 0) {
                    inspacei = 1;
                    gapcnt++;
                }
                printf("%c %c\n", n1[jj], '*');
                jj--;
            } else {
                score += charcomp[(int)'_'][(int)n2[ii]];
                if (inspacej == 1) {
                    gapcnt++;
                    inspacej = 2;
                } else if (inspacej == 0) {
                    inspacej = 1;
                    gapcnt++;
                }
                printf("%c %c\n", '*', n2[ii]);
                ii--;
            }
        }
    }
    if (gapcnt > 2) {
        score -= 3.0 * (gapcnt - 2);
    }
    if (score < 0.0) {
        score = 0.0;
    }
    score /= (float)(kk);
    return (score);
}

void gencharcomp()
{
    int ii, jj;
    for (ii = 0; ii < 28; ii++) {
        for (jj = 0; jj < 28; jj++) {
            charcomp[ii][jj] = -0.4;
        }
    }
    for (ii = 0; ii < 28; ii++) {
        charcomp[ii][ii] = 1.0;
    }
}
