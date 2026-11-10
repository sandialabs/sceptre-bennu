#include "LogControl.hpp"

#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <thread>
#include <vector>

#include "bennu/utility/Log.hpp"

namespace bennu {
namespace utility {

namespace {

const char* const LEVEL_NAMES[] = {"error", "warn", "info", "debug"};
const size_t DEFAULT_MAX_SIZE = 50UL << 20;
const int DEFAULT_KEEP = 3;

std::string socketDir()
{
    return "/tmp/bennu-" + std::to_string(getuid());
}

// The dir must be ours, a real directory (not a symlink) and 0700 before we trust it.
bool prepareDir(const std::string& dir, bool create)
{
    if (create)
    {
        mkdir(dir.c_str(), 0700);
    }
    struct stat st;
    if (lstat(dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode) || st.st_uid != getuid())
    {
        return false;
    }
    return (st.st_mode & 077) == 0 || chmod(dir.c_str(), 0700) == 0;
}

sockaddr_un makeAddr(const std::string& path)
{
    sockaddr_un a;
    std::memset(&a, 0, sizeof a);
    a.sun_family = AF_UNIX;
    std::strncpy(a.sun_path, path.c_str(), sizeof a.sun_path - 1);
    return a;
}

bool sendAll(int fd, const std::string& s, int flags = 0)
{
    size_t off = 0;
    while (off < s.size())
    {
        ssize_t n = send(fd, s.data() + off, s.size() - off, flags | MSG_NOSIGNAL);
        if (n < 0 && errno == EINTR)
        {
            continue;
        }
        if (n <= 0)
        {
            return false;
        }
        off += static_cast<size_t>(n);
    }
    return true;
}

std::string humanBytes(size_t n)
{
    char b[32];
    if (n >= (1UL << 20))
    {
        std::snprintf(b, sizeof b, "%.1f MB", static_cast<double>(n) / (1 << 20));
    }
    else
    {
        std::snprintf(b, sizeof b, "%.1f KB", static_cast<double>(n) / 1024);
    }
    return b;
}

// "50M", "512K", "1G" or plain bytes; 0 on error.
size_t parseSize(const std::string& s)
{
    char* end = nullptr;
    unsigned long long v = std::strtoull(s.c_str(), &end, 10);
    if (end == s.c_str())
    {
        return 0;
    }
    switch (std::toupper(static_cast<unsigned char>(*end)))
    {
    case 'K':
        v <<= 10;
        break;
    case 'M':
        v <<= 20;
        break;
    case 'G':
        v <<= 30;
        break;
    case '\0':
        break;
    default:
        return 0;
    }
    return static_cast<size_t>(v);
}

// "90", "30s", "10m", "2h" -> seconds; 0 on error.
long parseSeconds(const std::string& s)
{
    char* end = nullptr;
    long v = std::strtol(s.c_str(), &end, 10);
    if (end == s.c_str() || v <= 0)
    {
        return 0;
    }
    switch (*end)
    {
    case 'h':
        return v * 3600;
    case 'm':
        return v * 60;
    case 's':
    case '\0':
        return v;
    default:
        return 0;
    }
}

class LogControl
{
public:
    LogControl(const std::string& name) :
        mName(name.substr(0, 60)),
        mStartLevel(logLevel())
    {
    }

    ~LogControl()
    {
        mStop = true;
        if (mThread.joinable())
        {
            mThread.join();
        }
        closeFile();
        for (auto& t : mTailers)
        {
            close(t.fd);
        }
        mTailers.clear();
        stopCapture();
        if (mListen >= 0)
        {
            close(mListen);
            unlink(mPath.c_str());
        }
    }

    bool start()
    {
        std::string dir = socketDir();
        if (!prepareDir(dir, true))
        {
            std::fprintf(stderr, "log control disabled: %s is not a private directory owned by uid %d\n", dir.c_str(), getuid());
            return false;
        }
        mPath = dir + "/" + mName + "." + std::to_string(getpid()) + ".sock";
        mListen = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        mode_t old = umask(077);
        unlink(mPath.c_str());
        sockaddr_un a = makeAddr(mPath);
        bool ok = mListen >= 0 && bind(mListen, reinterpret_cast<sockaddr*>(&a), sizeof a) == 0 && chmod(mPath.c_str(), 0600) == 0 && listen(mListen, 8) == 0;
        umask(old);
        if (!ok)
        {
            std::fprintf(stderr, "log control disabled: cannot listen on %s: %s\n", mPath.c_str(), std::strerror(errno));
            if (mListen >= 0)
            {
                close(mListen);
                mListen = -1;
            }
            return false;
        }
        mThread = std::thread([this] { run(); });
        return true;
    }

private:
    struct Tailer
    {
        int fd;
        int restoreLevel; // -1: tail did not change the level
    };

