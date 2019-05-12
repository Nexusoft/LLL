/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#include <Util/include/debug.h>

#include <Util/include/args.h>
#include <Util/include/config.h>
#include <Util/include/convert.h>
#include <Util/include/filesystem.h>
#include <Util/include/mutex.h>
#include <Util/include/runtime.h>
#include <Util/include/version.h>

#include <cstdarg>
#include <cstdio>
#include <map>
#include <vector>

#include <iostream>
#include <iomanip>

#ifndef WIN32
#include <execinfo.h>

#define _vsnprintf(a,b,c,d) vsnprintf(a,b,c,d)
#define strlwr(psz)         to_lower(psz)
#define _strlwr(psz)        to_lower(psz)

#else


#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600    //targeting minimum Windows Vista version for winsock2, etc.
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1  //prevents windows.h from including winsock.h and messing up winsock2.h definitions we use
#endif

#ifndef NOMINMAX
#define NOMINMAX //prevents windows.h from including min/max and potentially interfering with std::min/std::max
#endif

#include <windows.h>

#endif



namespace debug
{
    /* Mutex to lock DEBUG and prevent race conditions. */
    std::mutex DEBUG_MUTEX;

    /* Main file object for debug logging. */
    std::ofstream ssFile;

    /* Determine the last error generated by debug::error. */
    thread_local std::string strLastError;


    /* Special Specification for HTTP Protocol. */
    std::string rfc1123Time()
    {
        LOCK(DEBUG_MUTEX); //gmtime and localtime are not thread safe together

        char buffer[64];
        time_t now;
        time(&now);
        struct tm* now_gmt = gmtime(&now);
        std::string locale(setlocale(LC_TIME, nullptr));
        setlocale(LC_TIME, "C"); // we want posix (aka "C") weekday/month strings
        strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S +0000", now_gmt);
        setlocale(LC_TIME, locale.c_str());
        return std::string(buffer);
    }


    /*  Writes log output to console and debug file with timestamps.
     *  Encapsulated log for improved compile time. Not thread safe. */
    void log_(time_t &timestamp, std::string &debug_str)
    {
        #ifndef UNIT_TESTS

        /* Dump it to the console. */
        std::cout << "["
                  << std::put_time(std::localtime(&timestamp), "%H:%M:%S")
                  << "."
                  << std::setfill('0')
                  << std::setw(3)
                  << (runtime::timestamp(true) % 1000)
                  << "] "
                  << debug_str
                  << std::endl;

        /* Write it to the debug file. */
        ssFile    << "["
                  << std::put_time(std::localtime(&timestamp), "%H:%M:%S")
                  << "."
                  << std::setfill('0')
                  << std::setw(3)
                  << (runtime::timestamp(true) % 1000)
                  << "] "
                  << debug_str
                  << std::endl;
        #endif
    }


    /* Write startup information into the log file */
    void InitializeLog(int argc, char** argv)
    {
        log(0, "Startup time ", convert::DateTimeStrFormat(runtime::timestamp()));
        log(0, version::CLIENT_VERSION_BUILD_STRING);

    #ifdef WIN32
        log(0, "Microsoft Windows Build (created ", version::CLIENT_DATE, ")");
    #else
        #ifdef MAC_OSX
        log(0, "Mac OSX Build (created ", version::CLIENT_DATE, ")");
        #else
        log(0, "Linux Build (created ", version::CLIENT_DATE, ")");
        #endif
    #endif


        std::string pathConfigFile = config::GetConfigFile();
        if (!filesystem::exists(pathConfigFile))
            log(0, "No configuration file");

        else
        {
            log(0, "Using configuration file: ", pathConfigFile);


            /* Log configuration file parameters. Need to read them into our own map copy first */
            std::map<std::string, std::string> mapBasicConfig;  //not used
            std::map<std::string, std::vector<std::string> > mapMultiConfig; //All values stored here whether multi or not, will use this

            config::ReadConfigFile(mapBasicConfig, mapMultiConfig);

            std::string confFileParams = "";

            for (const auto& argItem : mapMultiConfig)
            {
                for (int i = 0; i < argItem.second.size(); i++)
                {
                    confFileParams += argItem.first;

                    if (argItem.first.compare(0, 12, "-rpcpassword") == 0)
                        confFileParams += "=xxxxx";
                    else if (!argItem.second[i].empty())
                        confFileParams += "=" + argItem.second[i];

                    confFileParams += " ";
                }
            }

            if (confFileParams == "")
                confFileParams = "(none)";

            log(0, "Configuration file parameters: ", confFileParams);
        }


        /* Log command line parameters (which can override conf file settings) */
        std::string cmdLineParms = "";

        for (int i = 1; i < argc; i++)
        {

            if (std::string(argv[i]).compare(0, 12, "-rpcpassword") == 0)
                cmdLineParms += "-rpcpassword=xxxxx ";
            else
                cmdLineParms += std::string(argv[i]) + " ";
        }

        if (cmdLineParms == "")
            cmdLineParms = "(none)";

        log(0, "Command line parameters: ", cmdLineParms);

        log(0, "");
        log(0, "");
    }


    /*  Open the debug log file. */
    bool init(std::string debugPath)
    {
        LOCK(DEBUG_MUTEX);

        ssFile.open(debugPath, std::ios::app | std::ios::in | std::ios::out);
        return ssFile.is_open();
    }


    /*  Close the debug log file. */
    void shutdown()
    {
        LOCK(DEBUG_MUTEX);

        if(ssFile.is_open())
            ssFile.close();
    }

    /* Gets the last error string logged via debug::error and clears the last error */
    std::string GetLastError()
    {
        std::string strRet = strLastError;
        strLastError = "";
        return strRet;
    }

}
