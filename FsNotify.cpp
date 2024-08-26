
#include "Service.h"
#include "ConfFile.h"
#include "FilesysNotify.h"

#include <algorithm>
#include <regex>

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#else
#include <codecvt>
#include <locale>
#include <syslog.h>
#include <unistd.h>
#include <iomanip>
#include <fcntl.h>
#endif

#if !defined (_WIN32) && !defined (_WIN64)
void OutputDebugString(const wchar_t* pOut)
{   // mkfifo /tmp/dbgout  ->  tail -f /tmp/dbgout
    const int fdPipe = open("/tmp/dbgout", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fdPipe >= 0)
    {
        const std::wstring strTmp(pOut);
        write(fdPipe, wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().to_bytes(strTmp).c_str(), strTmp.size());
        close(fdPipe);
    }
}
void OutputDebugStringA(const char* pOut)
{   // mkfifo /tmp/dbgout  ->  tail -f /tmp/dbgout
    const int fdPipe = open("/tmp/dbgout", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fdPipe >= 0)
    {
        const std::string strTmp(pOut);
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
    svParam.szSrvName = L"fsnotify";                     // Service name (service id)

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
        std::wstring strAction;
        std::wstring strActionParam;
    }WATCHITEM;
    std::map<int, std::vector<WATCHITEM>> mapWatches;
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

        auto fnGetItemFlag = [](std::wstring& strFlag) -> uint32_t
        {
            std::transform(std::begin(strFlag), std::end(strFlag), std::begin(strFlag), [](wchar_t c) noexcept { return static_cast<wchar_t>(::toupper(c)); });

            if (strFlag == L"DONT_FOLLOW")
                return IN_DONT_FOLLOW;
            if (strFlag == L"EXCL_UNLINK")
                return IN_EXCL_UNLINK;
            if (strFlag == L"ONESHOT")
                return IN_ONESHOT;
            if (strFlag == L"ONLYDIR")
                return IN_ONLYDIR;
            return 0;
        };

        auto fnGetMonitorTyp = [](std::wstring& strMonitor) -> uint32_t
        {
            std::transform(std::begin(strMonitor), std::end(strMonitor), std::begin(strMonitor), [](wchar_t c) noexcept { return static_cast<wchar_t>(::toupper(c)); });

            uint32_t nMonitor{0};
            if (strMonitor == L"ACCESS")
                nMonitor = IN_ACCESS;
            if (strMonitor == L"ATTRIB")
                nMonitor = IN_ATTRIB;
            if (strMonitor == L"CLOSE_WRITE")
                nMonitor = IN_CLOSE_WRITE;
            if (strMonitor == L"CLOSE_NOWRITE")
                nMonitor = IN_CLOSE_NOWRITE;
            if (strMonitor == L"CREATE")
                nMonitor = IN_CREATE;
            if (strMonitor == L"DELETE")
                nMonitor = IN_DELETE;
            if (strMonitor == L"DELETE_SELF")
                nMonitor = IN_DELETE_SELF;
            if (strMonitor == L"MODIFY")
                nMonitor = IN_MODIFY;
            if (strMonitor == L"MOVE_SELF")
                nMonitor = IN_MOVE_SELF;
            if (strMonitor == L"MOVED_FROM")
                nMonitor = IN_MOVED_FROM;
            if (strMonitor == L"MOVED_TO")
                nMonitor = IN_MOVED_TO;
            if (strMonitor == L"OPEN")
                nMonitor = IN_OPEN;
            if (strMonitor == L"CLOSE")
                nMonitor = IN_CLOSE;
            return nMonitor;
        };

        ConfFile& conf = ConfFile::GetInstance(m_strModulePath + L"fsnotify.cfg");
        FileSysNotify& cFsysNotify = FileSysNotify::GetInstance();
        cFsysNotify.SetCallBackFunction([&](int iWatch, uint32_t nMask, uint32_t /*nCookie*/, std::string strName)
        {
            std::stringstream ss;
            ss << "0x" << std::setfill ('0') << std::setw(8) << std::hex << nMask;
            OutputDebugStringA(std::string("Watch-ID: " + to_string(iWatch) +  ", Mask: " + ss.str() + ", Name: " + strName + "\n").c_str());

            // Watch was removed explicitly (inotify_rm_watch(2) called)
            // or automatically (file was deleted, or filesystem was unmounted)
            if (nMask & IN_IGNORED)
            {
                for (auto itMap = mapWatches.begin(); itMap != mapWatches.end(); ++itMap)
                {
                    if (itMap->first == iWatch)
                    {
                        mapWatches.erase(itMap);
                        break;
                    }
                }
                return;
            }

            auto itMonitor = mapWatches.find(iWatch);
            if (itMonitor != mapWatches.end())
            {
                for (auto& vMonitor : itMonitor->second)
                {
                    if (nMask & vMonitor.nWatchTyp)
                    {
                        std::wstring strMonitor;
                        switch(vMonitor.nWatchTyp)
                        {
                        case IN_ACCESS:         strMonitor = L"ACCESS"; break;
                        case IN_ATTRIB:         strMonitor = L"ATTRIB"; break;
                        case IN_CLOSE_WRITE:    strMonitor = L"CLOSE_WRITE"; break;
                        case IN_CLOSE_NOWRITE:  strMonitor = L"CLOSE_NOWRITE"; break;
                        case IN_CREATE:         strMonitor = L"CREATE"; break;
                        case IN_DELETE:         strMonitor = L"DELETE"; break;
                        case IN_DELETE_SELF:    strMonitor = L"DELETE_SELF"; break;
                        case IN_MODIFY:         strMonitor = L"MODIFY"; break;
                        case IN_MOVE_SELF:      strMonitor = L"MOVE_SELF"; break;
                        case IN_MOVED_FROM:     strMonitor = L"MOVED_FROM"; break;
                        case IN_MOVED_TO:       strMonitor = L"MOVED_TO"; break;
                        case IN_OPEN:           strMonitor = L"OPEN"; break;
                        case IN_CLOSE:          strMonitor = L"CLOSE"; break;
                        }

                        if (strMonitor.empty() == false && std::regex_match(strName, vMonitor.reFilter) == true)
                        {
                            std::string strDebug("Event: " + to_string(vMonitor.nWatchTyp) + ", Name: " + strName + ", ");
                            vMonitor.nCount++;
                            const time_t tNow = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                            if (vMonitor.tLastNull == 0)
                                vMonitor.tLastNull = tNow;
                            if (vMonitor.nLimitSek != 0 && tNow - vMonitor.tLastNull > vMonitor.nLimitSek)
                            {
                                strDebug += "Zeit abgelaufen, Dif-Time=" + std::to_string(tNow - vMonitor.tLastNull) + ", SollTime=" + std::to_string(vMonitor.nLimitSek) + ", ";
                                vMonitor.nCount = 1;
                                vMonitor.tLastNull = tNow;
                            }

                            if (vMonitor.nCount >= vMonitor.nLimitAnz)
                            {
                                strDebug += "Max erreicht, Anzahl=" + std::to_string(vMonitor.nCount) + ", nLimitAnz: " + std::to_string(vMonitor.nLimitAnz) + ", Dif-Time=" + std::to_string(tNow - vMonitor.tLastNull);
                                vMonitor.nCount = 0;
                                vMonitor.tLastNull = tNow;

                                std::wstring strAction = vMonitor.strAction;// conf.getUnique(vMonitor.strWatchItem, strMonitor, L"actiontyp");
                                std::transform(std::begin(strAction), std::end(strAction), std::begin(strAction), [](wchar_t c) noexcept { return static_cast<wchar_t>(::toupper(c)); });
                                std::wstring strActionParam = vMonitor.strActionParam;// conf.getUnique(vMonitor.strWatchItem, strMonitor, L"actionpara");

                                if (strAction == L"SYSLOG")
                                    syslog(LOG_NOTICE, "%s", &strName[0]);
                                if (strAction == L"SYSTEM")
                                {
                                    strActionParam = std::regex_replace(strActionParam, std::wregex(L"\\{NOTIFYITEM\\}"), vMonitor.strWatchItem);
                                    std::string strSystem(std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().to_bytes(strActionParam));
                                    bool bSkip{false};
                                    if (strSystem.find("{NAME}") != std::string::npos)
                                    {
                                        if (!strName.empty())
                                            strSystem = std::regex_replace(strSystem, std::regex("\\{NAME\\}"), strName);
                                        else
                                            bSkip = true;
                                    }
                                    if (!bSkip)
                                    {
                                        std::thread([](const std::string strParam)
                                        {
                                            system(strParam.c_str());
                                        }, strSystem).detach();
                                    }
                                }
                                if (strAction == L"ADDMONITOR")
                                {
                                        static const std::wregex re(L"\\\\n");
                                        const std::vector<std::wstring> tokens(std::wsregex_token_iterator(strActionParam.begin(), strActionParam.end(), re, -1), std::wsregex_token_iterator());
                                        if (tokens.size() >= 3)
                                        {
                                            std::vector<WATCHITEM> vWatches;
                                            uint32_t nMask{0};
                                            uint32_t nMonitor{0};
                                            std::string strFilter;
                                            const std::wstring strItem = vMonitor.strWatchItem + L"/" + std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().from_bytes(strName);
                                            for (const auto& itWatch : tokens)
                                            {
                                                const size_t nPos = itWatch.find('=');
                                                const std::wstring strKeyWord = itWatch.substr(0, nPos);
                                                std::wstring strParam = itWatch.substr(nPos + 1);

                                                if (strKeyWord == L"itemflag")
                                                    nMask |= fnGetItemFlag(strParam);
                                                else if (strKeyWord == L"monitortyp")
                                                    nMonitor = fnGetMonitorTyp(strParam);
                                                else if (strKeyWord == L"filter")
                                                    strFilter = std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().to_bytes(strParam);
                                                else if (strKeyWord == L"actiontyp")
                                                    strAction = strParam;
                                                else if (strKeyWord == L"actionpara")
                                                    strActionParam = strParam;
                                            }
                                            if (nMonitor != 0)
                                            {
                                                nMask |= nMonitor;
                                                vWatches.emplace_back(WATCHITEM{strItem, nMonitor, 0, 0, 0, std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()), std::regex(strFilter), strAction, strActionParam });

                                                mxWatchList.lock();
                                                const int iWatchId = cFsysNotify.AddWatch(std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().to_bytes(strItem), nMask);
                                                if (iWatchId > 0)
                                                {
                                                    mapWatches.emplace(iWatchId, vWatches);
                                                }
                                                mxWatchList.unlock();
                                            }
                                        }
                                }
                                /*if (strAction == L"REMMONITOR")
                                {
                                    const std::wstring strWName = vMonitor.strWatchItem + L"/" + std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().from_bytes(strName);
                                    for (auto itMap = mapWatches.begin(); itMap != mapWatches.end(); ++itMap)
                                    {
                                        for (auto itWatch = itMap->second.begin(); itWatch != itMap->second.end(); ++itWatch)
                                        {
                                            if (itWatch->strWatchItem == strWName)
                                            {
                                                std::thread([&](const int iWatchId)
                                                {
                                                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

                                                    mxWatchList.lock();
                                                    const int iRet = cFsysNotify.DelWatch(iWatchId);
                                                    if (iRet == 0)
                                                    {
                                                        itMap->second.erase(itWatch);
                                                        if (itMap->second.empty())
                                                        {
                                                            mapWatches.erase(itMap);
                                                        }
                                                    }
                                                    else
                                                        OutputDebugString(std::wstring(L"Error: " + std::to_wstring(iRet) + L" bei freigeben.\n").c_str());
                                                    mxWatchList.unlock();
                                                }, itMap->first).detach();
                                                break;
                                            }
                                        }
                                    }
                                }*/
                            }
                            if (strDebug.empty() == false)
                            {
                                strDebug += "\n";
                                OutputDebugStringA(strDebug.c_str());
                            }
                        }
                    }
                }
            }
        });

        const std::vector<std::wstring> vSections = conf.get();
        for (const auto& strItem : vSections)
        {
            uint32_t nMask{0};  // IN_CREATE | IN_DELETE | IN_OPEN | IN_CLOSE | ...
            std::vector<WATCHITEM> vWatches;

            vector<wstring> vFlags = conf.getFlags(strItem);
            for (auto& strFlag : vFlags)
            {
                nMask |= fnGetItemFlag(strFlag);
            }

            vector<wstring> vActions = conf.get(strItem);
            for (auto& strMonitor : vActions)
            {
                const uint32_t nMonitor = fnGetMonitorTyp(strMonitor);

                if (nMonitor != 0)
                {
                    nMask |= nMonitor;
                    const std::string strFilter = std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().to_bytes(conf.getUnique(strItem, strMonitor, L"filter"));
                    uint32_t nLimitAnz{0};
                    uint32_t nLimitSek{0};
                    const std::wstring strLimit = conf.getUnique(strItem, strMonitor, L"limit");
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
                    std::wstring strAction = conf.getUnique(strItem, strMonitor, L"actiontyp");
                    std::transform(std::begin(strAction), std::end(strAction), std::begin(strAction), [](wchar_t c) noexcept { return static_cast<wchar_t>(::toupper(c)); });
                    std::wstring strActionParam = conf.getUnique(strItem, strMonitor, L"actionpara");
                    strActionParam.erase(strActionParam.find_last_not_of(L"\" \t\r\n") + 1);  // Trim Whitespace and " character on the right
                    strActionParam.erase(0, strActionParam.find_first_not_of(L"\" \t"));      // Trim Whitespace and " character on the left

                    vWatches.emplace_back(WATCHITEM{strItem, nMonitor, nLimitAnz, nLimitSek, 0, std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()), std::regex(strFilter), strAction, strActionParam});
                }
            }

            mxWatchList.lock();
            const int iWatchId = cFsysNotify.AddWatch(std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().to_bytes(strItem), nMask);
            if (iWatchId > 0)
            {
                mapWatches.emplace(iWatchId, vWatches);
            }
            mxWatchList.unlock();
        }

    };
    svParam.fnStopCallBack = [&mapWatches]() noexcept
    {
        // Stop you server here
        syslog(LOG_NOTICE, "StopCallBack called ");

        FileSysNotify& cFsysNotify = FileSysNotify::GetInstance();
        cFsysNotify.StopAllWatch();

        while (mapWatches.size())
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
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
