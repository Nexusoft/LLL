/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2018] ++

            (c) Copyright The Nexus Developers 2014 - 2018

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#include <Util/include/args.h>
#include <Util/include/config.h>
#include <Util/include/mutex.h>
#include <Util/include/filesystem.h>
#include <fstream>
#include <cstring> //for strlen

/* Read the Config file from the Disk. */
void ReadConfigFile(std::map<std::string, std::string>& mapSettingsRet,
                    std::map<std::string, std::vector<std::string> >& mapMultiSettingsRet)
{
    std::ifstream streamConfig(GetConfigFile());
    if(!streamConfig.is_open())
        return; // No nexus.conf file is OK

    std::string line;

    while(!streamConfig.eof())
    {
        std::getline(streamConfig, line);

        if(streamConfig.eof())
            break;

        size_t i = line.find('=');
        if(i == std::string::npos)
            continue;

        std::string strKey = std::string("-") + std::string(line, 0, i);
        std::string strVal = std::string(line, i + 1, line.size() - i - 1);

        if(mapSettingsRet.count(strKey) == 0)
        {
            mapSettingsRet[strKey] = strVal;
            // interpret nofoo=1 as foo=0 (and nofoo=0 as foo=1) as long as foo not set
            InterpretNegativeSetting(strKey, mapSettingsRet);
        }
        
        mapMultiSettingsRet[strKey].push_back(strVal);
    }
    streamConfig.close();
}


/* Setup PID file for Linux users. */
void CreatePidFile(const std::string &path, pid_t pid)
{
    FILE* file = fopen(path.c_str(), "w");
    if (file)
    {
        fprintf(file, "%d\n", pid);
        fclose(file);
    }
}


#ifdef WIN32
std::string MyGetSpecialFolderPath(int nFolder, bool fCreate)
{
    char pszPath[MAX_PATH] = "";
    std::string p;

    if(SHGetSpecialFolderPathA(NULL, pszPath, nFolder, fCreate))
        p = pszPath;
    else if (nFolder == CSIDL_STARTUP)
    {
        p = getenv("USERPROFILE");
        p.append("\\Start Menu");
        p.append("\\Programs");
        p.append("\\Startup");
    }
    else if (nFolder == CSIDL_APPDATA)
        p = getenv("APPDATA");

    return p;
}
#endif


/* Get the default directory Nexus data is stored in. */
std::string GetDefaultDataDir(std::string strName)
{
    // Windows: C:\Documents and Settings\username\Application Data\Nexus
    // Mac: ~/Library/Application Support/Nexus
    // Unix: ~/.Nexus
#ifdef WIN32
    // Windows
    pathRet = MyGetSpecialFolderPath(CSIDL_APPDATA, true);
    pathRet.append("\\" + strName + "\\");
#else
    std::string pathRet;    
    char* pszHome = getenv("HOME");
    if (pszHome == NULL || strlen(pszHome) == 0)
        pathRet = "/";
    else
        pathRet = pszHome;
#ifdef MAC_OSX
    // Mac
    pathRet.append("Library/Application Support");
    create_directories(pathRet);
    pathRet.append("/" + strName + "/");
#else
    // Unix
    pathRet.append("/." + strName + "/");
#endif

    return pathRet;
#endif
}


/* Get the Location of the Config File. */
std::string GetConfigFile()
{
    std::string pathConfigFile(GetDataDir(false));

    pathConfigFile.append(GetArg("-conf", "nexus.conf"));

    return pathConfigFile;
}


/* Get the Location of the PID File. */
std::string GetPidFile()
{
    std::string pathPidFile(GetDataDir());

    pathPidFile.append(GetArg("-pid", "nexus.pid"));

    return pathPidFile;
}


/* Get the location that Nexus data is being stored in. */
std::string GetDataDir(bool fNetSpecific)
{
    static std::string pathCached[2];
    static Mutex_t csPathCached;
    static bool cachedPath[2] = {false, false};

    std::string &path = pathCached[fNetSpecific];

    // This can be called during exceptions by printf, so we cache the
    // value so we don't have to do memory allocations after that.
    if (cachedPath[fNetSpecific])
        return path;

    LOCK(csPathCached);

    if (mapArgs.count("-datadir")) 
    {
        path = system_complete(mapArgs["-datadir"]);        

        if(is_directory(path) == false)
        {
            path = "";
            return path;
        }
    } 
    else 
        path = GetDefaultDataDir();

    if (fNetSpecific && GetBoolArg("-testnet", false))
        path.append("testnet/");

    create_directories(path);

    cachedPath[fNetSpecific]=true;
    return path;
}


