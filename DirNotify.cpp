
#include <string>
#include <algorithm>
#include <regex>

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#else
#include <codecvt>
#include <locale>
#include <syslog.h>
#include <unistd.h>
#endif

#include "Service.h"
#include "ConfFile.h"
#include "FilesysNotify.h"

#if !defined (_WIN32) && !defined (_WIN64)
#include <locale>
#include <iomanip>
#include <codecvt>
#include <fcntl.h>
#include <unistd.h>
void OutputDebugString(const wchar_t* pOut)
{   // mkfifo /tmp/dbgout  ->  tail -f /tmp/dbgout
    int fdPipe = open("/tmp/dbgout", O_WRONLY | O_NONBLOCK);
    if (fdPipe >= 0)
    {
        wstring strTmp(pOut);
        write(fdPipe, wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().to_bytes(strTmp).c_str(), strTmp.size());
        close(fdPipe);
    }
}
void OutputDebugStringA(const char* pOut)
{   // mkfifo /tmp/dbgout  ->  tail -f /tmp/dbgout
    int fdPipe = open("/tmp/dbgout", O_WRONLY | O_NONBLOCK);
    if (fdPipe >= 0)
    {
        std::string strTmp(pOut);
        write(fdPipe, strTmp.c_str(), strTmp.size());
        close(fdPipe);
    }
}
#endif