    std::string mName, mPath;
    int mListen = -1;
    std::thread mThread;
    std::atomic<bool> mStop{false};
    int mStartLevel;

    // stdout/stderr capture: fd 1/2 are replaced by pipes drained by run(), which forwards
    // to the original destination, the file and tail clients.
    bool mCapturing = false;
    int mSaved[2] = {-1, -1};
    int mPipe[2] = {-1, -1};

    int mFile = -1;
    std::string mFilePath;
    size_t mFileSize = 0, mMaxSize = DEFAULT_MAX_SIZE;
    int mKeep = DEFAULT_KEEP;

    std::vector<Tailer> mTailers;

    bool mDirty = false; // a tail client went away; capture may no longer be needed
    bool mRevertPending = false;
    int mRevertTo = 0;
    std::chrono::steady_clock::time_point mRevertAt;

    void run()
    {
        while (!mStop)
        {
            std::vector<pollfd> fds = {{mListen, POLLIN, 0}};
            if (mCapturing)
            {
                fds.push_back({mPipe[0], POLLIN, 0});
                fds.push_back({mPipe[1], POLLIN, 0});
            }
            size_t tailBase = fds.size();
            for (auto& t : mTailers)
            {
                fds.push_back({t.fd, POLLIN, 0});
            }
            int timeout = 200;
            if (mRevertPending)
            {
                auto left = std::chrono::duration_cast<std::chrono::milliseconds>(mRevertAt - std::chrono::steady_clock::now()).count();
                timeout = static_cast<int>(std::max<long long>(0, std::min<long long>(left, timeout)));
            }
            if (poll(fds.data(), fds.size(), timeout) < 0 && errno != EINTR)
            {
                break;
            }
            if (mRevertPending && std::chrono::steady_clock::now() >= mRevertAt)
            {
                setLogLevel(mRevertTo);
                mRevertPending = false;
            }
            // reverse, so erasing keeps the indices of the ones still to visit valid
            for (size_t i = fds.size(); i-- > tailBase;)
            {
                char c;
                if (!fds[i].revents)
                {
                    continue;
                }
                ssize_t r = recv(fds[i].fd, &c, 1, MSG_DONTWAIT);
                if (r == 0 || (r < 0 && errno != EAGAIN && errno != EINTR))
                {
                    dropTailer(i - tailBase);
                }
            }
            if (mCapturing)
            {
                for (int i = 0; i < 2; i++)
                {
                    if (fds[1 + i].revents & (POLLIN | POLLHUP))
                    {
                        drain(i);
                    }
                }
            }
            if (fds[0].revents & POLLIN)
            {
                accept_();
            }
            if (mDirty)
            {
                mDirty = false;
                updateCapture();
            }
        }
    }

    // ---- capture and sinks ----

    void startCapture()
    {
        if (mCapturing)
        {
            return;
        }
        std::fflush(stdout);
        std::fflush(stderr);
        std::setvbuf(stdout, nullptr, _IOLBF, 0); // so tail sees lines as they happen
        int p[2][2];
        if (pipe2(p[0], O_CLOEXEC) != 0 || pipe2(p[1], O_CLOEXEC) != 0)
        {
            return;
        }
        for (int i = 0; i < 2; i++)
        {
            mSaved[i] = dup(i + 1); // [0] is stdout, [1] is stderr
            fcntl(p[i][0], F_SETFL, O_NONBLOCK);
            mPipe[i] = p[i][0];
            dup2(p[i][1], i + 1);
            close(p[i][1]);
        }
        mCapturing = true;
    }

