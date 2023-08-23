/*
Copyright (c) 2017-2020,
Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance
for Sustainable Energy, LLC.  See the top-level NOTICE for additional details.
All rights reserved. SPDX-License-Identifier: BSD-3-Clause
*/
#include "namecmp.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

float SW_comp(char* n1, char* n2)
{
    int ii, jj, kk, nn;
    int maxi = 0;
    // float maxiscore=0.0;
    int maxj = 0;
    // float maxjscore=0.0;
    float DV[50][50];
    signed char c1[50];
    signed char c2[50];
    char v1[50];
    char v2[50];
    int gnum1[10];
    int gnum2[10];
    int numwords1 = 1;
    float words1 = 1.0;
    int numwords2 = 1;
    float words2 = 1.0;
    float ws1[10];
    float ws2[10];
    float gap = 0.4;
    float maxdv = 0.0;
    int l1;
    int l2;
    int lc;
    // float pen;
    FILE* fp;

    float score;
    /*
    if (strcasecmp(n1,n2)==0)
    {
        return 100.0;
    }
    */
    gnum1[0] = 1;
    gnum2[0] = 1;
    l1 = strlen(n1);
    l2 = strlen(n2);
    for (ii = 0; ii < l1; ii++) {
        c1[ii] = (signed char)toupper(n1[ii]) - 65;
        if (c1[ii] == -33) {
            c1[ii] = 27;
            gnum1[numwords1] = ii + 1;
            numwords1++;
        } else if ((c1[ii] < 0) || (c1[ii] > 28)) {
            c1[ii] = 28;
        }
    }
    for (ii = 0; ii < l2; ii++) {
        c2[ii] = (signed char)toupper(n2[ii]) - 65;
        if (c2[ii] == -33) {
            c2[ii] = 27;
            gnum2[numwords2] = ii + 1;
            numwords2++;
        } else if ((c2[ii] < 0) || (c2[ii] > 28)) {
            c2[ii] = 28;
        }
    }

    for (ii = 1; ii <= l2; ii++) {
        for (jj = 1; jj <= l1; jj++) {
            DV[ii][jj] =
                MAX(DV[ii - 1][jj - 1] +
                        charcomp[(int)c1[jj - 1]][(int)c2[ii - 1]],
                    MAX(0, MAX(DV[ii][jj - 1] - gap, DV[ii - 1][jj] - gap)));
            if ((ii > 1) && (jj > 1)) {
                if ((DV[ii][jj - 1] > DV[ii - 1][jj - 1]) &&
                    (DV[ii - 1][jj] > DV[ii - 1][jj - 1])) {
                    DV[ii - 1][jj - 1] = MAX(DV[ii - 1][jj], DV[ii][jj - 1]);
                    DV[ii][jj] = MAX(DV[ii - 1][jj - 1], DV[ii][jj]);
                }
            }
            if (DV[ii][jj] > maxdv) {
                maxdv = DV[ii][jj];
            }
        }
    }
    /*find the local maxima in the final rows*/
    if ((DV[l2][l1] - DV[l2 - 1][l1] > 0.001) &&
        (DV[l2][l1] - DV[l2][l1 - 1] > 0.001)) {
        maxj = l1;
        maxi = l2;
    } else {
        maxj = l1;
        for (jj = l1 - 1; jj > 2; jj--) {
            if ((DV[l2][jj] >= 1.95) && (DV[l2][jj] - DV[l2][jj - 1] > 0.001) &&
                (DV[l2][jj] - DV[l2][jj + 1] > 0.001)) {
                maxj = jj;
            }
        }
        maxi = l2;
        for (ii = l2 - 1; ii > 2; ii--) {
            if ((DV[ii][l1] >= 1.95) && (DV[ii][l1] - DV[ii - 1][l1] > 0.001) &&
                (DV[ii][l1] - DV[ii + 1][l1] > 0.001)) {
                maxi = ii;
            }
        }
    }
    // printf("maxi=%d,maxj=%d\n",maxi,maxj);
    fp = fopen("outfile.csv", "w");
    fprintf(fp, "0,0,");
    for (ii = 0; ii <= l1; ii++) {
        fprintf(fp, "%c,", n1[ii]);
    }
    fprintf(fp, "\n");
    for (ii = 0; ii <= l2; ii++) {
        if (ii > 0) {
            fprintf(fp, "%c,", n2[ii - 1]);
        } else {
            fprintf(fp, "0,");
        }
        for (jj = 0; jj <= l1; jj++) {
            fprintf(fp, "%5.2f,", DV[ii][jj]);
        }
        fprintf(fp, "\n");
    }
    fclose(fp);
    /*matrix score algorithm*/
    if (maxdv / (float)(MIN(l1, l2)) < 0.3) {
        return 0.0;
    }
    memset(v1, 0, 50);
    memset(v2, 0, 50);
    gnum1[numwords1] = l1;
    gnum2[numwords2] = l2;
    /*
    for (ii=0;ii<=numwords1;ii++)
    {
        printf("gnum1[%d]=%d\n",ii,gnum1[ii]);
    }
    for (ii=0;ii<=numwords2;ii++)
    {
        printf("gnum2[%d]=%d\n",ii,gnum2[ii]);
    }
    */
    words1 = (float)numwords1;
    words2 = (float)numwords2;
    // printf("numwords1=%d,numwords2=%d\n",numwords1,numwords2);
    /*backtrace along name 2*/
    jj = maxj;
    ii = l2;
    for (kk = numwords2 - 1; kk >= 0; kk--) {
        ws2[kk] = 0.0;
        lc = 0;
        while (ii >= gnum2[kk]) {
            // printf("DV[%d][%d]=%f\n",ii,jj,DV[ii][jj]);
            if (jj == 0) {
                ii--;
                continue;
            }
            if (charcomp[(int)c1[jj - 1]][(int)c2[ii - 1]] > 0.4) {
                ws2[kk] += DV[ii][jj] - DV[ii - 1][jj - 1];
                v2[jj] = 1;
                lc++;
                ii--;
                jj--;
                continue;
            }
            if (DV[ii - 1][jj - 1] >= DV[ii - 1][jj]) {
                if (DV[ii - 1][jj - 1] >= DV[ii][jj - 1]) {
                    ws2[kk] += DV[ii][jj] - DV[ii - 1][jj - 1];
                    v2[jj] = 1;
                    ii--;
                    jj--;
                    lc++;
                } else {
                    v2[jj] = 1;
                    if (c1[jj - 1] != 27) {
                        lc++;
                        ws2[kk] = MAX(0.0, ws2[kk] - 0.2);
                    }
                    jj--;
                }
            } else {
                if (DV[ii - 1][jj] > DV[ii][jj - 1]) {
                    ii--;
                    if ((c2[ii - 1] != 27) && (jj > 0)) {
                        ws2[kk] = MAX(0.0, ws2[kk] - 0.2);
                        lc++;
                    }
                } else {
                    v2[jj] = 1;
                    if (c1[jj - 1] != 27) {
                        lc++;
                        ws2[kk] = MAX(0.0, ws2[kk] - 0.2);
                    }
                    jj--;
                }
            }
        }
        maxj = l1;
        if (!((v2[l1] == 0) && (DV[ii][l1] - DV[ii - 1][l1] > 0.001) &&
              (DV[ii][l1] - DV[ii][l1 - 1] > 0.001) &&
              (DV[ii][l1] - DV[ii + 1][l1] > 0.001))) {
            for (nn = l1 - 1; nn > 2; nn--) {
                if ((v2[nn] == 0) && (DV[ii][nn] - DV[ii][nn - 1] > 0.001) &&
                    (DV[ii][nn] - DV[ii][nn + 1] > 0.001)) {
                    maxj = nn;
                }
            }
        }
        jj = maxj;

        ws2[kk] /= (float)lc;
        // printf("ws2[%d]=%f\n",kk,ws2[kk]);
        if (lc < 4) {
            if (lc == 2) {
                words2 -= 2.0 / 3;
                ws2[kk] /= 3.0;
            } else if (lc == 1) {
                words2 -= 0.75;
                ws2[kk] /= 4.0;
            } else {
                words2 -= 0.5;
                ws2[kk] /= 2.0;
            }
        }
        // printf("ws2[%d]=%f, lc=%d\n",kk,ws2[kk],lc);
    }
    /*backtrace along name 1*/
    ii = maxi;
    jj = l1;
    for (kk = numwords1 - 1; kk >= 0; kk--) {
        ws1[kk] = 0.0;
        lc = 0;
        while (jj >= gnum1[kk]) {
            if (ii == 0) {
                jj--;
                continue;
            }
            // printf("DV[%d][%d]=%f\n",ii,jj,DV[ii][jj]);
            if (charcomp[(int)c1[jj - 1]][(int)c2[ii - 1]] > 0.4) {
                ws1[kk] += DV[ii][jj] - DV[ii - 1][jj - 1];
                v1[ii] = 1;
                ii--;
                jj--;
                lc++;
                continue;
            }
            if (DV[ii - 1][jj - 1] >= DV[ii - 1][jj]) {
                if (DV[ii - 1][jj - 1] >= DV[ii][jj - 1]) {
                    ws1[kk] += DV[ii][jj] - DV[ii - 1][jj - 1];
                    v1[ii] = 1;
                    ii--;
                    jj--;
                    lc++;
                } else {
                    jj--;
                    if ((c1[jj - 1] != 27) && (ii > 0)) {
                        ws1[kk] = MAX(0.0, ws1[kk] - 0.2);
                        lc++;
                    }
                }
            } else {
                if (DV[ii - 1][jj] > DV[ii][jj - 1]) {
                    v1[ii] = 1;
                    if (c2[ii - 1] != 27) {
                        ws1[kk] = MAX(0.0, ws1[kk] - 0.2);
                        lc++;
                    }
                    ii--;
                } else {
                    jj--;
                    if ((c1[jj - 1] != 27) && (ii > 0)) {
                        ws1[kk] = MAX(0.0, ws1[kk] - 0.2);
                        lc++;
                    }
                }
            }
        }
        maxi = l2;
        if (!((v1[l2] == 0) && (DV[l2][jj] - DV[l2][jj - 1] > 0.001) &&
              (DV[l2][jj] - DV[l2 - 1][jj] > 0.001) &&
              (DV[l2][jj] - DV[l2][jj + 1] > 0.001))) {
            for (nn = l2 - 1; nn > 2; nn--) {
                if ((v1[nn] == 0) && (DV[nn][jj] - DV[nn - 1][jj] > 0.001) &&
                    (DV[nn][jj] - DV[nn + 1][jj] > 0.001)) {
                    maxi = nn;
                }
            }
        }
        ii = maxi;

        ws1[kk] /= (float)lc;

        if (lc < 4) {
            if (lc == 2) {
                words1 -= 2.0 / 3;
                ws1[kk] /= 3.0;
            } else if (lc == 1) {
                words1 -= 0.75;
                ws1[kk] /= 4.0;
            } else {
                words1 -= 0.5;
                ws1[kk] /= 2.0;
            }
        }
        // printf("ws1[%d]=%f, lc=%d\n",kk,ws1[kk],lc);
    }

    // printf("maxdv=%f\n",maxdv);
    score = maxdv / ((l1 + l2) / 2.0) * 0.2;
    for (ii = 0; ii < numwords1; ii++) {
        printf("ws1[%d]=%f\n", ii, ws1[ii]);
        score += ws1[ii] / words1 * 0.4;
    }
    for (ii = 0; ii < numwords2; ii++) {
        printf("ws2[%d]=%f\n", ii, ws2[ii]);
        score += ws2[ii] / words2 * 0.4;
    }
    return (score);
}
