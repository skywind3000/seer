// SPDX-FileCopyrightText: 2026 Seer Windows port (fork) contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SEERSTACKTRACE_H
#define SEERSTACKTRACE_H

// Fatal-point stack-trace capture (risk R8 investigation aid). qFatal /
// Q_ASSERT failures and std::terminate leave a log with a backtrace in
// the temp directory before the normal abort path continues (so WER
// LocalDumps on Windows still collects its dump on top).
namespace SeerStackTrace {

    // Install the Qt message handler and the std::terminate hook.
    // Call once at the very start of main().
    void                            installFatalHandler            ();
}

#endif
