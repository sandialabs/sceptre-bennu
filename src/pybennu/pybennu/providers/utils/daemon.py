##############################################################################
# This code was taken from Gist marazmiki/3618191
##############################################################################
import atexit
import os
import signal
import sys
import time

class Daemon(object):
    """
    A generic daemon class.

    Usage: subclass the Daemon class and override the run() method
    """

    def __init__(self, pidfile, stdin='/dev/null',
                 stdout='/dev/null', stderr='/dev/null'):
        self.pidfile = pidfile
        self.stdin   = stdin
        self.stdout  = stdout
        self.stderr  = stderr

    def daemonize(self):
        """
        do the UNIX double-fork magic, see Stevens' "Advanced
        Programming in the UNIX Environment" for details (ISBN 0201563177)
        http://www.erlenstar.demon.co.uk/unix/faq_2.html#SEC16
        """

        # Do first fork
        self.fork()

        # Decouple from parent environment
        self.dettach_env()

        # Do second fork
        self.fork()

        # write pidfile
        self.create_pidfile()

        # Flush standard file descriptors
        sys.stdout.flush()
        sys.stderr.flush()

        # Redirect standard file descriptors
        si = open(self.stdin,  'r')
        so = open(self.stdout, 'a+')
        se = open(self.stderr, 'ab+', 0)

        os.dup2(si.fileno(), sys.stdin.fileno())
        os.dup2(so.fileno(), sys.stdout.fileno())
        os.dup2(se.fileno(), sys.stderr.fileno())

    def dettach_env(self):
        os.chdir("/")
        os.setsid()
        os.umask(0)

    def fork(self):
        """
        Spawn the child process
        """

        try:
            pid = os.fork()
            if pid > 0:
                sys.exit(0)
        except OSError as err:
            sys.stderr.write("Fork failed: %d (%s)\n" % (err.errno, err.strerror))
            sys.exit(1)

    def create_pidfile(self):
        atexit.register(self.del_pidfile)

        pid = str(os.getpid())
        open(self.pidfile,'w+').write("%s\n" % pid)

        sys.stderr.write('\nProcess started with PID %s\n' % (pid))

    def del_pidfile(self):
        """
        Removes the pidfile on process exit
        """

        os.remove(self.pidfile)

    def start(self):
        """
        Start the daemon
        """

        # Check for a pidfile to see if the daemon already runs
        pid = self.get_pid()

        if pid:
            message = "pidfile %s already exist. Daemon already running?\n"
            sys.stderr.write(message % self.pidfile)
            sys.exit(1)

        # Start the daemon
        self.daemonize()
        self.run()

    def get_pid(self):
        """
        Returns the PID from pidfile
        """

        try:
            pf  = open(self.pidfile,'r')
            pid = int(pf.read().strip())
            pf.close()
        except (IOError, TypeError):
            pid = None
        return pid

    def stop(self, silent = False):
        """
        Stop the daemon
        """

        # Get the pid from the pidfile
        pid = self.get_pid()

        if not pid:
            if not silent:
                message = "pidfile %s does not exist. Daemon not running?\n"
                sys.stderr.write(message % self.pidfile)
            return # not an error in a restart

        # Try killing the daemon process
        try:
            while True:
                os.kill(pid, signal.SIGTERM)
                time.sleep(0.1)
        except OSError as err:
            err = str(err)
            if err.find("No such process") > 0:
                if os.path.exists(self.pidfile):
                    os.remove(self.pidfile)
            else:
                sys.stdout.write(str(err))
                sys.exit(1)

    def restart(self):
        """
        Restart the daemon
        """

        self.stop(silent = True)
        self.start()

    def run(self):
        """
        You should override this method when you subclass Daemon. It will be called after the process has been
        daemonized by start() or restart().
        """

        raise NotImplementedError
