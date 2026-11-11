#include "doctest.h"

#include <string>

#include "bennu/loaders/DynamicLibraryLoader.hpp"
#include "bennu/loaders/PathFinder.hpp"

using bennu::loaders::DynamicLibraryLoader;
using bennu::loaders::PathFinder;

TEST_CASE("DynamicLibraryLoader: Load built libraries and handle error cases")
{
    DynamicLibraryLoader* loader = DynamicLibraryLoader::the();
    REQUIRE(loader != nullptr);

    // 1. Loading built installed libraries should succeed
    CHECK(loader->load("libbennu-parsers.so"));
    CHECK(loader->load("libbennu-utility.so"));

    // 2. Loading non-existent library returns false without crashing
    CHECK_FALSE(loader->load("libnonexistent_xyz.so"));
    CHECK_FALSE(loader->load("nonexistent_unknown_lib"));
}

TEST_CASE("PathFinder: Filename correction and extension normalization")
{
    PathFinder pf;
    std::string fixed;

    // Bare library name without extension or "lib" prefix
    std::string ext = pf.checkAndFixFilename("myplugin", fixed);
#if defined(__APPLE__)
    CHECK(ext == "dylib");
    CHECK(fixed == "libmyplugin.dylib");
#else
    CHECK(ext == "so");
    CHECK(fixed == "libmyplugin.so");
#endif

    // Filename with cross-platform extension normalization (.dylib on Linux -> .so)
    std::string fixed2;
    std::string ext2 = pf.checkAndFixFilename("libtest.dylib", fixed2);
#if defined(__APPLE__)
    CHECK(ext2 == "dylib");
    CHECK(fixed2 == "libtest.dylib");
#else
    CHECK(ext2 == "so");
    CHECK(fixed2 == "libtest.so");
#endif

    // Filename with .so on macOS or Linux
    std::string fixed3;
    std::string ext3 = pf.checkAndFixFilename("libtest.so", fixed3);
#if defined(__APPLE__)
    CHECK(ext3 == "dylib");
    CHECK(fixed3 == "libtest.dylib");
#else
    CHECK(ext3 == "so");
    CHECK(fixed3 == "libtest.so");
#endif

    // DynamicLibraryLoader successfully normalizes .dylib on Linux to load .so
    CHECK(DynamicLibraryLoader::the()->load("libbennu-parsers.dylib"));
}
