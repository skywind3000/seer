// SPDX-FileCopyrightText: 2026 Seer Windows port (fork) contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Windows implementation of the QProcessInfo::populate() family
// (see docs/porting/specs/processinfo.md). Uses a Toolhelp32 snapshot,
// QueryFullProcessImageName and the process token SID. The command
// line is left empty on Windows.

// Q_OS_WIN is defined by Qt's headers, so a Qt header must come before
// the platform guard below.
#include <QtCore/QtGlobal>

#ifdef Q_OS_WIN

#include "QProcessInfo.h"
#include <QtCore/QFileInfo>
#include <windows.h>
#include <tlhelp32.h>

static QString windowsProcessImagePath (DWORD pid) {

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);

    if (process == NULL) {
        return QString();
    }

    wchar_t path[1000];
    DWORD  size = 1000;

    QString image;

    if (QueryFullProcessImageNameW(process, 0, path, &size)) {
        image = QString::fromWCharArray(path);
    }

    CloseHandle(process);

    return image;
}

static QString windowsProcessUsername (DWORD pid) {

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);

    if (process == NULL) {
        return QString();
    }

    QString username;

    HANDLE token = NULL;

    if (OpenProcessToken(process, TOKEN_QUERY, &token)) {

        DWORD needed = 0;
        GetTokenInformation(token, TokenUser, NULL, 0, &needed);

        if (needed > 0) {
            char* buffer = new char[needed];

            if (GetTokenInformation(token, TokenUser, buffer, needed, &needed)) {
                TOKEN_USER* tokenUser = (TOKEN_USER*)buffer;
                wchar_t    name[300];
                wchar_t    domain[300];
                DWORD      nameLen   = 300;
                DWORD      domainLen = 300;
                SID_NAME_USE use;

                if (LookupAccountSidW(NULL, tokenUser->User.Sid, name, &nameLen, domain, &domainLen, &use)) {
                    username = QString::fromWCharArray(name);
                }
            }

            delete[] buffer;
        }

        CloseHandle(token);
    }

    CloseHandle(process);

    return username;
}

static void windowsPopulateEntry (const PROCESSENTRY32W& entry, QProcessInfo& info) {

    info.setPid(entry.th32ProcessID);

    QString path = windowsProcessImagePath(entry.th32ProcessID);

    if (path.isEmpty() == false) {
        // Keep the '.exe' extension: the name is used to rebuild the
        // executable path ('path + "/" + name'), and a stripped name
        // reconstructs a file gdb only finds via its auto-'exe' retry.
        // Linux names have no extension by nature, so both platforms
        // end up with the executable's real basename.
        info.setName(QFileInfo(path).fileName());
        info.setPath(QFileInfo(path).absolutePath());
    }else{
        // Bracket the snapshot name to indicate it is not a file path,
        // like the Linux version does for /proc/<pid>/status names.
        info.setName(QString("[%1]").arg(QString::fromWCharArray(entry.szExeFile)));
        info.setPath("");
    }

    info.setUsername(windowsProcessUsername(entry.th32ProcessID));

    // The command line is left empty on Windows.
}

QProcessInfo QProcessInfo::populate(uint32_t pid) {

    QProcessInfo info;

    info.setPid(pid);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (snapshot == INVALID_HANDLE_VALUE) {
        return info;
    }

    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);

    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (entry.th32ProcessID == pid) {
                windowsPopulateEntry(entry, info);
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);

    return info;
}

QProcessList QProcessInfo::populate() {

    QProcessList list;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (snapshot == INVALID_HANDLE_VALUE) {
        return list;
    }

    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);

    if (Process32FirstW(snapshot, &entry)) {
        do {
            QProcessInfo info;
            windowsPopulateEntry(entry, info);
            list.push_back(info);
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);

    return list;
}

#endif