#ifdef WIN32
std::string static StartupShortcutPath()
{
    std::string str = MyGetSpecialFolderPath(CSIDL_STARTUP, true); 
    return str.append("\\nexus.lnk");
}


bool GetStartOnSystemStartup()
{
    return exists(StartupShortcutPath());
}


bool SetStartOnSystemStartup(bool fAutoStart)
{
    // If the shortcut exists already, remove it for updating
    remove(StartupShortcutPath());

    if (fAutoStart)
    {
        CoInitialize(NULL);

        // Get a pointer to the IShellLink interface.
        IShellLink* psl = NULL;
        HRESULT hres = CoCreateInstance(CLSID_ShellLink, NULL,
                                CLSCTX_INPROC_SERVER, IID_IShellLink,
                                reinterpret_cast<void**>(&psl));

        if (SUCCEEDED(hres))
        {
            // Get the current executable path
            TCHAR pszExePath[MAX_PATH];
            GetModuleFileName(NULL, pszExePath, sizeof(pszExePath));

            TCHAR pszArgs[5] = TEXT("-min");

            // Set the path to the shortcut target
            psl->SetPath(pszExePath);
            PathRemoveFileSpec(pszExePath);
            psl->SetWorkingDirectory(pszExePath);
            psl->SetShowCmd(SW_SHOWMINNOACTIVE);
            psl->SetArguments(pszArgs);

            // Query IShellLink for the IPersistFile interface for
            // saving the shortcut in persistent storage.
            IPersistFile* ppf = NULL;
            hres = psl->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&ppf));
            if (SUCCEEDED(hres))
            {
                WCHAR pwsz[MAX_PATH];
                // Ensure that the string is ANSI.
                MultiByteToWideChar(CP_ACP, 0, StartupShortcutPath().c_str(), -1, pwsz, MAX_PATH);
                // Save the link by calling IPersistFile::Save.
                hres = ppf->Save(pwsz, TRUE);
                ppf->Release();
                psl->Release();
                CoUninitialize();
                return true;
            }
            psl->Release();
        }
        CoUninitialize();
        return false;
    }
    return true;
}

#elif defined(LINUX)

// Follow the Desktop Application Autostart Spec:
//  http://standards.freedesktop.org/autostart-spec/autostart-spec-latest.html


std::string static GetAutostartDir()
{
    std::string autostart_dir;

    char* pszHome = getenv("XDG_CONFIG_HOME");
    if (pszHome) 
    {
        autostart_dir = pszHome;
        autostart_dir.append("autostart/");
    }
    else
    {
        pszHome = getenv("HOME");
        if (pszHome)
        {
            autostart_dir = pszHome;
            autostart_dir.append(".config/autostart/"); 
        }
    }

    return autostart_dir;
}


std::string static GetAutostartFilePath()
{
    return GetAutostartDir().append("nexus.desktop");
}


bool GetStartOnSystemStartup()
{
    std::ifstream optionFile(GetAutostartFilePath());
    if (!optionFile.good())
        return false;
    
    // Scan through file for "Hidden=true":
    std::string line;
    while (!optionFile.eof())
    {
        std::getline(optionFile, line);
        if (line.find("Hidden") != std::string::npos &&
            line.find("true") != std::string::npos)
            return false;
    }
    optionFile.close();

    return true;
}


bool SetStartOnSystemStartup(bool fAutoStart)
{
    if (!fAutoStart)
        remove(GetAutostartFilePath());
    else
    {
        char pszExePath[MAX_PATH+1];
        memset(pszExePath, 0, sizeof(pszExePath));
        if (readlink("/proc/self/exe", pszExePath, sizeof(pszExePath)-1) == -1)
            return false;

        create_directories(GetAutostartDir());

        std::ofstream optionFile(GetAutostartFilePath(), 
            std::ios_base::out | std::ios_base::trunc);

        if (!optionFile.good())
            return false;

        // Write a nexus.desktop file to the autostart directory:
        optionFile << "[Desktop Entry]\n";
        optionFile << "Type=Application\n";
        optionFile << "Name=Nexus\n";
        optionFile << "Exec=" << pszExePath << " -min\n";
        optionFile << "Terminal=false\n";
        optionFile << "Hidden=false\n";
        optionFile.close();
    }
    return true;
}
#else

// TODO: OSX startup stuff; see:
// http://developer.apple.com/mac/library/documentation/MacOSX/Conceptual/BPSystemStartup/Articles/CustomLogin.html

bool GetStartOnSystemStartup() { return false; }
bool SetStartOnSystemStartup(bool fAutoStart) { return false; }

#endif