    void stopCapture()
    {
        if (!mCapturing)
        {
            return;
        }
        for (int i = 0; i < 2; i++)
        {
            if (mSaved[i] >= 0)
            {
                dup2(mSaved[i], i + 1); // also drops the pipe's write end, so drain sees EOF
            }
            drain(i);
            close(mPipe[i]);
            if (mSaved[i] >= 0)
            {
                close(mSaved[i]);
            }
            mSaved[i] = mPipe[i] = -1;
        }
        mCapturing = false;
    }

    void updateCapture()
    {
        if (mFile >= 0 || !mTailers.empty())
        {
            startCapture();
        }
        else
        {
            stopCapture();
        }
    }

    void drain(int i)
    {
        char buf[4096];
        ssize_t n;
        while ((n = read(mPipe[i], buf, sizeof buf)) > 0)
        {
            std::string s(buf, static_cast<size_t>(n));
            if (mSaved[i] >= 0)
            {
                size_t off = 0;
                while (off < s.size())
                {
                    ssize_t w = write(mSaved[i], s.data() + off, s.size() - off);
                    if (w < 0 && errno == EINTR)
                    {
                        continue;
                    }
                    if (w <= 0)
                    {
                        break;
                    }
                    off += static_cast<size_t>(w);
                }
            }
            writeFile(s);
            for (size_t t = mTailers.size(); t-- > 0;)
            {
                // a slow reader loses lines rather than stalling the process
                if (!sendAll(mTailers[t].fd, s, MSG_DONTWAIT) && errno != EAGAIN)
                {
                    dropTailer(t);
                }
            }
        }
    }

    void dropTailer(size_t i)
    {
        close(mTailers[i].fd);
        if (mTailers[i].restoreLevel >= 0)
        {
            setLogLevel(mTailers[i].restoreLevel);
        }
        mTailers.erase(mTailers.begin() + static_cast<long>(i));
        mDirty = true;
    }

    bool openFile(const std::string& path)
    {
        int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (fd < 0)
        {
            return false;
        }
        closeFile();
        mFile = fd;
        mFilePath = path;
        struct stat st;
        mFileSize = fstat(fd, &st) == 0 ? static_cast<size_t>(st.st_size) : 0;
        return true;
    }

    void closeFile()
    {
        if (mFile >= 0)
        {
            close(mFile);
            mFile = -1;
        }
    }

    // path -> path.1 -> ... -> path.<keep>; the oldest falls off the end
    void rotate()
    {
        close(mFile);
        if (mKeep > 0)
        {
            for (int k = mKeep; k >= 1; k--)
            {
                std::string from = k == 1 ? mFilePath : mFilePath + "." + std::to_string(k - 1);
                rename(from.c_str(), (mFilePath + "." + std::to_string(k)).c_str());
            }
        }
        else
        {
            unlink(mFilePath.c_str());
        }
        mFile = open(mFilePath.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC | O_NOFOLLOW, 0600);
        mFileSize = 0;
    }

    void writeFile(const std::string& s)
    {
        if (mFile < 0)
        {
            return;
        }
        if (mFileSize > 0 && mFileSize + s.size() > mMaxSize)
        {
            rotate();
            if (mFile < 0)
            {
                return;
            }
        }
        ssize_t w = write(mFile, s.data(), s.size());
        if (w > 0)
        {
            mFileSize += static_cast<size_t>(w);
        }
    }

    // ---- control socket ----

    void accept_()
    {
        int c = accept4(mListen, nullptr, nullptr, SOCK_CLOEXEC);
        if (c < 0)
        {
            return;
        }
        ucred cred;
        socklen_t len = sizeof cred;
        pollfd pf = {c, POLLIN, 0};
        char buf[4096];
        ssize_t n = 0;
        // directory + socket modes already keep other users out; this is belt and braces
        if (getsockopt(c, SOL_SOCKET, SO_PEERCRED, &cred, &len) == 0 && (cred.uid == getuid() || cred.uid == 0) && poll(&pf, 1, 200) > 0)
        {
            n = recv(c, buf, sizeof buf - 1, 0);
        }
        if (n <= 0)
        {
            close(c);
            return;
        }
        std::string line(buf, static_cast<size_t>(n));
        line = line.substr(0, line.find('\n'));
        std::string out = handle(line, c);
        if (!out.empty())
        {
            sendAll(c, out);
        }
        if (mTailers.empty() || mTailers.back().fd != c)
        {
            close(c);
        }
    }

