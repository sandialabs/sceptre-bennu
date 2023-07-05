#include "DynamicLibraryLoader.hpp"

#include <dlfcn.h>
#include <iostream>

#include "bennu/loaders/PathFinder.hpp"

namespace bennu {
namespace loaders {

DynamicLibraryLoader::DynamicLibraryLoader():
    mPathFinder(new PathFinder)
{
    mPathFinder->addPath("/usr/lib");
    mPathFinder->addPath("/usr/local/lib");
}

bool DynamicLibraryLoader::load(const std::string& filename)
{
    std::string loadfile;
    std::string extension = mPathFinder->checkAndFixFilename(filename, loadfile);

    if (filename != loadfile)
    {
        std::cerr << "INFO: Loader changed library name: \"" << filename << "\" -> \"" << loadfile << std::endl;
    }

    std::string fullFilename = mPathFinder->getPathForFilename(loadfile);
    if (fullFilename == "")
    {
        std::cerr << "ERROR: Dynamic library " << loadfile << " was not found!" << std::endl;
        return false;
    }

    void* handle = NULL;
    handle = dlopen(fullFilename.c_str(), RTLD_GLOBAL | RTLD_NOW);
    if (handle == NULL)
    {
        char* e = dlerror();
        std::cerr << "ERROR: Dynamic library load: " << std::string(e) << std::endl;
        return false;
    }

    return true;
}

} // namespace loaders
} // namespace bennu
