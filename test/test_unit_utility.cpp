#include "doctest.h"

#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>

#include "bennu/loaders/PathFinder.hpp"
#include "bennu/utility/Log.hpp"
#include "bennu/utility/Singleton.hpp"

namespace {

struct ScopedDirCleanup
{
    boost::filesystem::path path;
    explicit ScopedDirCleanup(boost::filesystem::path p) :
        path(std::move(p))
    {}
    ~ScopedDirCleanup()
    {
        boost::system::error_code ec;
        boost::filesystem::remove_all(path, ec);
    }
};

class DummyService : public bennu::utility::Singleton<DummyService>
{
public:
    friend class bennu::utility::Singleton<DummyService>;
    int value{42};

private:
    DummyService() = default;
};

} // namespace

TEST_CASE("Singleton: theExists transitions from false to true, the returns same pointer")
{
    CHECK_FALSE(DummyService::theExists());

    DummyService* instance1 = DummyService::the();
    CHECK(DummyService::theExists());
    REQUIRE(instance1 != nullptr);
    CHECK(instance1->value == 42);

    DummyService* instance2 = DummyService::the();
    CHECK(instance1 == instance2);

    instance1->value = 99;
    CHECK(instance2->value == 99);
}

TEST_CASE("Log: logLevel parsing of BENNU_LOG_LEVEL and logEnabled truth evaluation")
{
    using bennu::utility::LOG_DEBUG;
    using bennu::utility::LOG_ERROR;
    using bennu::utility::LOG_INFO;
    using bennu::utility::LOG_WARN;
    using bennu::utility::logEnabled;
    using bennu::utility::logLevel;
    using bennu::utility::parseLogLevel;

    // Direct parser tests
    CHECK(parseLogLevel(nullptr) == LOG_INFO);
    CHECK(parseLogLevel("") == LOG_INFO);
    CHECK(parseLogLevel("invalid_string") == LOG_INFO);

    CHECK(parseLogLevel("0") == LOG_ERROR);
    CHECK(parseLogLevel("error") == LOG_ERROR);
    CHECK(parseLogLevel("ERROR") == LOG_ERROR);
    CHECK(parseLogLevel("Error") == LOG_ERROR);

    CHECK(parseLogLevel("1") == LOG_WARN);
    CHECK(parseLogLevel("warn") == LOG_WARN);
    CHECK(parseLogLevel("warning") == LOG_WARN);
    CHECK(parseLogLevel("WARN") == LOG_WARN);

    CHECK(parseLogLevel("2") == LOG_INFO);
    CHECK(parseLogLevel("info") == LOG_INFO);
    CHECK(parseLogLevel("INFO") == LOG_INFO);

    CHECK(parseLogLevel("3") == LOG_DEBUG);
    CHECK(parseLogLevel("debug") == LOG_DEBUG);
    CHECK(parseLogLevel("DEBUG") == LOG_DEBUG);

    // Environment-driven logLevel(reload = true) tests
    unsetenv("BENNU_LOG_LEVEL");
    CHECK(logLevel(true) == LOG_INFO);

    setenv("BENNU_LOG_LEVEL", "0", 1);
    CHECK(logLevel(true) == LOG_ERROR);
    CHECK(logEnabled(LOG_ERROR));
    CHECK_FALSE(logEnabled(LOG_WARN));
    CHECK_FALSE(logEnabled(LOG_INFO));
    CHECK_FALSE(logEnabled(LOG_DEBUG));

    setenv("BENNU_LOG_LEVEL", "WARN", 1);
    CHECK(logLevel(true) == LOG_WARN);
    CHECK(logEnabled(LOG_ERROR));
    CHECK(logEnabled(LOG_WARN));
    CHECK_FALSE(logEnabled(LOG_INFO));
    CHECK_FALSE(logEnabled(LOG_DEBUG));

    setenv("BENNU_LOG_LEVEL", "info", 1);
    CHECK(logLevel(true) == LOG_INFO);
    CHECK(logEnabled(LOG_ERROR));
    CHECK(logEnabled(LOG_WARN));
    CHECK(logEnabled(LOG_INFO));
    CHECK_FALSE(logEnabled(LOG_DEBUG));

    setenv("BENNU_LOG_LEVEL", "Debug", 1);
    CHECK(logLevel(true) == LOG_DEBUG);
    CHECK(logEnabled(LOG_ERROR));
    CHECK(logEnabled(LOG_WARN));
    CHECK(logEnabled(LOG_INFO));
    CHECK(logEnabled(LOG_DEBUG));

    // Reset back to ERROR or unset for quiet test runs
    setenv("BENNU_LOG_LEVEL", "ERROR", 1);
    logLevel(true);
}

