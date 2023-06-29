#include "BrashServer.hpp"

#include <syslog.h>
#include <unistd.h>
#include <csignal>
#include <cstring>
#include <fstream>
#include <iostream>

namespace bennu {
namespace shell {

BrashServer::BrashServer() :
    mUserID(1001),
    mUserHome("/home/sceptre"),
    mLockFile("/var/run/shell-server.pid")
{
}

bool BrashServer::isRunning() const
{
    // If the LOCKFILE cannot be opened, then we're going to assume that this is the first time
    // we've run the device-shell on this machine.
    std::ifstream is(mLockFile.c_str());
    if (!is.is_open())
    {
        syslog(LOG_INFO, "Unable to open the lockfile %s.", mLockFile.c_str());
        return false;
    }

    pid_t pid = 0;
    is >> pid;

    if (pid != 0)
    {
        return true;
    }

    return false;
}

int BrashServer::start()
{
    pid_t pid = fork();

    if (pid == 0)
    {
        // child process
        std::ofstream os(mLockFile.c_str(), std::fstream::trunc);
        os << getpid();
        os.close();

        if (chdir(mUserHome.c_str()) != 0)
        {
            std::cerr << "Error changing directory in BrashServer: " << errno << std::endl;
        }
        //setresuid(mUserID, mUserID, 0);

        char** argv = new char*[4];
        std::string args[] = {
            "socat",
            "TCP-LISTEN:1337,reuseaddr,fork,crlf",
            "SYSTEM:'clear && cat /etc/issue.net && /bin/login',pty,stderr,setsid,sane,echo=0"
        };
        for (int i = 0; i < 3; i++)
        {
            argv[i] = new char[args[i].length() + 1];
            strcpy(argv[i], args[i].c_str());
        }
        argv[3] = static_cast<char *>(nullptr);

        std::string tmp("TERM=xterm");
        char *envp[] = {tmp.data(), nullptr};
        execvpe(argv[0], argv, envp);

    }
    else if (pid > 0)
    {
        // parent process
        return 0;

    }
    else
    {
        // fork failed
        return 1;
    }
    return 0;
}

void BrashServer::stop()
{
    std::ifstream is;
    is.open (mLockFile.c_str());

    pid_t pid = 0;
    is >> pid;
    is.close();

    if (pid != 0)
    {
        kill(pid, SIGTERM);
        sleep(1);
        kill(pid, SIGKILL);
    }

    std::ofstream os(mLockFile.c_str(), std::fstream::trunc);
    os << 0;
    os.close();
}

} // namespace shell
} // namespace bennu
