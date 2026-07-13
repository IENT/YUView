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

// This file is compiled on macOS and Linux only.
#if !defined(_WIN32) && !defined(Q_OS_WIN)

#  include "CrashHandler.h"

#  include <execinfo.h>
#  include <fcntl.h>
#  include <limits.h>
#  include <signal.h>
#  include <string.h>
#  include <time.h>
#  include <unistd.h>

// ---------------------------------------------------------------------------
// Internal helpers  (all async-signal-safe unless noted)
// ---------------------------------------------------------------------------

namespace
{

// Pre-converted (at init time) UTF-8 copy of the log directory path.
// Using a plain char[] avoids any heap allocation inside the signal handler.
static char g_logDirBuf[PATH_MAX]{};

// Previous signal handlers so we can chain to the OS default.
static struct sigaction g_oldSIGSEGV{};
static struct sigaction g_oldSIGABRT{};
static struct sigaction g_oldSIGBUS{};
static struct sigaction g_oldSIGFPE{};
static struct sigaction g_oldSIGILL{};

// Guard against recursive signals (e.g. SIGSEGV inside the handler).
static volatile sig_atomic_t g_handlerActive = 0;

// async-signal-safe: write a NUL-terminated string to fd.
static void safeWrite(int fd, const char *s)
{
  if (fd < 0 || !s)
    return;
  size_t remaining = strlen(s);
  while (remaining > 0)
  {
    ssize_t n = ::write(fd, s, remaining);
    if (n <= 0)
      break;
    s         += n;
    remaining -= static_cast<size_t>(n);
  }
}

// async-signal-safe: write an integer as decimal.
static void safeWriteInt(int fd, long v)
{
  if (v < 0)
  {
    safeWrite(fd, "-");
    v = -v;
  }
  char   buf[24];
  int    idx = 0;
  if (v == 0)
  {
    buf[idx++] = '0';
  }
  else
  {
    while (v > 0)
    {
      buf[idx++] = '0' + static_cast<char>(v % 10);
      v /= 10;
    }
    // reverse
    for (int i = 0, j = idx - 1; i < j; ++i, --j)
    {
      char tmp = buf[i];
      buf[i]   = buf[j];
      buf[j]   = tmp;
    }
  }
  buf[idx] = '\0';
  safeWrite(fd, buf);
}

// Build a timestamped crash-log path into `out` (async-signal-safe).
// `out` must be at least 512 bytes.
static void buildCrashLogPath(char *out, int outLen, const char *logDir)
{
  // Use clock_gettime – async-signal-safe.
  struct timespec ts{};
  clock_gettime(CLOCK_REALTIME, &ts);

  // Convert seconds to broken-down time using gmtime_r (re-entrant; in
  // practice async-signal-safe on glibc/Bionic, though not guaranteed by
  // strict POSIX signal-safety(7)).
  struct tm tm_info{};
  time_t    sec = ts.tv_sec;
  gmtime_r(&sec, &tm_info);

  char tsStr[32]{};
  // Format: YYYYMMDD_HHmmss
  snprintf(tsStr, sizeof(tsStr),
           "%04d%02d%02d_%02d%02d%02d",
           tm_info.tm_year + 1900,
           tm_info.tm_mon + 1,
           tm_info.tm_mday,
           tm_info.tm_hour,
           tm_info.tm_min,
           tm_info.tm_sec);

  snprintf(out, static_cast<size_t>(outLen),
           "%s/yuview_crash_%s.log", logDir, tsStr);
}

static const char *signalName(int sig)
{
  switch (sig)
  {
    case SIGSEGV: return "SIGSEGV (Segmentation Fault)";
    case SIGABRT: return "SIGABRT (Abort)";
    case SIGBUS:  return "SIGBUS  (Bus Error)";
    case SIGFPE:  return "SIGFPE  (Floating Point Exception)";
    case SIGILL:  return "SIGILL  (Illegal Instruction)";
    default:      return "Unknown Signal";
  }
}

// ---------------------------------------------------------------------------
// The actual signal handler
// ---------------------------------------------------------------------------
static void yuviewSignalHandler(int sig, siginfo_t * /*info*/, void * /*ctx*/)
{
  // Prevent re-entrance.
  if (g_handlerActive)
    return;
  g_handlerActive = 1;

  // g_logDirBuf was pre-populated in installPlatformHandlers() so that no
  // heap allocation (QString copy / toUtf8()) is needed here.
  char path[512]{};
  buildCrashLogPath(path, sizeof(path), g_logDirBuf);

  int fd = ::open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd >= 0)
  {
    safeWrite(fd, "=== YUView Crash Report ===\n");
    safeWrite(fd, "Signal: ");
    safeWrite(fd, signalName(sig));
    safeWrite(fd, "\n");
    safeWrite(fd, "PID: ");
    safeWriteInt(fd, static_cast<long>(getpid()));
    safeWrite(fd, "\n");
    safeWrite(fd, "===========================\n");
    safeWrite(fd, "Stack Trace:\n");

    // backtrace / backtrace_symbols_fd are async-signal-safe on most platforms.
    void *frames[64]{};
    int   count = backtrace(frames, 64);
    backtrace_symbols_fd(frames, count, fd);

    safeWrite(fd, "===========================\n");
    ::close(fd);
  }

  // Re-raise to let the OS produce a core file / crash report.
  struct sigaction *old = nullptr;
  switch (sig)
  {
    case SIGSEGV: old = &g_oldSIGSEGV; break;
    case SIGABRT: old = &g_oldSIGABRT; break;
    case SIGBUS:  old = &g_oldSIGBUS;  break;
    case SIGFPE:  old = &g_oldSIGFPE;  break;
    case SIGILL:  old = &g_oldSIGILL;  break;
    default: break;
  }

  if (old)
  {
    // Restore old handler then re-raise so the default action runs.
    sigaction(sig, old, nullptr);
  }
  else
  {
    // Restore SIG_DFL and re-raise.
    struct sigaction sa{};
    sa.sa_handler = SIG_DFL;
    sigaction(sig, &sa, nullptr);
  }

  ::raise(sig);
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// CrashHandler::installPlatformHandlers  (POSIX implementation)
// ---------------------------------------------------------------------------

void CrashHandler::installPlatformHandlers()
{
  // Pre-convert the log directory path to a plain C string so that the signal
  // handler can use it without calling any heap-allocating Qt functions.
  {
    const QByteArray dirBytes = crashLogDirectory().toUtf8();
    strncpy(g_logDirBuf, dirBytes.constData(), sizeof(g_logDirBuf) - 1);
    g_logDirBuf[sizeof(g_logDirBuf) - 1] = '\0';
  }

  struct sigaction sa{};
  sa.sa_sigaction = yuviewSignalHandler;
  sa.sa_flags     = SA_SIGINFO | SA_RESETHAND;
  sigemptyset(&sa.sa_mask);

  sigaction(SIGSEGV, &sa, &g_oldSIGSEGV);
  sigaction(SIGABRT, &sa, &g_oldSIGABRT);
  sigaction(SIGBUS,  &sa, &g_oldSIGBUS);
  sigaction(SIGFPE,  &sa, &g_oldSIGFPE);
  sigaction(SIGILL,  &sa, &g_oldSIGILL);
}

#endif // !_WIN32