TEST_CASE("PathFinder: extractDirectory")
{
    bennu::loaders::PathFinder pf;

    // Create a temporary file to test existing file extraction
    const std::string tmpDir = "/tmp/pf_extract_test";
    ScopedDirCleanup cleanup(tmpDir);
    boost::filesystem::create_directories(tmpDir);
    const std::string tmpFilePath = tmpDir + "/sample.txt";
    {
        std::ofstream out(tmpFilePath);
        out << "test";
    }

    std::string extracted = pf.extractDirectory(tmpFilePath);
    CHECK_FALSE(extracted.empty());
    CHECK(extracted.find("pf_extract_test") != std::string::npos);

    // Nonexistent file returns empty string
    std::string missing = pf.extractDirectory("/tmp/nonexistent_dir_9999/none.txt");
    CHECK(missing.empty());
}

TEST_CASE("PathFinder: semicolon-delimited path parsing (setPath, getPath)")
{
    bennu::loaders::PathFinder pf;

    // Semicolon-delimited paths
    pf.setPath("/usr/bin;/usr/local/bin;/opt/bennu");
    CHECK(pf.getPath(";") == "/usr/bin;/usr/local/bin;/opt/bennu");

    std::vector<std::string> paths = pf.getPaths();
    REQUIRE(paths.size() == 3);
    CHECK(paths[0] == "/usr/bin");
    CHECK(paths[1] == "/usr/local/bin");
    CHECK(paths[2] == "/opt/bennu");

    // Custom delimiter
    pf.setPath("alpha:beta:gamma", ":");
    CHECK(pf.getPath(":") == "alpha:beta:gamma");
    CHECK(pf.getPaths().size() == 3);

    // Empty path string
    pf.setPath("");
    CHECK(pf.getPath(";").empty());
    CHECK(pf.getPaths().empty());

    // Empty separator string does not hang and stores path as single element
    pf.setPath("/single/path", "");
    CHECK(pf.getPaths().size() == 1);
    CHECK(pf.getPaths()[0] == "/single/path");
}

TEST_CASE("PathFinder: working directory push/pop and empty pop safety")
{
    bennu::loaders::PathFinder pf;

    CHECK(pf.getWorkingDirectory().empty());

    // Popping empty working directory does not crash or underflow
    pf.popWorkingDirectory();
    CHECK(pf.getWorkingDirectory().empty());

    pf.pushWorkingDirectory("/work/dir1");
    REQUIRE(pf.getWorkingDirectory().size() == 1);
    CHECK(pf.getWorkingDirectory()[0] == "/work/dir1");

    pf.pushWorkingDirectory("/work/dir2");
    REQUIRE(pf.getWorkingDirectory().size() == 2);
    CHECK(pf.getWorkingDirectory()[1] == "/work/dir2");

    // getPaths combines base paths with working directories
    pf.setPath("/base/path");
    std::vector<std::string> combined = pf.getPaths();
    REQUIRE(combined.size() == 3);
    CHECK(combined[0] == "/base/path");
    CHECK(combined[1] == "/work/dir1");
    CHECK(combined[2] == "/work/dir2");

    pf.popWorkingDirectory();
    REQUIRE(pf.getWorkingDirectory().size() == 1);
    CHECK(pf.getWorkingDirectory()[0] == "/work/dir1");

    pf.popWorkingDirectory();
    CHECK(pf.getWorkingDirectory().empty());

    // Extra pop when already empty is safe
    pf.popWorkingDirectory();
    CHECK(pf.getWorkingDirectory().empty());
}

TEST_CASE("PathFinder: findFile resolving relative and nested paths")
{
    bennu::loaders::PathFinder pf;

    const std::string rootDir = "/tmp/pf_findfile_test";
    ScopedDirCleanup cleanup(rootDir);
    const std::string subDir = rootDir + "/nested/deep";
    boost::filesystem::create_directories(subDir);

    const std::string targetFile = subDir + "/found_me.xml";
    {
        std::ofstream out(targetFile);
        out << "<xml/>";
    }

    boost::filesystem::path pathFound;
    bool found = pf.findFile(rootDir, "found_me.xml", pathFound);
    CHECK(found);
    CHECK(boost::filesystem::exists(pathFound));
    CHECK(pathFound.filename().string() == "found_me.xml");

    // Searching for missing file returns false
    boost::filesystem::path notFound;
    CHECK_FALSE(pf.findFile(rootDir, "missing_file.xml", notFound));

    // Searching inside nonexistent root returns false
    CHECK_FALSE(pf.findFile("/tmp/nonexistent_root_9999", "file.xml", notFound));
}
