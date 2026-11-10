#ifndef BENNU_UTILITY_LOGCONTROL_HPP
#define BENNU_UTILITY_LOGCONTROL_HPP

#include <string>

namespace bennu {
namespace utility {

// Starts a thread serving a Unix domain socket (owner-only, never TCP) at
// /tmp/bennu-<uid>/<name>.<pid>.sock so `<program> log ...` can change the log level, tee
// stdout/stderr into a size-capped rotating file, or stream live output, without a restart.
// Call once, after the process has set up its stdout/stderr. Without a client nothing changes.
void startLogControl(const std::string& name);

// Stops the thread, restores stdout/stderr and removes the socket. Also runs at exit, but
// call it before returning from main: destructors of other singletons can crash first.
void stopLogControl();

// Implements `<program> log status|on|off|level|tail`; args are those after "log".
// Returns the process exit code.
int logControlClient(int argc, char** argv);

} // namespace utility
} // namespace bennu

#endif
