#include "doctest.h"
#include <fstream>
#include <string>

extern std::string exec(const char*);
extern void stopProcesses(const std::string& name);

// Runtime log control of a running field device: status, file logging with rotation cap,
// level with auto-revert, live tail, socket permissions, discovery and stale-socket cleanup.
TEST_CASE("testing runtime log control")
{
    const std::string fd = "bennu-field-device";
    auto sh = [](const std::string& c) { return exec((c + " 2>&1").c_str()); };
    auto has = [](const std::string& hay, const std::string& needle) { return hay.find(needle) != std::string::npos; };

    // Three internal tags that change every 50 ms cycle: at debug level the logic logs every
    // rule every cycle (a few KB/s); at info it logs nothing.
    sh("rm -rf lc && mkdir lc");
    std::ofstream("lc/a.xml") << R"(<?xml version="1.0" encoding="utf-8"?>
<SCEPTRE><field-device><name>lc-1</name><cycle-time>50</cycle-time>
<tags>
<internal-tag><name>c0</name><value>0.0</value></internal-tag>
<internal-tag><name>c1</name><value>0.0</value></internal-tag>
<internal-tag><name>c2</name><value>0.0</value></internal-tag>
</tags>
<logic><![CDATA[
c0 = c0 + 1
c1 = c1 + 1
c2 = c2 + 1
]]></logic></field-device></SCEPTRE>
)";
    sh("cp lc/a.xml lc/b.xml");
    std::string sockdir = "/tmp/bennu-$(id -u)";
    stopProcesses("bennu-field-device"); // left over from other tests
    sh("rm -f " + sockdir + "/*.sock");
    sh("unset BENNU_LOG_LEVEL; " + fd + " --f lc/a.xml >lc/a.out 2>&1 & echo $! > lc/a.pid; sleep 2");

    SUBCASE("sockets are private")
    {
        CHECK(sh("stat -c %a " + sockdir) == "700\n");
        CHECK(sh("stat -c %a " + sockdir + "/*.sock") == "600\n");
    }

    SUBCASE("status, file with rotation cap, level, tail, off")
    {
        std::string st = sh(fd + " log status");
        CHECK(has(st, "level:   info"));
        CHECK(has(st, "file:    off"));
        CHECK(has(st, "lc/a.out"));

        // info level: no per-cycle logic lines yet
        CHECK(sh("grep -c LOGIC lc/a.out") == "0\n");

        CHECK(has(sh(fd + " log on --file lc/x.log --level debug --max-size 2K --keep 2"), "logging to"));
        sh("sleep 3");
        CHECK(has(sh(fd + " log status"), "level:   debug"));
        CHECK(sh("grep -c LOGIC lc/x.log") != "0\n");
        CHECK(sh("grep -c LOGIC lc/a.out") != "0\n");   // default output unchanged
        CHECK(sh("ls lc | grep -c '^x.log'") == "3\n"); // x.log, .1, .2; never a .3
        // 2K cap plus at most one 4K read per file
        CHECK(sh("find lc -name 'x.log*' -size +6k | wc -l") == "0\n");
        CHECK(sh("stat -c %a lc/x.log") == "600\n");

        // temporary level
        CHECK(has(sh(fd + " log level warn --for 2s"), "level warn for 2s"));
        CHECK(has(sh(fd + " log status"), "level:   warn"));
        CHECK(has(sh(fd + " log status"), "back to debug"));
        sh("sleep 3");
        CHECK(has(sh(fd + " log status"), "level:   debug"));

        // tail needs no file; --level only lasts as long as the tail
        sh(fd + " log off");
        CHECK(has(sh(fd + " log status"), "level:   info"));
        CHECK(has(sh(fd + " log status"), "file:    off"));
        std::string t = sh("timeout 2 " + fd + " log tail --level debug");
        CHECK(has(t, "LOGIC"));
        sh("sleep 1");
        CHECK(has(sh(fd + " log status"), "level:   info"));
        CHECK(has(sh(fd + " log status"), "0 client"));

        // off really closed the file
        std::string size1 = sh("stat -c %s lc/x.log");
        sh("sleep 1");
        CHECK(sh("stat -c %s lc/x.log") == size1);

        CHECK(has(sh(fd + " log level loud"), "bad level"));
    }

    SUBCASE("on and off are idempotent and say where logs go")
    {
        std::string first = sh(fd + " log on --file lc/i.log --level debug --max-size 1M");
        std::string abs = sh("realpath lc/i.log");
        abs.pop_back();
        CHECK(has(first, "file logging on: " + abs));
        CHECK(has(first, "pid "));
        CHECK(has(first, "level debug"));
        std::string sinks = sh("ls /proc/$(cat lc/a.pid)/fd -l | grep -c i.log");
        for (int i = 0; i < 2; i++)
        {
            std::string again = sh(fd + " log on --file lc/i.log");
            CHECK(has(again, "already logging to " + abs));
            CHECK(has(again, "level debug"));
        }
        CHECK(sh("ls /proc/$(cat lc/a.pid)/fd -l | grep -c i.log") == sinks); // one fd, no second sink
        CHECK(sinks == "1\n");
        CHECK(sh("ls lc | grep -c '^i.log'") == "1\n"); // no rotation, no second file
        sh("sleep 1");
        // the cap given the first time survived the repeats
        CHECK(has(sh(fd + " log status"), "rotates at 1.0 MB"));

        // switching closes the old file
        std::string sw = sh(fd + " log on --file lc/j.log");
        CHECK(has(sw, "file logging on:"));
        CHECK(sh("ls /proc/$(cat lc/a.pid)/fd -l | grep -c i.log") == "0\n");
        CHECK(sh("ls /proc/$(cat lc/a.pid)/fd -l | grep -c j.log") == "1\n");

        std::string off = sh(fd + " log off");
        CHECK(has(off, "file logging off"));
        CHECK(has(off, "default output"));
        CHECK(has(off, "lc/a.out"));
        std::string off2 = sh(fd + " log off");
        CHECK(has(off2, "already off"));
        CHECK(has(off2, "lc/a.out"));
        CHECK(sh("ls /proc/$(cat lc/a.pid)/fd -l | grep -c 'i.log\\|j.log'") == "0\n");
        CHECK(has(sh(fd + " log level warn"), "level warn"));
        // three "log on" must not triple the lines: the file is a subset of the default output
        CHECK(sh("[ $(grep -c LOGIC lc/i.log) -gt 0 ] && [ $(grep -c LOGIC lc/i.log) -le $(grep -c LOGIC lc/a.out) ] && echo ok") == "ok\n");
    }

    SUBCASE("discovery and stale sockets")
    {
        sh("unset BENNU_LOG_LEVEL; " + fd + " --f lc/b.xml >lc/b.out 2>&1 & echo $! > lc/b.pid; sleep 2");
        std::string many = sh(fd + " log status");
        CHECK(has(many, "several processes"));
        CHECK(has(many, "--pid"));
        CHECK(has(sh(fd + " log status --pid $(cat lc/a.pid)"), "level:   info"));

        sh("kill -9 $(cat lc/b.pid); sleep 0.5");
        CHECK(has(sh(fd + " log status"), "level:   info"));      // one live process again
        CHECK(sh("ls " + sockdir + " | grep -c sock$") == "1\n"); // stale one removed
    }

    // a normal exit (SIGINT) removes its socket
    sh("kill -INT $(cat lc/a.pid); sleep 1");
    CHECK(sh("ls " + sockdir + " | grep -c sock$") == "0\n");
    stopProcesses("bennu-field-device");
}
