# ~~~
# Copyright (c) 2017-2019, Battelle Memorial Institute; Lawrence Livermore
# National Security, LLC; Alliance for Sustainable Energy, LLC.
# See the top-level NOTICE for additional details.
# All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
# ~~~

set(utilities_source_files
    base64.cpp
    charMapper.cpp
    string_viewOps.cpp
    stringOps.cpp
    stringConversion.cpp
    vectorOps.cpp
    timeStringOps.cpp
)

set(string_comparison_sources editdist.cpp smithWat.cpp namecmp.h jwink.cpp dpcomp.cpp)

set(utilities_header_files
    base64.h
    charMapper.h
    generic_string_ops.hpp
    string_viewConversion.h
    string_viewDef.h
    string_viewOps.h
    stringConversion.h
    stringOps.h
    timeRepresentation.hpp
    timeStringOps.hpp
    vectorOps.hpp
    TimeSeries.hpp
    TimeSeriesMulti.hpp
    demangle.hpp
)

if(GMLC_UTILITIES_OBJECT_LIB)
    add_library(
        gmlc_utilities_obj OBJECT ${utilities_source_files} ${utilities_header_files}
                                  ${string_comparison_files}
    )
    target_include_directories(
        gmlc_utilities_obj PUBLIC $<BUILD_INTERFACE:${GMLC_UTILITIES_SOURCE_DIR}>
    )
    target_include_directories(
        gmlc_utilities_obj
        PRIVATE $<TARGET_PROPERTY:utilities_base,INTERFACE_INCLUDE_DIRECTORIES>
    )

    target_include_directories(
        gmlc_utilities_obj SYSTEM
        PRIVATE $<TARGET_PROPERTY:utilities_base,INTERFACE_SYSTEM_INCLUDE_DIRECTORIES>
    )

    target_compile_definitions(
        gmlc_utilities_obj
        PRIVATE $<TARGET_PROPERTY:utilities_base,INTERFACE_COMPILE_DEFINITIONS>
    )
endif()

if(GMLC_UTILITIES_STATIC_LIB)

    add_library(
        gmlc_utilities STATIC ${utilities_source_files} ${utilities_header_files}
                              ${string_comparison_files}
    )
    target_include_directories(
        gmlc_utilities PUBLIC $<BUILD_INTERFACE:${GMLC_UTILITIES_SOURCE_DIR}>
    )

    add_library(gmlc::utilities ALIAS gmlc_utilities)

    target_link_libraries(gmlc_utilities utilities_base)
    if(GMLC_UTILITIES_INSTALL)
        install(
            TARGETS gmlc_utilities ${UTILITIES_LIBRARY_EXPORT_COMMAND}
            RUNTIME DESTINATION bin
            ARCHIVE DESTINATION lib
            LIBRARY DESTINATION lib
        )

        install(FILES ${utilities_header_files} DESTINATION include)

        if(GMLC_UTILITIES_CLANG_TIDY)
            set_property(
                TARGET gmlc_utilities PROPERTY CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
            )
        endif()
    endif()

endif()
