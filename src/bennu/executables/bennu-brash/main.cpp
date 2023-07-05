#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <csignal>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/tokenizer.hpp>

// get bennu version from CMakeLists.txt
#ifdef BENNU_VERSION
    #define VERSION BENNU_VERSION
#endif

using namespace std;
namespace io = boost::iostreams;

const string BABYSCEPTRE = "\033c \033[38;5;202m"
    "                                        ``.`      \r\n"
    "                                     .+syyyyyo:`  \r\n"
    "                                    ++-..:oyyyyy- \r\n"
    "  -- SCEPTRE |3rash --             `.`    `-/yyyy`\r\n"
    "                        `.---.`    /+-....:  +yyy-\r\n"
    "                     .-sssy:yy/oo:`:yyyyyyy+:syys`\r\n"
    "     .---``        .:/:- -/:://:oyyoyyyyyyy` +yo. \r\n"
    "  `+syyy/+ys/`    +yoooo -syooss+syys`:+ssy+:/.   \r\n"
    " -oooo+/-yyyys.  -.oo/-/ -/.+o-:ooosy/            \r\n"
    "`yyyyy/`-syyyys--+ooooo+ -o+//:-//+/++            \r\n"
    "`yyyyyo`y:syyyy++syyyyys /yyyyyyoo+/:.            \r\n"
    " +yyyyo yy/syy/  .yyyyys /yyyyyyyyyyy-            \r\n"
    "  :syy+ syyos-    -yyyys /yyyyyyyyyy:             \r\n"
    "    -/- /+:.       `+yys /yyyyyyyy+.              \r\n"
    "                     -yo :yyyso++y+               \r\n"
    "             .:+osssoyy`         :yo`             \r\n"
    "           .+ssyy+:/syys:         .ss::///:.      \r\n"
    "          -o-oo-:   `yyyy+        `+yyyys+yys/    \r\n"
    "          :  -y--   `yyyyy.      `syyyy-``.+syo`  \r\n"
    "          -  `y-.   `++++o-      +yyyys :/o :yy:  \r\n"
    "          `  `yy-   `yyyo:`      /yyyyy/:+/-syy:  \r\n"
    "             `yy-   `yyy+        `syyyyyo..syyo   \r\n"
    "             `sy-   `y+-          `/syyys `ys:    \r\n"
    "               ``                    .:/:  `      \r\n"
    "\033[m";

class Brash
{
public:
    Brash()
    {
        mBanner = "SCEPTRE Field-Device FW v" + string(VERSION) + ". Use 'help' for a list of commands.\r\n\r\n";
        mSceptreArt = BABYSCEPTRE;
        defaultCommands();
        generateHelpCommand();
    }

    void addCommands(map<string, pair<string, string>>& commandList)
    {
        mCommandList.insert(commandList.begin(), commandList.end());
        generateHelpCommand();
    }

    string processCommand(const string& comm) const
    {
        typedef boost::tokenizer<boost::char_separator<char>> tokenizer;
        boost::char_separator<char> sep("|");
        boost::char_separator<char> spaceSep(" ");
        tokenizer partCommands(comm, sep);

        string fullCommand;
        string command;
        string arguments;

        for (auto it = partCommands.begin(); it != partCommands.end(); ++it)
        {
            arguments = "";

            tokenizer tok(*it, spaceSep);
            command = *tok.begin();
            auto comIt = mCommandList.find(command);
            if (comIt == mCommandList.end())
            {
                return "echo -brash: command not found. Use 'help' to see a list of commands.";
            }

            for (auto itt = ++tok.begin(); itt != tok.end(); ++itt)
            {
                auto argIt = mArgsList.find(command);
                if (argIt == mArgsList.end())
                {
                    return "echo -brash: " + command + ": invalid option.";
                }

                if (argIt->second.front() == "allArgsAllowed")
                {
                    // Disallow special bash characters
                    if ((*itt).find('`') != string::npos || (*itt).find('>') != string::npos ||
                         (*itt).find('<') != string::npos || (*itt).find(';') != string::npos)
                        return "echo -brash: " + command + ": invalid option.";

                    arguments += " " + *itt;
                    continue;
                }

                if (find(argIt->second.begin(), argIt->second.end(), *itt) != argIt->second.end())
                {
                    arguments += " " + *itt;
                    continue;
                }

                return "echo " + command + ": invalid option.";
            }

            auto next = it;
            next++;

            fullCommand += comIt->second.first + arguments;

            if (next != partCommands.end())
            {
                fullCommand += " | ";
            }
        }
        return fullCommand;
    }

