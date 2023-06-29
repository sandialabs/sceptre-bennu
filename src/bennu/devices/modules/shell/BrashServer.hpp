#ifndef BENNU_FIELDDEVICE_SHELL_BRASHSERVER_HPP
#define BENNU_FIELDDEVICE_SHELL_BRASHSERVER_HPP

#include <string>

namespace bennu {
namespace shell {

class BrashServer
{
public:
    BrashServer();

	bool isRunning() const;

    int start();

    void stop();

private:
    const int mUserID;
    const std::string mUserName;
    const std::string mUserHome;
    const std::string mLockFile;
    const std::string mShellType;
    const std::string mBinaryLoc;

};

} // namespace shell
} // namespace bennu

#endif // BENNU_FIELDDEVICE_SHELL_BRASHSERVER_HPP
