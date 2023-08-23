/*
Copyright (c) 2017-2019,
Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance
for Sustainable Energy, LLC.  See the top-level NOTICE for additional details.
All rights reserved. SPDX-License-Identifier: BSD-3-Clause
*/

/* Created by Anjuta version 1.2.4 */
/*	This file will not be overwritten */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "namecmp.h"

int main()
{
    FILE *fp;
    char n1[100];
    char n2[100];
    float score;
    gencharcomp();
    if ((fp = fopen("testfile.txt", "r")) == NULL)
    {
        fprintf(stderr, "Unable to open name file\n");
        exit(-1);
    }
    fgets(n1, 99, fp);
    fgets(n2, 99, fp);
    n1[strlen(n1) - 1] = '\0';
    n2[strlen(n2) - 1] = '\0';
    printf("name 1 = \"%s\", name 2 = \"%s\"\n\n", n1, n2);
    // score=dp_comp(n1,n2);
    // printf("\nscore=%f\n",score);

    score = jaro_winkler_comp(n1, n2);
    printf("Jaro-Winkler score= %f\n", score);
    printf("edit distance=%d\n", edit_distace(n1, n2));
    score = SW_comp(n1, n2);
    printf("\nscore=%f\n", score);
    return 0;
}