    string mBanner;
    string mSceptreArt;

private:
    void defaultCommands()
    {
        const string expInt = "eth0";  // Experiment network interface on the Field Device

        // These commands are passed directly to the bash (respecting user permissions)
        mCommandList["clear"]              = {"clear", "Clears screen."};
        mCommandList["time"]               = {"date", "Display the current date and time (UTC)."};
        mCommandList["passwd"]             = {"passwd", "Change current user password."};
        mCommandList["ifShow"]             = {"ip addr show lo && ip addr show " + expInt, "Display the attached network interfaces."};
        mCommandList["arpShow"]            = {"arp -i " + expInt, "Display entries in the system ARP table."};
        mCommandList["routeShow"]          = {"route | grep 'Destination\\|" + expInt + "'", "Display all IP routes (summary information)."};
        mCommandList["logShow"]            = {"cat /etc/sceptre/log/watcher-stdout.log", "Display log file."};
        mCommandList["configShow"]         = {"less /etc/sceptre/config.xml", "Display current device configuration."};
        mCommandList["inetstatShow"]       = {"ss | grep 'Netid' && IFADDR=`ip addr show " + expInt + " | grep -Po 'inet \\K[\\d.]+'` && ss |grep $IFADDR", "Display all active connections for IP sockets."};
        mCommandList["fieldDeviceStop"]    = {"bennu-field-deviced --c stop", "Stops field device (must be root)."};
        mCommandList["fieldDeviceStart"]   = {"bennu-field-deviced --c start --f /etc/sceptre/config.xml", "Starts field device (must be root)."};
        mCommandList["fieldDeviceRestart"] = {"bennu-field-deviced --c restart --f /etc/sceptre/config.xml", "Restarts field device (must be root)."};
        mCommandList["updateConfig"]       = {"cp /home/sceptre/config.xml /etc/sceptre/config.xml && bennu-field-deviced --c restart --f /etc/sceptre/config.xml",
                                              "Update field device configuration with a file uploaded via FTP (must be root)."};
        mCommandList["sceptre"]            = {"echo '" + mSceptreArt + "'", "Display SCEPTRE ASCII art."};

        // These commands are handled manually (handling logic is in the brash command loop)
        mCommandList["exit"]   = {"", "Exits shell."};
        mCommandList["su"]     = {"", "Switch to root."};

        // mArgsList map specifies what arguments are allowed for a command
        // If command doesn't appear in the map, then no arguments are allowed for that command
        // Below are examples are of specifying specific arguments and allowing all arguments.
        // mArgsList["ifShow"] = {"-s", "-v", "-a", "--help", "-h"};
        // mArgsList["grep"]     = {"allArgsAllowed"};
    }

    void generateHelpCommand()
    {
        string helpstr = "Command List:\r\n" + string(13, '-') + "\r\n";
        ostringstream strStream;
        for (const auto& it : mCommandList)
        {
            if (it.first == "help" || it.first == "sceptre") continue;
            strStream << setw(20) << left << it.first << left << it.second.second << endl;
        }
        strStream << setw(20) << left << "help" << left << "Prints this help." << endl;
        helpstr += strStream.str();
        mCommandList["help"] = {"printf '" + helpstr + "'", ""};
    }

    map<string, pair<string, string>> mCommandList;
    map<string, vector<string>> mArgsList;
};


