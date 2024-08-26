/* Copyright (C) 2016-2020 Thomas Hauck - All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT

   The author would be happy if changes and
   improvements were reported back to him.

   Author:  Thomas Hauck
   Email:   Thomas@fam-hauck.de
*/

#include "ConfFile.h"

#include <fstream>
#include <codecvt>
#include <algorithm>
#include <functional>

#if defined(_WIN32) || defined(_WIN64)
#define FN_CA(x) x.c_str()
#define FN_STR(x) x
#else
#include <locale>
#include <sys/stat.h>
#include <cmath>
#define _stat stat
#define _wstat stat
#define FN_CA(x) wstring_convert<codecvt_utf8<wchar_t>, wchar_t>().to_bytes(x).c_str()
#define FN_STR(x) wstring_convert<codecvt_utf8<wchar_t>, wchar_t>().to_bytes(x).c_str()
#endif

map<wstring, ConfFile> ConfFile::s_lstConfFiles;

ConfFile& ConfFile::GetInstance(const wstring& strConfigFile)
{
    const auto& instance = s_lstConfFiles.find(strConfigFile);
    if (instance == end(s_lstConfFiles))
    {
        s_lstConfFiles.emplace(strConfigFile, ConfFile(strConfigFile));
        return  s_lstConfFiles.find(strConfigFile)->second;
    }

    return instance->second;
}

ConfFile::ConfFile(const ConfFile& src) : m_strFileName(src.m_strFileName), m_tLastCheck(src.m_tLastCheck), m_tFileTime(src.m_tFileTime), m_mSections(src.m_mSections)
{
}

vector<wstring> ConfFile::get()
{
    CheckFileLoaded();

    vector<wstring> vReturn;

    vReturn.reserve(m_mSections.size());
    for (const auto &item : m_mSections)
    {
        vReturn.push_back(item.first);
    }

    return vReturn;
}

vector<wstring> ConfFile::get(const wstring& strSektion)
{
    CheckFileLoaded();

    vector<wstring> vReturn;

    const auto& section = m_mSections.find(strSektion);
    if (section != end(m_mSections))
    {
        for (const auto& item : section->second)
        {
            if (find(begin(vReturn), end(vReturn), item.first) == end(vReturn))
                vReturn.push_back(item.first);
        }
    }
    return vReturn;
}

vector<wstring> ConfFile::get(const wstring& strSektion, const wstring& strValue)
{
    CheckFileLoaded();

    vector<wstring> vReturn;

    const auto& section = m_mSections.find(strSektion);
    if (section != end(m_mSections))
    {
        auto item = section->second.equal_range(strValue);
        for (; item.first != item.second; ++item.first)
        {
            vReturn.push_back(item.first->first);
        }
    }

    return vReturn;
}

vector<wstring>& ConfFile::getFlags(const wstring& strSektion)
{
    CheckFileLoaded();

    const auto section = m_mSectionFlags.find(strSektion);
    if (section != m_mSectionFlags.end())
    {
        return section->second;
    }

    static vector<wstring> strEmpty;
    return strEmpty;
}

const wstring& ConfFile::getUnique(const wstring& strSektion, const wstring& strAction, const wstring& strValue)
{
    CheckFileLoaded();

    const auto section = m_mSections.find(strSektion);
    if (section != m_mSections.end())
    {
        const auto action = section->second.find(strAction);
        if (action != section->second.end())
        {
            const auto item = action->second.equal_range(strValue);
            if (item.first != item.second)
            {
                //if (distance(item.first, item.second) > 1)
                //    MyTrace("Warnung: Configfile has hidden entry's in section \'", strSektion, "\', key \'", strValue, "\' exist more than once");
                unordered_map<wstring, wstring>::const_iterator it = item.first, itNext = it;
                while (++itNext != item.second && it != item.second) ++it;
                return it->second;  // Letztes Element    //item.first->second;
            }
        }
    }

    static wstring strEmpty;
    return strEmpty;
}

int ConfFile::CheckFileLoaded()
{
    lock_guard<mutex> lock(m_mtxLoad);

    if (m_mSections.empty() == true || AreFilesModified() == true)
    {
        return LoadFile(m_strFileName);
    }
    return 0;
}

