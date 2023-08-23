/*
Copyright (c) 2017-2020,
Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance
for Sustainable Energy, LLC.  See the top-level NOTICE for additional details.
All rights reserved. SPDX-License-Identifier: BSD-3-Clause
*/
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

float charcomp[128][128];

void gencharcomp();
float namecomp(char* n1, char* n2, char* nv1, char* nv2);