    // Returns the reply. An empty reply means "tail": the connection is kept open and streamed.
    std::string handle(const std::string& line, int c)
    {
        std::istringstream in(line);
        std::string cmd;
        in >> cmd;
        if (cmd == "status")
        {
            return status();
        }
        if (cmd == "on") // on <max-bytes> <keep> <level|-> <file...>
        {
            size_t maxSize = 0;
            int keep = -1;
            std::string level, file;
            in >> maxSize >> keep >> level;
            in.get();
            std::getline(in, file);
            if (file.empty() || file[0] != '/')
            {
                file = socketDir() + "/" + mName + ".log";
            }
            if (level != "-" && parseLogLevel(level) < 0)
            {
                return "error: bad level\n";
            }
            bool same = mFile >= 0 && file == mFilePath;
            // a new file is opened first (closing the old one); on failure nothing changes
            if (!same && !openFile(file))
            {
                return "error: cannot open " + file + ": " + std::strerror(errno) + "\n";
            }
            // 0 / -1 mean "not given": defaults for a new file, unchanged for the same one
            if (maxSize || !same)
            {
                mMaxSize = maxSize ? maxSize : DEFAULT_MAX_SIZE;
            }
            if (keep >= 0 || !same)
            {
                mKeep = keep >= 0 ? keep : DEFAULT_KEEP;
            }
            if (level != "-")
            {
                setLevel(parseLogLevel(level), 0);
            }
            if (same)
            {
                // idempotent: no new fd, no rotation, no extra copy of each line
                return "already logging to " + file + "\n" + where();
            }
            updateCapture();
            return "file logging on: " + file + " (max " + humanBytes(mMaxSize) + " x " + std::to_string(mKeep + 1) + " files)\n" + where();
        }
        if (cmd == "off")
        {
            bool was = mFile >= 0;
            closeFile();
            mRevertPending = false;
            setLogLevel(mStartLevel);
            updateCapture();
            return std::string(was ? "file logging off\n" : "file logging already off\n") + where();
        }
        if (cmd == "level") // level <L> <seconds>
        {
            std::string level;
            long secs;
            in >> level >> secs;
            if (parseLogLevel(level) < 0)
            {
                return "error: bad level\n";
            }
            setLevel(parseLogLevel(level), secs);
            return std::string("level ") + LEVEL_NAMES[logLevel()] + (secs > 0 ? " for " + std::to_string(secs) + "s" : "") + "\n" + where();
        }
        if (cmd == "tail") // tail <level|->
        {
            std::string level;
            in >> level;
            if (level != "-" && parseLogLevel(level) < 0)
            {
                return "error: bad level\n";
            }
            int restore = -1;
            if (level != "-")
            {
                restore = logLevel();
                setLogLevel(parseLogLevel(level));
            }
            mTailers.push_back({c, restore});
            updateCapture();
            return "";
        }
        return "error: unknown command\n";
    }

    void setLevel(int level, long secs)
    {
        if (secs > 0 && !mRevertPending)
        {
            mRevertTo = logLevel();
        }
        setLogLevel(level);
        mRevertPending = secs > 0;
        mRevertAt = std::chrono::steady_clock::now() + std::chrono::seconds(secs);
    }

    std::string fdTarget(int i)
    {
        char path[32], buf[512];
        std::snprintf(path, sizeof path, "/proc/self/fd/%d", mCapturing ? mSaved[i - 1] : i);
        ssize_t n = readlink(path, buf, sizeof buf - 1);
        return n > 0 ? std::string(buf, static_cast<size_t>(n)) : "unknown";
    }

    // One line every command ends with, so it is always clear where a process logs to.
    std::string where()
    {
        std::ostringstream o;
        o << "pid " << getpid() << " (" << mName << "): ";
        if (mFile >= 0)
        {
            o << "logging to " << mFilePath << " and default output";
        }
        else
        {
            o << "logging to default output (stdout " << fdTarget(1) << ", stderr " << fdTarget(2) << ")";
        }
        o << ", level " << LEVEL_NAMES[logLevel()] << "\n";
        return o.str();
    }

