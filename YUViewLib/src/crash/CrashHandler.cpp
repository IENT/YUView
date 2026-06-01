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

#include "CrashHandler.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>

#include <exception>
#include <typeinfo>

#if defined(Q_OS_WIN) || defined(_WIN32)
// clang-format off
#  include <windows.h>
#  include <io.h>      // _get_osfhandle
// clang-format on
#else
#  include <fcntl.h>
#  include <unistd.h>
#endif

// ---------------------------------------------------------------------------
// Static storage
// ---------------------------------------------------------------------------

QString CrashHandler::s_logDir;
QString CrashHandler::s_appVersion;

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------

CrashHandler &CrashHandler::instance()
{
  static CrashHandler inst;
  return inst;
}

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------

void CrashHandler::init(const QString &logDir)
{
  s_logDir     = logDir;
  s_appVersion = qApp ? qApp->applicationVersion() : QString("unknown");

  installPlatformHandlers();

  // std::set_terminate – catches uncaught C++ exceptions on all platforms.
  std::set_terminate([]()
  {
    // Try to get the exception type name.
    const char *reason = "Uncaught C++ exception (unknown type)";
    std::string typeMsg;
    try
    {
      std::rethrow_exception(std::current_exception());
    }
    catch (const std::exception &e)
    {
      typeMsg = std::string("Uncaught C++ exception: ") + typeid(e).name() + ": " + e.what();
      reason  = typeMsg.c_str();
    }
    catch (...)
    {
      // Unidentified exception – keep default reason.
    }

#if defined(Q_OS_WIN) || defined(_WIN32)
    // On Windows we open the file with CreateFileA to stay async-safe.
    const QString    path      = s_logDir + "/yuview_crash_terminate_"
                                 + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".log";
    const QByteArray pathBytes = path.toLocal8Bit();
    HANDLE           hFile     = CreateFileA(pathBytes.constData(),
                                             GENERIC_WRITE,
                                             0,
                                             nullptr,
                                             CREATE_ALWAYS,
                                             FILE_ATTRIBUTE_NORMAL,
                                             nullptr);
    if (hFile != INVALID_HANDLE_VALUE)
    {
      DWORD written = 0;
      // Use wf (not write) to avoid collision with the CRT ::write symbol.
      auto wf = [&](const char *msg)
      {
        WriteFile(hFile, msg, static_cast<DWORD>(strlen(msg)), &written, nullptr);
      };
      wf("=== YUView Crash Report (terminate) ===\n");
      wf("Reason: ");
      wf(reason);
      wf("\n");
      wf("======================================\n");
      CloseHandle(hFile);
    }
#else
    const QString path = s_logDir + "/yuview_crash_terminate_"
                         + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".log";
    const QByteArray pathBytes = path.toUtf8();
    int fd = ::open(pathBytes.constData(),
                    O_WRONLY | O_CREAT | O_TRUNC,
                    0644);
    if (fd >= 0)
    {
      writeCrashLogHeader(fd, reason);
      writeCrashLogTail(fd);
      ::close(fd);
    }
#endif

    // Re-raise default behaviour (produces core / Windows crash dialog).
    std::abort();
  });
}

// ---------------------------------------------------------------------------
// Pending crash report helpers
// ---------------------------------------------------------------------------

QString CrashHandler::crashLogDirectory()
{
  return s_logDir;
}

QString CrashHandler::appVersion()
{
  return s_appVersion;
}

bool CrashHandler::hasPendingCrashReport()
{
  if (s_logDir.isEmpty())
    return false;
  QDir dir(s_logDir);
  return !dir.entryList({"yuview_crash_*.log"}, QDir::Files).isEmpty();
}

QString CrashHandler::lastCrashReportPath()
{
  if (s_logDir.isEmpty())
    return {};
  QDir dir(s_logDir);
  const QFileInfoList files =
      dir.entryInfoList({"yuview_crash_*.log"}, QDir::Files, QDir::Time);
  if (files.isEmpty())
    return {};
  return files.first().absoluteFilePath();
}

void CrashHandler::archiveCrashReport()
{
  const QString src = lastCrashReportPath();
  if (src.isEmpty())
    return;
  // Rename: yuview_crash_XXX.log → yuview_crash_XXX.log.seen
  QFile::rename(src, src + ".seen");
}

// ---------------------------------------------------------------------------
// writeCrashLogHeader / writeCrashLogTail
// These are called from signal handlers so they MUST use only
// async-signal-safe functions (write, not printf/fwrite).
// ---------------------------------------------------------------------------

static void safeWrite(int fd, const char *s)
{
  if (fd < 0 || !s)
    return;
#if defined(Q_OS_WIN) || defined(_WIN32)
  DWORD  written = 0;
  HANDLE hFile   = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
  WriteFile(hFile, s, static_cast<DWORD>(strlen(s)), &written, nullptr);
#else
  size_t remaining = strlen(s);
  while (remaining > 0)
  {
    ssize_t n = ::write(fd, s, remaining);
    if (n <= 0)
      break;
    s         += n;
    remaining -= static_cast<size_t>(n);
  }
#endif
}

void CrashHandler::writeCrashLogHeader(int fd, const char *reason)
{
  safeWrite(fd, "=== YUView Crash Report ===\n");
  safeWrite(fd, "Reason: ");
  safeWrite(fd, reason ? reason : "(unknown)");
  safeWrite(fd, "\n");
  safeWrite(fd, "Version: ");
  safeWrite(fd, s_appVersion.toUtf8().constData());
  safeWrite(fd, "\n");
  safeWrite(fd, "===========================\n");
  safeWrite(fd, "Stack Trace:\n");
}

void CrashHandler::writeCrashLogTail(int fd)
{
  safeWrite(fd, "===========================\n");
}
