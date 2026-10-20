// SPDX-FileCopyrightText: 2026 Seer Windows port (fork) contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Fatal-point stack-trace capture. On a Qt fatal message (qFatal /
// Q_ASSERT failure) or std::terminate, append the message plus a
// backtrace to "<temp>/seer-fatal-<pid>.log" and to stderr, then let
// the normal termination path run.
//
// Symbolication note: the MinGW build carries DWARF, which the
// dbghelp-based tooling cannot read. The log therefore records
// module+offset frames and, for frames inside the main executable,
// the link-time address that can be fed straight into addr2line:
//
//     addr2line -f -C -e seergdb.exe 0x140001234
//
// Windows dump analysis (cdb) complements this with full stacks from
// the WER LocalDumps files; see docs/porting (AGENTS.local.md section 7).

#include "SeerStackTrace.h"
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QDateTime>
#include <QtCore/QString>
#include <QtGlobal>
#include <cstdio>
#include <cstdlib>
#include <exception>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <execinfo.h>
#include <unistd.h>
#endif

namespace SeerStackTrace {

#ifdef Q_OS_WIN

static QString nativeExePath () {

    wchar_t path[MAX_PATH * 2];

    DWORD n = GetModuleFileNameW(NULL, path, sizeof(path) / sizeof(wchar_t));

    return QString::fromWCharArray(path, n);
}

static unsigned long nativePid () {
    return GetCurrentProcessId();
}

// The link-time image base of the main executable, so frame addresses
// can be converted into the addresses stored in the DWARF tables.
// The in-memory OptionalHeader.ImageBase is updated by the loader to
// the ASLR base, so this reads the value from the executable file.
static quint64 exeImageBase () {

    QFile file(nativeExePath());

    if (file.open(QIODevice::ReadOnly) == false) {
        return 0;
    }

    quint32 lfanew = 0;

    if (file.seek(0x3c) == false || file.read((char*)&lfanew, sizeof(lfanew)) != (qint64)sizeof(lfanew)) {
        return 0;
    }

    // NT signature (4) + IMAGE_FILE_HEADER (20) + OptionalHeader up to
    // ImageBase (24).
    quint64 base = 0;

    if (file.seek(lfanew + 4 + 20 + 24) == false || file.read((char*)&base, sizeof(base)) != (qint64)sizeof(base)) {
        return 0;
    }

    return base;
}

static QString capture () {

    void*  frames[64];
    USHORT count = CaptureStackBackTrace(0, 64, frames, NULL);

    HMODULE exeModule = GetModuleHandleW(NULL);
    quint64 base      = exeImageBase();

    QString text;

    for (USHORT i = 0; i < count; i++) {

        HMODULE module = NULL;

        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)frames[i],
                           &module);

        wchar_t wname[MAX_PATH * 2];
        QString name = "<unknown>";

        if (module != NULL && GetModuleFileNameW(module, wname, sizeof(wname) / sizeof(wchar_t)) != 0) {
            name = QString::fromWCharArray(wname);
            name = name.mid(qMax(name.lastIndexOf('/'), name.lastIndexOf('\\')) + 1);
        }

        quintptr offset = (quintptr)((char*)frames[i] - (char*)module);

        QString line = QString("#%1  0x%2  %3+0x%4")
            .arg((int)i, 2, 10, QChar(' '))
            .arg((quintptr)frames[i], 16, 16, QChar('0'))
            .arg(name)
            .arg(offset, 16, 16, QChar('0'));

        if (module == exeModule && base != 0) {
            line += QString("  (addr2line: 0x%1)").arg(base + offset, 16, 16, QChar('0'));
        }

        text += line + "\n";
    }

    return text;
}

#else

static QString nativeExePath () {

    char path[4096];

    ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);

    if (n <= 0) {
        return QString("seergdb");
    }

    path[n] = 0;

    return QString::fromLocal8Bit(path);
}

static unsigned long nativePid () {
    return getpid();
}

static QString capture () {

    void* frames[64];

    int count = backtrace(frames, 64);

    char** symbols = backtrace_symbols(frames, count);

    QString text;

    for (int i = 0; i < count; i++) {
        if (symbols != NULL) {
            text += QString::fromLocal8Bit(symbols[i]) + "\n";
        }else{
            text += QString("#%1  0x%2\n").arg(i).arg((quintptr)frames[i], 16, 16, QChar('0'));
        }
    }

    if (symbols != NULL) {
        free(symbols);
    }

    return text;
}

#endif

static void writeFatalLog (const QString& kind, const QString& message, const QString& location) {

    QString path = QDir::tempPath() + QString("/seer-fatal-%1.log").arg(nativePid());

    QFile file(path);

    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {

        QTextStream stream(&file);

        stream << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
        stream << "exe: " << nativeExePath() << "\n";
        stream << kind << ": " << message << "\n";

        if (location.isEmpty() == false) {
            stream << "at: " << location << "\n";
        }

        stream << "stack:\n" << capture();
        stream << "symbolize own-exe frames with: addr2line -f -C -e <exe> <addr>\n\n";
        stream.flush();
    }

    std::fprintf(stderr, "Seer fatal (%s): %s\nLog written to %s\n",
                       qPrintable(kind), qPrintable(message), qPrintable(path));
    std::fflush(stderr);
}

static QtMessageHandler previousMessageHandler = nullptr;

static void messageHandler (QtMsgType type, const QMessageLogContext& context, const QString& message) {

    if (type == QtFatalMsg) {
        QString location = QString("%1:%2 (%3)")
                               .arg(context.file != nullptr ? context.file : "?")
                               .arg(context.line)
                               .arg(context.function != nullptr ? context.function : "?");
        writeFatalLog("qFatal", message, location);
    }

    if (previousMessageHandler != nullptr) {
        previousMessageHandler(type, context, message);
    }else{
        std::fprintf(stderr, "%s\n", qPrintable(message));
        std::fflush(stderr);
    }
}

static void terminateHandler () {

    writeFatalLog("std::terminate", "uncaught exception or explicit std::terminate", "");

    std::abort();
}

void installFatalHandler () {

    previousMessageHandler = qInstallMessageHandler(messageHandler);

    std::set_terminate(terminateHandler);
}

}