int main(int, char**)
{
    typedef boost::tokenizer< boost::char_separator<char> > tokenizer;
    boost::char_separator<char> spaceSep(" ");
    vector<string> filteredCommand;

    string userinput;
    string command;
    string directory;

    char hname[128];
    gethostname(hname, sizeof hname);
    const string hostname(hname);

    uid_t uid;
    uid = getuid();
    // Assumes sticky bit is set on Brash ("chmod u+s bennu-brash")
    if (setresuid(uid, uid, 0) != 0)
    {
        std::cerr << "Error calling setresuid(): " << errno << std::endl;
    }

    Brash brash;

    string animCommand("pv -q -L 9600 < /etc/sceptre/brash/globe.vt");
    const char *animComm = animCommand.c_str();
    int value = system(animComm);
    if (value != 0)
    {
        std::cerr << "Error running animation command!" << std::endl;
    }
    cout << brash.mSceptreArt;
    cout << brash.mBanner;

    while (true)
    {
        try
        {
            char uname[L_cuserid];
            cuserid(uname);
            const string username(uname);

            cout << "\033[38;5;202m" << username << "@" << hostname << "# \033[m" << flush;
            cin.clear();
            getline(cin, userinput);
            tokenizer unfilteredCommand(userinput, spaceSep);
            if (unfilteredCommand.begin() == unfilteredCommand.end())
            {
                cout << "\r\n" << flush;
                continue;
            }

            filteredCommand.clear();
            command = *unfilteredCommand.begin();

            if (command == "exit")
            {
                if (getuid() == 0 and uid != 0)
                {
                    if (setresuid(uid, uid, 0) != 0)
                    {
                        std::cerr << "Error calling setresuid(): " << errno << std::endl;
                    }
                    continue;
                }
                return 0;
            }
            else if (command == "su")
            {
                ifstream passfile("/etc/sceptre/brash/password");
                string password;
                getline(passfile, password);

                cout << "Password: \033[30;40m" << flush;
                cin.clear();
                getline(cin, userinput);

                if (userinput == password)
                {
                    if (setresuid(0, 0, 0) != 0)
                    {
                        std::cerr << "Error calling setresuid(): " << errno << std::endl;
                    }
                    cout << "\033[m";
                }
                else
                {
                    cout << "\033[msu: Authentication failure\r\n";
                }
            }
            else if (command == "passwd" and getuid() == 0 and uid != 0)
            {
                ifstream passfile("/etc/sceptre/brash/password");
                string password;
                getline(passfile, password);

                cout << "Changing password for root." << endl;
                cout << "(current) root password: \033[30;40m" << flush;
                cin.clear();
                getline(cin, userinput);

                if (userinput == password)
                {
                    cout << "\033[mEnter new root password: \033[30;40m" << flush;
                    cin.clear();
                    getline(cin, userinput);
                    string userinput2;
                    cout << "\033[mRetype new root password: \033[30;40m" << flush;
                    cin.clear();
                    getline(cin, userinput2);

                    if (userinput == userinput2)
                    {
                        ofstream outpassfile;
                        outpassfile.open("/etc/sceptre/brash/password", ios::trunc);
                        outpassfile << userinput;
                        outpassfile.close();
                        cout << "\033[mpasswd: password updated successfully\r\n";
                        continue;
                    }
                    else
                    {
                        cout << "\033[mSorry, passwords do not match\r\n";
                    }
                }
                cout << "\033[mpasswd: Authentication token manipulation error\r\n";
                cout << "passwd: password unchanged\r\n";
                continue;
            }
            else
            {
                command = brash.processCommand(userinput);
                filteredCommand.emplace_back("sh");
                filteredCommand.emplace_back("-c");
                filteredCommand.emplace_back(command);
            }
        }
        catch (const exception&)
        {
            continue;
        }

        // Convert commands and args to char*[] for excecvp
        const unsigned int size = distance(filteredCommand.begin(), filteredCommand.end());
        char** execArgv = new char*[size + 1];
        int k = 0;
        for (const auto& it : filteredCommand)
        {
            execArgv[k] = new char[it.length() + 1];
            strcpy(execArgv[k], it.c_str());
            k++;
        }
        execArgv[size] = (char *)nullptr;

        // fork and run command
        pid_t kidpid = fork();
        if (kidpid < 0)
        {
            perror("Internal error: cannot fork.");
            return -1;
        }
        else if (kidpid == 0)
        {
            execvp(execArgv[0], execArgv);
            perror(execArgv[0]);
            return -1;
        }
        else
        {
            if (waitpid(kidpid, nullptr, 0) < 0)
            {
                perror("Internal error: cannot wait for child.");
                return -1;
            }
        }
    }
    return 0;
}
