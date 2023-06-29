#include "PathFinder.hpp"

#include <stdlib.h>
#include <sys/stat.h>
#include <iostream>

namespace bennu {
namespace loaders {

bennu::loaders::PathFinder::PathFinder()
{
}

bool bennu::loaders::PathFinder::findFile(const boost::filesystem::path& path,  // in this directory,
                                            const std::string& filename, // search for this name,
                                            boost::filesystem::path& pathFound) // placing path here if found
{
    if (!boost::filesystem::exists(path))
    {
        return false;
    }

    boost::filesystem::directory_iterator end_itr; // default construction yields past-the-end
    for (boost::filesystem::directory_iterator itr(path); itr != end_itr; ++itr)
    {
        if (boost::filesystem::is_directory(itr->status()))
        {
            if (findFile(itr->path(), filename, pathFound))
            {
                return true;
            }
        }
        else if (itr->path().filename() == filename) // see below
        {
            pathFound = itr->path();
            return true;
        }
    }
    return false;
}

void bennu::loaders::PathFinder::addPath(const std::string& x)
{
    p_paths.push_back(x);
}

void bennu::loaders::PathFinder::setPaths(const std::vector<std::string>& x)
{
    p_paths = x;
}

std::vector<std::string> bennu::loaders::PathFinder::getPaths() const
{
    std::vector<std::string> paths = p_paths;
    std::copy(p_workingDirectory.begin(), p_workingDirectory.end(), std::back_inserter(paths));
    return paths;
}

std::string bennu::loaders::PathFinder::getPathForFilename(const std::string& filename) const
{
    std::vector<std::string> p = getPaths();

    struct stat status;
    for (auto i = p.begin(); i != p.end(); ++i)
    {
        *i += "/" + filename;
        if (!stat(i->c_str(), &status) && S_ISREG(status.st_mode))
        {
            return *i;
        }
    }


    if (!stat(filename.c_str(), &status) && S_ISREG(status.st_mode))
    {
        return filename;
    }

    return "";
}

void bennu::loaders::PathFinder::pushWorkingDirectory(const std::string& x)
{
    p_workingDirectory.push_back(x);
}

void bennu::loaders::PathFinder::popWorkingDirectory()
{
    p_workingDirectory.pop_back();
}

std::vector<std::string> bennu::loaders::PathFinder::getWorkingDirectory()
{
    return p_workingDirectory;
}

std::string bennu::loaders::PathFinder::extractDirectory(const std::string& fullName)
{
    size_t forwardSlashPos = fullName.find_last_of('/');
    size_t backwardSlashPos = fullName.find_last_of('\\');
    size_t lastSlashPos = std::string::npos;
    if (forwardSlashPos != std::string::npos && backwardSlashPos != std::string::npos)
    {
        lastSlashPos = std::max(forwardSlashPos, backwardSlashPos);
    }
    else if (forwardSlashPos != std::string::npos)
    {
        lastSlashPos = forwardSlashPos;
    }
    else if (backwardSlashPos != std::string::npos)
    {
        lastSlashPos = backwardSlashPos;
    }

    std::string directory = "";

    boost::filesystem::path fname(fullName);
    if (boost::filesystem::exists(fname))
    {
        //file exists
        if (lastSlashPos != std::string::npos)
        {
            // directory supplied in fullName
            directory = fullName.substr(0, lastSlashPos + 1);
            boost::filesystem::path path(directory);

            boost::filesystem::path dirpath = boost::filesystem::system_complete(path);

            directory = dirpath.string();
        }
        else
        {
            // no directory supplied so add a relative path inorder to get full path
            std::string dotpath("./");
            boost::filesystem::path dirpath = boost::filesystem::system_complete(dotpath);

            directory = dirpath.string();
        }
    }
    else
    {
        // file does not exist
        std::cerr << "ERROR: file " << fullName << " does not exist\n" << std::endl;;
    }

    return directory;
}

std::string bennu::loaders::PathFinder::checkAndFixFilename(const std::string& originalFilename, std::string& newFilename)
{
    newFilename = originalFilename;
    std::string tail = "";
    size_t dot = newFilename.find_last_of('.');
    if (dot != std::string::npos)
    {
        //  Extract the extension that was found.
        tail = newFilename.substr(dot+1);

#if defined(__APPLE__)
        if (tail == "so")
        {
            newFilename.replace(dot + 1, tail.length(), "dylib");
            tail = "dylib";

            //  Windows doesn't normally have a preceding lib in the name of
            //  its libraries, so check to see that it is there and if it
            //  isn't, then add it.
            std::string firstThree = newFilename.substr(0, 3);
            if (firstThree != "lib")
            {
                newFilename.insert(0, "lib");
            }
        }
#else
        if (tail == "dylib")
        {
            newFilename.replace(dot + 1, tail.length(), "so");
            tail = "so";

            //  Windows doesn't normally have a preceding lib in the name of
            //  its libraries, so check to see that it is there and if it
            //  isn't, then add it.
            std::string firstThree = newFilename.substr(0, 3);
            if (firstThree != "lib")
            {
                newFilename.insert(0, "lib");
            }
        }
#endif
    }
    else
    {
        //  There was no extension on the filename.  We'll assume for now that
        //  they are trying to load a library and chose not to add the
        //  extension.  We'll add the extension for them.
#if defined(__APPLE__)
        tail = "dylib";
        newFilename += ".dylib";

        //  Darwin likes to have a preceding lib in the name of its libraries, so
        //  check to see that it is there and if it isn't, then add it.
        std::string firstThree = newFilename.substr(0, 3);
        if (firstThree != "lib")
        {
            newFilename.insert(0, "lib");
        }
#else
        tail = "so";
        newFilename += ".so";

        //  Linux likes to have a preceding lib in the name of its libraries, so
        //  check to see that it is there and if it isn't, then add it.
        std::string firstThree = newFilename.substr(0, 3);
        if (firstThree != "lib")
        {
            newFilename.insert(0, "lib");
        }

#endif
    }

    return tail;
}

} // namespace bennu
} // namespace loaders
