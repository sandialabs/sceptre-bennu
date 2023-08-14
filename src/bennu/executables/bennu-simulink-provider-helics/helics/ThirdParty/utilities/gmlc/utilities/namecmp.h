/*
Copyright (c) 2017-2020,
Battelle Memorial Institute; Lawrence Livermore National Security, LLC; Alliance
for Sustainable Energy, LLC.  See the top-level NOTICE for additional details.
All rights reserved. SPDX-License-Identifier: BSD-3-Clause
*/
#pragma once

#include "string_viewDef.h"

namespace gmlc {
namespace utilities {

    bool strIeq(string_view str1, string_view str2);

    void gencharcomp();
    float namecomp(char* n1, char* n2, char* nv1, char* nv2);
}  // namespace utilities
}  // namespace gmlc