    std::string status()
    {
        std::ostringstream o;
        o << "process: " << mName << " (pid " << getpid() << ")\n";
        o << "level:   " << LEVEL_NAMES[logLevel()] << " (startup " << LEVEL_NAMES[mStartLevel] << ")";
        if (mRevertPending)
        {
            auto left = std::chrono::duration_cast<std::chrono::seconds>(mRevertAt - std::chrono::steady_clock::now()).count();
            o << ", back to " << LEVEL_NAMES[mRevertTo] << " in " << std::max<long long>(0, left) << "s";
        }
        o << "\nstdout:  " << fdTarget(1) << "\nstderr:  " << fdTarget(2) << "\nfile:    ";
        if (mFile >= 0)
        {
            o << mFilePath << " (" << humanBytes(mFileSize) << ", rotates at " << humanBytes(mMaxSize) << ", keeps " << mKeep << " old)";
        }
        else
        {
            o << "off";
        }
        o << "\ntail:    " << mTailers.size() << " client(s)\n";
        return o.str() + where();
    }
};

LogControl* gControl = nullptr;

} // namespace

void startLogControl(const std::string& name)
{
    if (gControl)
    {
        return;
    }
    gControl = new LogControl(name);
    if (!gControl->start())
    {
        delete gControl;
        gControl = nullptr;
        return;
    }
    // restores stdout/stderr and removes the socket on a normal exit; after a crash the
    // stale socket is cleaned up by the next client
    std::atexit(stopLogControl);
}

void stopLogControl()
{
    delete gControl;
    gControl = nullptr;
}

// ---- client ----

namespace {

struct Candidate
{
    std::string path, name;
    long pid;
};

std::vector<Candidate> liveSockets(const std::string& dir)
{
    std::vector<Candidate> out;
    DIR* d = opendir(dir.c_str());
    if (!d)
    {
        return out;
    }
    while (dirent* e = readdir(d))
    {
        std::string f = e->d_name;
        if (f.size() < 8 || f.compare(f.size() - 5, 5, ".sock") != 0)
        {
            continue;
        }
        std::string base = f.substr(0, f.size() - 5);
        size_t dot = base.rfind('.');
        if (dot == std::string::npos)
        {
            continue;
        }
        Candidate c{dir + "/" + f, base.substr(0, dot), std::strtol(base.c_str() + dot + 1, nullptr, 10)};
        int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        sockaddr_un a = makeAddr(c.path);
        if (connect(fd, reinterpret_cast<sockaddr*>(&a), sizeof a) == 0)
        {
            out.push_back(c);
        }
        else if (errno == ECONNREFUSED || errno == ENOENT)
        {
            unlink(c.path.c_str()); // owner died without cleaning up
        }
        close(fd);
    }
    closedir(d);
    std::sort(out.begin(), out.end(), [](const Candidate& x, const Candidate& y) { return x.pid < y.pid; });
    return out;
}

int usage()
{
    std::fprintf(stderr,
                 "usage: <program> log [--pid N | --name TEXT] <command>\n"
                 "  status                          where logs go, level, file and size\n"
                 "  on [--file PATH] [--level L] [--max-size 50M] [--keep 3]\n"
                 "                                  also write output to a size-capped rotating file\n"
                 "  off                             stop file logging, restore the startup level\n"
                 "  level L [--for 10m]             set the level (error|warn|info|debug), optionally auto-revert\n"
                 "  tail [--level L]                stream live output from the running process\n");
    return 2;
}

} // namespace

int logControlClient(int argc, char** argv)
{
    std::vector<std::string> args(argv, argv + argc);
    if (!args.empty() && (args[0] == "--help" || args[0] == "-h"))
    {
        usage();
        return 0;
    }
    long pid = 0;
    std::string name, cmd;
    std::string file;
    std::string level = "-";
    size_t maxSize = 0;
    int keep = -1;
    long secs = 0;
    auto bad = [](const std::string& what) {
        std::fprintf(stderr, "error: bad %s\n", what.c_str());
        return 2;
    };

    for (size_t i = 0; i < args.size(); i++)
    {
        const std::string& a = args[i];
        auto val = [&]() -> const std::string* { return i + 1 < args.size() ? &args[++i] : nullptr; };
        const std::string* v = nullptr;
        if (a == "--pid" || a == "--name" || a == "--file" || a == "--level" || a == "--max-size" || a == "--keep" || a == "--for")
        {
            if (!(v = val()))
            {
                return usage();
            }
            if (a == "--pid")
            {
                pid = std::strtol(v->c_str(), nullptr, 10);
            }
            else if (a == "--name")
            {
                name = *v;
            }
            else if (a == "--file")
            {
                file = *v;
            }
            else if (a == "--level")
            {
                level = *v;
            }
            else if (a == "--max-size")
            {
                if (!(maxSize = parseSize(*v)))
                {
                    return bad("--max-size (use e.g. 50M)");
                }
            }
            else if (a == "--keep")
            {
                keep = std::atoi(v->c_str());
                if (keep < 0 || (keep == 0 && *v != "0"))
                {
                    return bad("--keep");
                }
            }
            else if (!(secs = parseSeconds(*v)))
            {
                return bad("--for (use e.g. 10m)");
            }
        }
        else if (cmd.empty())
        {
            cmd = a;
        }
        else if (cmd == "level" && level == "-")
        {
            level = a;
        }
        else
        {
            return usage();
        }
    }
    if (cmd != "status" && cmd != "on" && cmd != "off" && cmd != "level" && cmd != "tail")
    {
        return usage();
    }
    if (level != "-" && parseLogLevel(level) < 0)
    {
        return bad("level (use error, warn, info or debug)");
    }
    if (cmd == "level" && level == "-")
    {
        return usage();
    }

    std::string dir = socketDir();
    if (!prepareDir(dir, false))
    {
        std::fprintf(stderr, "error: no bennu process with log control is running (no %s)\n", dir.c_str());
        return 1;
    }
    std::vector<Candidate> found;
    for (auto& c : liveSockets(dir))
    {
        if ((!pid || c.pid == pid) && (name.empty() || c.name.find(name) != std::string::npos))
        {
            found.push_back(c);
        }
    }
    if (found.empty())
    {
        std::fprintf(stderr, "error: no matching bennu process with log control is running\n");
        return 1;
    }
    if (found.size() > 1)
    {
        std::fprintf(stderr, "error: several processes match; pick one with --pid or --name:\n");
        for (auto& c : found)
        {
            std::fprintf(stderr, "  --pid %ld   (%s)\n", c.pid, c.name.c_str());
        }
        return 1;
    }

    std::string req = cmd;
    if (cmd == "on")
    {
        if (!file.empty() && file[0] != '/')
        {
            char cwd[4096];
            if (!getcwd(cwd, sizeof cwd))
            {
                return bad("--file");
            }
            file = std::string(cwd) + "/" + file;
        }
        if (!file.empty())
        {
            // resolve ./ ../ and symlinks in the directory so the same file always compares equal
            size_t slash = file.rfind('/');
            char real[4096];
            if (realpath(file.substr(0, slash ? slash : 1).c_str(), real))
            {
                file = std::string(real) + (real[1] ? "/" : "") + file.substr(slash + 1);
            }
        }
        req += " " + std::to_string(maxSize) + " " + std::to_string(keep) + " " + level + " " + file;
    }
    else if (cmd == "level")
    {
        req += " " + level + " " + std::to_string(secs);
    }
    else if (cmd == "tail")
    {
        req += " " + level;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    sockaddr_un a = makeAddr(found[0].path);
    if (connect(fd, reinterpret_cast<sockaddr*>(&a), sizeof a) != 0 || !sendAll(fd, req + "\n"))
    {
        std::fprintf(stderr, "error: cannot talk to pid %ld: %s\n", found[0].pid, std::strerror(errno));
        return 1;
    }
    std::string first;
    char buf[4096];
    ssize_t n;
    while ((n = read(fd, buf, sizeof buf)) > 0)
    {
        if (first.empty())
        {
            first.assign(buf, static_cast<size_t>(n));
        }
        ssize_t off = 0;
        while (off < n)
        {
            ssize_t w = write(cmd == "tail" || first.rfind("error:", 0) != 0 ? 1 : 2, buf + off, static_cast<size_t>(n - off));
            if (w <= 0)
            {
                return 1;
            }
            off += w;
        }
    }
    close(fd);
    return first.rfind("error:", 0) == 0 ? 1 : 0;
}

} // namespace utility
} // namespace bennu
