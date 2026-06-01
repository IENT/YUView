/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   In addition, as a special exception, the copyright holders give
 *   permission to link the code of portions of this program with the
 *   OpenSSL library under certain conditions as described in each
 *   individual source file, and distribute linked combinations including
 *   the two.
 *
 *   You must obey the GNU General Public License in all respects for all
 *   of the code used other than OpenSSL. If you modify file(s) with this
 *   exception, you may extend this exception to your version of the
 *   file(s), but you are not obligated to do so. If you do not wish to do
 *   so, delete this exception statement from your version. If you delete
 *   this exception statement from all source files in the program, then
 *   also delete it here.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QString>

// ---------------------------------------------------------------------------
// CrashHandler
//
// Cross-platform crash / unhandled-exception handler.
//
//  • Windows: SetUnhandledExceptionFilter + MiniDumpWriteDump (dbghelp.dll)
//             + StackWalk64 for a text stack trace in the crash log.
//  • macOS / Linux: sigaction for SIGSEGV / SIGABRT / SIGBUS / SIGFPE / SIGILL
//             + backtrace() + backtrace_symbols_fd() for the stack trace.
//  • All platforms: std::set_terminate() to catch uncaught C++ exceptions.
//
// The handler writes two artefacts to the log directory:
//   yuview_crash_<timestamp>.log   – human-readable summary + stack trace
//   yuview_crash_<timestamp>.dmp   – Windows minidump (Windows only)
//
// On the next launch, CrashHandler::hasPendingCrashReport() returns true and
// lastCrashReportPath() gives the path so the UI can inform the user.
//
// Usage:
//   CrashHandler::instance().init(logDirectory);
// ---------------------------------------------------------------------------

class CrashHandler
{
public:
  static CrashHandler &instance();

  // Install all handlers.  logDir must already exist.
  void init(const QString &logDir);

  // Returns true if a crash log from the previous run is present.
  static bool hasPendingCrashReport();

  // Path to the most recent crash log (newest yuview_crash_*.log).
  static QString lastCrashReportPath();

  // Archive (rename) the pending crash log so it is not shown again.
  static void archiveCrashReport();

  // The directory where crash logs are written (set during init).
  static QString crashLogDirectory();

  // App version string set during init – used by platform crash handlers.
  static QString appVersion();

private:
  CrashHandler()  = default;
  ~CrashHandler() = default;

  CrashHandler(const CrashHandler &)            = delete;
  CrashHandler &operator=(const CrashHandler &) = delete;

  static void installPlatformHandlers();

  // Shared helper: write the crash log header and tail.
  // Called from platform-specific handlers after collecting the trace.
  static void writeCrashLogHeader(int fd, const char *reason);
  static void writeCrashLogTail(int fd);

  // Directory set during init – stored in a static so signal handlers
  // (which cannot use member variables safely) can access it.
  static QString s_logDir;
  static QString s_appVersion;
};
