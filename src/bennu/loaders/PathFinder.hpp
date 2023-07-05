#ifndef BENNU_LOADERS_PATHFINDER_HPP
#define BENNU_LOADERS_PATHFINDER_HPP

#include <map>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>

namespace bennu {
namespace loaders {

class PathFinder
{
public:
    PathFinder();

    void addPath(const std::string& x);

    void pushWorkingDirectory(const std::string& x);

    void popWorkingDirectory();

    std::string extractDirectory(const std::string& fullName);

    std::vector<std::string> getWorkingDirectory();

    void setPaths(const std::vector<std::string>& x);

    std::vector<std::string> getPaths() const;

    void setPath(const std::string& paths, const std::string& separator = ";");

    std::string getPath(const std::string& separator = ";") const;

    std::string getPathForFilename(const std::string& filename) const;

    bool findFile(const boost::filesystem::path &path, const std::string &filename, boost::filesystem::path &pathFound);

    std::string checkAndFixFilename(const std::string& originalFilename, std::string& newFilename);

private:
    std::vector<std::string> p_paths;
    std::vector<std::string> p_workingDirectory;
    PathFinder(const PathFinder&);
    PathFinder& operator =(const PathFinder&);

};

} // namespace bennu
} // namespace loaders

#endif // BENNU_LOADERS_PATHFINDER_HPP