int main(int argc, const char* argv[])
{
    SrvParam svParam;
#if defined(_WIN32) || defined(_WIN64)
    svParam.szDspName = L"Example Service";                 // Servicename in Service control manager of windows
    svParam.szDescribe = L"Example Service by your name";    // Description in Service control manager of windows
#endif
    svParam.szSrvName = L"dirnotify";                     // Service name (service id)

    std::wstring m_strModulePath;
    typedef struct tagWatchItem
    {
        std::wstring strWatchItem;
        uint32_t nWatchTyp;
        uint32_t nLimitAnz;
        uint32_t nLimitSek;
        uint32_t nCount;
        time_t tLastNull;
        std::regex reFilter;
    }WATCHITEM;
    std::map<int, WATCHITEM> mapWatches;
    std::mutex mxWatchList;

    svParam.fnStartCallBack = [&m_strModulePath, &mapWatches, &mxWatchList]()
    {
        m_strModulePath = std::wstring(FILENAME_MAX, 0);
#if defined(_WIN32) || defined(_WIN64)
        if (GetModuleFileName(nullptr, &m_strModulePath[0], FILENAME_MAX) > 0)
            m_strModulePath.erase(m_strModulePath.find_last_of(L'\\') + 1); // Sollte der Backslash nicht gefunden werden wird der ganz String gelöscht

        if (_wchdir(m_strModulePath.c_str()) != 0)
            m_strModulePath = L"./";
#else
        std::string strTmpPath(FILENAME_MAX, 0);
        if (readlink(std::string("/proc/" + std::to_string(getpid()) + "/exe").c_str(), &strTmpPath[0], FILENAME_MAX) > 0)
            strTmpPath.erase(strTmpPath.find_last_of('/'));

        //Change Directory
        //If we cant find the directory we exit with failure.
        if ((chdir(strTmpPath.c_str())) < 0) // if ((chdir("/")) < 0)
            strTmpPath = ".";
        m_strModulePath = std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().from_bytes(strTmpPath) + L"/";
#endif

        // Start you server here
        syslog(LOG_NOTICE, "StartCallBack called ");

        ConfFile& conf = ConfFile::GetInstance(m_strModulePath + L"dirnotify.cfg");
        FileSysNotify& cFsysNotify = FileSysNotify::GetInstance();
        cFsysNotify.SetCallBackFunction([&](int iWatch, uint32_t nMask, uint32_t /*nCookie*/, std::string strName)
        {
            auto itMonitor = mapWatches.find(iWatch);
            if (itMonitor != mapWatches.end() && nMask & itMonitor->second.nWatchTyp)
            {
                if (std::regex_match(strName, itMonitor->second.reFilter) == true)
                {
                    std::string strDebug;
                    itMonitor->second.nCount++;
                    time_t tNow = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                    if (itMonitor->second.nLimitSek != 0 && tNow - itMonitor->second.tLastNull > itMonitor->second.nLimitSek)
                    {
                        strDebug += "Zeit abgelaufen, Dif-Time=" + std::to_string(tNow - itMonitor->second.tLastNull) + ", SollTime=" + std::to_string(itMonitor->second.nLimitSek) + ", ";
                        itMonitor->second.nCount = 1;
                        itMonitor->second.tLastNull = tNow;
                    }

                    if (itMonitor->second.nCount >= itMonitor->second.nLimitAnz)
                    {
                        strDebug += "Max erreicht, Anzahl=" + std::to_string(itMonitor->second.nCount) + ", nLimitAnz: " + std::to_string(itMonitor->second.nLimitAnz) + ", Dif-Time=" + std::to_string(tNow - itMonitor->second.tLastNull);
                        itMonitor->second.nCount = 0;
                        itMonitor->second.tLastNull = tNow;

                        std::wstring strAction = conf.getUnique(itMonitor->second.strWatchItem, L"actiontyp");
                        std::transform(std::begin(strAction), std::end(strAction), std::begin(strAction), [](wchar_t c) noexcept { return static_cast<wchar_t>(::toupper(c)); });
                        std::wstring strActionParam = conf.getUnique(itMonitor->second.strWatchItem, L"actionpara");
                        if (strAction == L"SYSLOG")
                            syslog(LOG_NOTICE, "%s", &strName[0]);
                        if (strAction == L"SYSTEM")
                            system(std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().to_bytes(strActionParam).c_str());
                    }
                    if (strDebug.empty() == false)
                    {
                        strDebug += "\n";
                        OutputDebugStringA(strDebug.c_str());
                    }
                }
            }
        });

        vector<wstring> vSections = conf.get();
        for (auto& strItem : vSections)
        {
            std::wstring strMonitor = conf.getUnique(strItem, L"monitortyp");
            std::transform(std::begin(strMonitor), std::end(strMonitor), std::begin(strMonitor), [](wchar_t c) noexcept { return static_cast<wchar_t>(::toupper(c)); });

            uint32_t nMask{0};  // IN_CREATE | IN_DELETE | IN_OPEN | IN_CLOSE
            if (strMonitor == L"CREATE")
                nMask |= IN_CREATE;
            if (nMask != 0)
            {
                std::string strFilter = std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().to_bytes(conf.getUnique(strItem, L"filter"));
                uint32_t nLimitAnz{0};
                uint32_t nLimitSek{0};
                std::wstring strLimit = conf.getUnique(strItem, L"limit");
                static const std::wregex rxLimit(L"^(\\d+)\\s*(?:\\/\\s*(\\d+)?\\s*([smh]))?$");
                std::wsmatch mr;
                if (std::regex_search(strLimit, mr ,rxLimit) == true && mr.size() >= 2 && mr[1].matched == true)
                {
                    nLimitAnz = std::stoi(mr[1]);
                    if (mr.size() >= 3 && mr[3].matched == true)
                    {
                        nLimitSek = (mr.size() == 4 && mr[2].matched == true) ? std::stoi(mr[2]) : 1;

                        if (mr[3].str()[0] == 'm')
                            nLimitSek *= 60;
                        else if (mr[3].str()[0] == 'h')
                            nLimitSek *= 3600;
                        //  else if (mr[3][0] == 's')
                        //  do nothing
                    }
                }

                mxWatchList.lock();
                int iWatchId = cFsysNotify.AddWatch(std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().to_bytes(strItem), nMask);
                if (iWatchId > 0)
                {
                    mapWatches.emplace(iWatchId, WATCHITEM{strItem, nMask, nLimitAnz, nLimitSek, 0, std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()), std::regex(strFilter)});
                }
                mxWatchList.unlock();
            }
        }

    };
    svParam.fnStopCallBack = []() noexcept
    {
        // Stop you server here
        syslog(LOG_NOTICE, "StopCallBack called ");

        FileSysNotify& cFsysNotify = FileSysNotify::GetInstance();
        cFsysNotify.StopAllWatch();
    };
    svParam.fnSignalCallBack = []() noexcept
    {
        // what ever you do with this callback, maybe reload the configuration
#if defined(_WIN32) || defined(_WIN64)
        OutputDebugString(L"Signal Callback\r\n");
#else
        syslog(LOG_NOTICE, "SignalCallBack called ");
#endif
    };

    return ServiceMain(argc, argv, svParam);
}
