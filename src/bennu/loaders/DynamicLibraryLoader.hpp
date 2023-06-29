#ifndef BENNU_LOADERS_DYNAMICLIBRARYLOADER_HPP
#define BENNU_LOADERS_DYNAMICLIBRARYLOADER_HPP

#include <memory>
#include <string>

#include "bennu/utility/Singleton.hpp"

namespace bennu {
namespace loaders {

class PathFinder;

class DynamicLibraryLoader : public utility::Singleton<DynamicLibraryLoader>
{
public:
    friend class utility::Singleton<DynamicLibraryLoader>;

    virtual bool load(const std::string& fileName);

private:
    std::shared_ptr<PathFinder> mPathFinder;
    DynamicLibraryLoader();
    DynamicLibraryLoader(const DynamicLibraryLoader&);
    DynamicLibraryLoader& operator =(const DynamicLibraryLoader&);

};

} // namespace loaders
} // namespace bennu

#endif // BENNU_LOADERS_DYNAMICLIBRARYLOADER_HPP