int ConfFile::LoadFile(const wstring& strFilename)
{
    m_mSections.clear();

    function<int(const wstring&)> fnLoadFileRecursive = [&](const wstring& strFilename) -> int
    {
        wifstream fin;
        fin.open(FN_STR(strFilename), ios::in | ios::binary);
        if (fin.is_open() == true)
        {
            fin.imbue(std::locale(fin.getloc(), new codecvt_utf8<wchar_t>));

            unordered_map<wstring, unordered_map<wstring, wstring>>* LastSection = nullptr;
            unordered_map<wstring, wstring>* LastTyp = nullptr;
            vector<wstring>* LastFlag = nullptr;

            auto TrimString = [](wstring strVal) -> wstring
            {
                size_t nPos = strVal.find_last_not_of(L" \t\r\n");
                strVal.erase(nPos + 1);  // Trim Whitespace character on the right
                nPos = strVal.find_first_not_of(L" \t");
                strVal.erase(0, nPos);
                return strVal;
            };

            while (fin.eof() == false)
            {
                wstring strLine;
                getline(fin, strLine);

                size_t nPos = strLine.find_first_of(L"#;\r\n");
                if (nPos != string::npos) strLine.erase(nPos);   // erase commends from line
                strLine = TrimString(strLine);

                if (strLine.empty() == false)
                {
                    if (strLine[0] == L'[' && strLine[strLine.size() - 1] == L']')
                    {
                        LastSection = nullptr;
                        LastTyp = nullptr;
                        const auto strTmp = TrimString(strLine.substr(1, strLine.size() - 2));
                        if (strTmp.empty() == false)
                        {
                            const auto& paRet = m_mSections.insert(make_pair(strTmp, unordered_map<wstring, unordered_map<wstring, wstring>>()));
                            if (paRet.second == true)
                            {
                                LastSection = &paRet.first->second;

                                const auto& paRet = m_mSectionFlags.insert(make_pair(strTmp, vector<wstring>()));
                                if (paRet.second == true)
                                {
                                    LastFlag = &paRet.first->second;
                                }
                                continue;
                            }
                        }
                        LastSection = nullptr;
                        LastFlag = nullptr;
                    }
                    else if (nPos = strLine.find(L'='), nPos != string::npos && LastSection != nullptr)
                    {
                        const auto strTmp = TrimString(strLine.substr(0, nPos));
                        if (strTmp.empty() == false)
                        {
                            if (strTmp == L"monitortyp")
                            {
                                LastTyp = nullptr;
                                LastFlag = nullptr;
                                const auto& paRet = LastSection->insert(make_pair(TrimString(strLine.substr(nPos + 1)), unordered_map<wstring, wstring>()));
                                if (paRet.second == true)
                                {
                                    LastTyp = &paRet.first->second;
                                }
                            }
                            else if (LastTyp != nullptr)
                            {
                                LastTyp->insert(make_pair(strTmp, TrimString(strLine.substr(nPos + 1))));
                            }
                            else if (LastFlag != nullptr && strTmp == L"itemflag")
                            {
                                LastFlag->emplace_back(TrimString(strLine.substr(nPos + 1)));
                            }
                        }
                    }
                    else if (strLine[0] == L'@')
                    {
                        fnLoadFileRecursive(TrimString(strLine.substr(1)));
                        LastSection = nullptr;
                    }
                }
            }

            fin.close();

            // We get the file time of the config file we just read, and safe it
            struct _stat stFileInfo;
            if (::_wstat(FN_CA(strFilename), &stFileInfo) == 0)
            {
                m_tFileTime = stFileInfo.st_mtime;
                m_tLastCheck = chrono::steady_clock::now();
            }
            return 0;
        }
        else
            return -1;  //MyTrace("Error: Configfile \'", strFilename, "\' could not be opened");
    };

    return fnLoadFileRecursive(strFilename);
}

bool ConfFile::AreFilesModified() const noexcept
{
    if (m_tFileTime == 0)    // We have no files, we return true as if the file is modified
        return true;

    if (chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - m_tLastCheck).count() < 1000)
        return false;

    struct _stat stFileInfo;
    if (::_wstat(FN_CA(m_strFileName), &stFileInfo) != 0 || ::fabs(::difftime(stFileInfo.st_mtime, m_tFileTime)) > 0.00001)
    {   // error on getting the file time or the file was modified
        return true;
    }

    m_tLastCheck = chrono::steady_clock::now();

    return false;
}
