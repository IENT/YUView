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

// This file is compiled ONLY on Windows.
#if defined(Q_OS_WIN) || defined(_WIN32)

#  include "CrashHandler.h"

// clang-format off
#  include <windows.h>
#  include <dbghelp.h>
#  include <psapi.h>
// clang-format on

#  include <QDateTime>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace
{

// A small fixed-size text buffer for the crash-log path that can be used
// without any heap allocation inside the exception filter.
static char g_crashLogPath[MAX_PATH * 2] = {};
static char g_dumpPath[MAX_PATH * 2]     = {};

// Write a DWORD as a decimal string into dest (no CRT needed).
static void dwordToStr(DWORD v, char *dest, int destLen)
{
  if (destLen <= 0)
    return;
  char   tmp[20];
  int    idx = 0;
  if (v == 0)
  {
    tmp[idx++] = '0';
  }
  else
  {
    while (v > 0 && idx < 19)
    {
      tmp[idx++] = '0' + static_cast<char>(v % 10);
      v /= 10;
    }
  }
  // reverse
  int out = 0;
  for (int i = idx - 1; i >= 0 && out < destLen - 1; --i)
    dest[out++] = tmp[i];
  dest[out] = '\0';
}

static void uint64ToHex(DWORD64 v, char *dest, int destLen)
{
  static const char hex[] = "0123456789ABCDEF";
  if (destLen < 19)
    return;
  dest[0] = '0';
  dest[1] = 'x';
  for (int i = 0; i < 16; ++i)
    dest[2 + i] = hex[(v >> (60 - i * 4)) & 0xF];
  dest[18] = '\0';
}

// WriteFile wrapper (no CRT).
static void fileWrite(HANDLE hFile, const char *s)
{
  if (hFile == INVALID_HANDLE_VALUE || !s)
    return;
  DWORD written = 0;
  WriteFile(hFile, s, static_cast<DWORD>(strlen(s)), &written, nullptr);
}

// ---------------------------------------------------------------------------
// WriteMiniDump
// ---------------------------------------------------------------------------
static void writeMiniDump(EXCEPTION_POINTERS *ep, const char *dumpPath)
{
  // Load MiniDumpWriteDump dynamically so the app can still run if dbghelp
  // is not installed (it always is on modern Windows, but be safe).
  HMODULE hDbgHelp = LoadLibraryA("dbghelp.dll");
  if (!hDbgHelp)
    return;

  using MiniDumpWriteDump_t = BOOL(WINAPI *)(HANDLE,
                                             DWORD,
                                             HANDLE,
                                             MINIDUMP_TYPE,
                                             PMINIDUMP_EXCEPTION_INFORMATION,
                                             PMINIDUMP_USER_STREAM_INFORMATION,
                                             PMINIDUMP_CALLBACK_INFORMATION);
  auto pMiniDumpWriteDump =
      reinterpret_cast<MiniDumpWriteDump_t>(GetProcAddress(hDbgHelp, "MiniDumpWriteDump"));

  if (pMiniDumpWriteDump)
  {
    HANDLE hDump = CreateFileA(dumpPath,
                               GENERIC_WRITE,
                               0,
                               nullptr,
                               CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL,
                               nullptr);
    if (hDump != INVALID_HANDLE_VALUE)
    {
      MINIDUMP_EXCEPTION_INFORMATION mei{};
      mei.ThreadId          = GetCurrentThreadId();
      mei.ExceptionPointers = ep;
      mei.ClientPointers    = FALSE;

      pMiniDumpWriteDump(GetCurrentProcess(),
                         GetCurrentProcessId(),
                         hDump,
                         MiniDumpWithDataSegs,
                         &mei,
                         nullptr,
                         nullptr);
      CloseHandle(hDump);
    }
  }
  FreeLibrary(hDbgHelp);
}

// ---------------------------------------------------------------------------
// WriteStackTrace  (uses StackWalk64 from dbghelp)
// ---------------------------------------------------------------------------
static void writeStackTrace(HANDLE hFile, EXCEPTION_POINTERS *ep)
{
  HMODULE hDbgHelp = LoadLibraryA("dbghelp.dll");
  if (!hDbgHelp)
  {
    fileWrite(hFile, "(dbghelp.dll unavailable – no stack trace)\n");
    return;
  }

  using SymInitialize_t     = BOOL(WINAPI *)(HANDLE, PCSTR, BOOL);
  using SymCleanup_t        = BOOL(WINAPI *)(HANDLE);
  using StackWalk64_t       = BOOL(WINAPI *)(DWORD,
                                             HANDLE,
                                             HANDLE,
                                             LPSTACKFRAME64,
                                             PVOID,
                                             PREAD_PROCESS_MEMORY_ROUTINE64,
                                             PFUNCTION_TABLE_ACCESS_ROUTINE64,
                                             PGET_MODULE_BASE_ROUTINE64,
                                             PTRANSLATE_ADDRESS_ROUTINE64);
  using SymFunctionTableAccess64_t = PVOID(WINAPI *)(HANDLE, DWORD64);
  using SymGetModuleBase64_t       = DWORD64(WINAPI *)(HANDLE, DWORD64);
  using SymFromAddr_t              = BOOL(WINAPI *)(HANDLE, DWORD64, PDWORD64, PSYMBOL_INFO);
  using SymGetLineFromAddr64_t     = BOOL(WINAPI *)(HANDLE, DWORD64, PDWORD, PIMAGEHLP_LINE64);

  auto pSymInitialize              = reinterpret_cast<SymInitialize_t>(GetProcAddress(hDbgHelp, "SymInitialize"));
  auto pSymCleanup                 = reinterpret_cast<SymCleanup_t>(GetProcAddress(hDbgHelp, "SymCleanup"));
  auto pStackWalk64                = reinterpret_cast<StackWalk64_t>(GetProcAddress(hDbgHelp, "StackWalk64"));
  auto pSymFunctionTableAccess64   = reinterpret_cast<SymFunctionTableAccess64_t>(GetProcAddress(hDbgHelp, "SymFunctionTableAccess64"));
  auto pSymGetModuleBase64         = reinterpret_cast<SymGetModuleBase64_t>(GetProcAddress(hDbgHelp, "SymGetModuleBase64"));
  auto pSymFromAddr                = reinterpret_cast<SymFromAddr_t>(GetProcAddress(hDbgHelp, "SymFromAddr"));
  auto pSymGetLineFromAddr64       = reinterpret_cast<SymGetLineFromAddr64_t>(GetProcAddress(hDbgHelp, "SymGetLineFromAddr64"));

  if (!pSymInitialize || !pStackWalk64)
  {
    fileWrite(hFile, "(StackWalk64 unavailable)\n");
    FreeLibrary(hDbgHelp);
    return;
  }

  HANDLE hProcess = GetCurrentProcess();
  HANDLE hThread  = GetCurrentThread();

  pSymInitialize(hProcess, nullptr, TRUE);

  CONTEXT ctx{};
  ctx = *ep->ContextRecord;

  STACKFRAME64 frame{};
  DWORD        machineType = IMAGE_FILE_MACHINE_AMD64;

#  if defined(_M_IX86)
  machineType          = IMAGE_FILE_MACHINE_I386;
  frame.AddrPC.Offset  = ctx.Eip;
  frame.AddrPC.Mode    = AddrModeFlat;
  frame.AddrFrame.Offset = ctx.Ebp;
  frame.AddrFrame.Mode = AddrModeFlat;
  frame.AddrStack.Offset = ctx.Esp;
  frame.AddrStack.Mode = AddrModeFlat;
#  elif defined(_M_X64)
  machineType            = IMAGE_FILE_MACHINE_AMD64;
  frame.AddrPC.Offset    = ctx.Rip;
  frame.AddrPC.Mode      = AddrModeFlat;
  frame.AddrFrame.Offset = ctx.Rsp;
  frame.AddrFrame.Mode   = AddrModeFlat;
  frame.AddrStack.Offset = ctx.Rsp;
  frame.AddrStack.Mode   = AddrModeFlat;
#  elif defined(_M_ARM64)
  machineType            = IMAGE_FILE_MACHINE_ARM64;
  frame.AddrPC.Offset    = ctx.Pc;
  frame.AddrPC.Mode      = AddrModeFlat;
  frame.AddrFrame.Offset = ctx.Fp;
  frame.AddrFrame.Mode   = AddrModeFlat;
  frame.AddrStack.Offset = ctx.Sp;
  frame.AddrStack.Mode   = AddrModeFlat;
#  endif

  // Symbol info buffer
  constexpr int   SYM_NAME_LEN = 256;
  alignas(SYMBOL_INFO) char symBuf[sizeof(SYMBOL_INFO) + SYM_NAME_LEN * sizeof(TCHAR)]{};
  auto *sym       = reinterpret_cast<SYMBOL_INFO *>(symBuf);
  sym->SizeOfStruct = sizeof(SYMBOL_INFO);
  sym->MaxNameLen   = SYM_NAME_LEN;

  int frameIdx = 0;
  while (pStackWalk64(machineType,
                      hProcess,
                      hThread,
                      &frame,
                      &ctx,
                      nullptr,
                      pSymFunctionTableAccess64,
                      pSymGetModuleBase64,
                      nullptr)
         && frameIdx < 64)
  {
    char addrBuf[20];
    uint64ToHex(frame.AddrPC.Offset, addrBuf, sizeof(addrBuf));

    char idxBuf[8];
    dwordToStr(static_cast<DWORD>(frameIdx), idxBuf, sizeof(idxBuf));

    fileWrite(hFile, "#");
    fileWrite(hFile, idxBuf);
    fileWrite(hFile, "  ");
    fileWrite(hFile, addrBuf);
    fileWrite(hFile, "  ");

    DWORD64 displacement = 0;
    if (pSymFromAddr && pSymFromAddr(hProcess, frame.AddrPC.Offset, &displacement, sym))
    {
      fileWrite(hFile, sym->Name);

      if (pSymGetLineFromAddr64)
      {
        DWORD         lineDisp = 0;
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
        if (pSymGetLineFromAddr64(hProcess, frame.AddrPC.Offset, &lineDisp, &line))
        {
          fileWrite(hFile, "  (");
          fileWrite(hFile, line.FileName);
          fileWrite(hFile, ":");
          char lineBuf[12];
          dwordToStr(line.LineNumber, lineBuf, sizeof(lineBuf));
          fileWrite(hFile, lineBuf);
          fileWrite(hFile, ")");
        }
      }
    }
    else
    {
      // No symbol – print module name if possible
      char  modName[MAX_PATH] = "(unknown)";
      DWORD64 modBase = pSymGetModuleBase64 ? pSymGetModuleBase64(hProcess, frame.AddrPC.Offset) : 0;
      if (modBase)
      {
        HMODULE hMod = reinterpret_cast<HMODULE>(modBase);
        GetModuleFileNameA(hMod, modName, MAX_PATH);
        // Keep only filename
        char *slash = strrchr(modName, '\\');
        if (slash)
          fileWrite(hFile, slash + 1);
        else
          fileWrite(hFile, modName);
      }
      else
      {
        fileWrite(hFile, "(no symbol)");
      }
    }

    fileWrite(hFile, "\n");
    ++frameIdx;
  }

  if (pSymCleanup)
    pSymCleanup(hProcess);

  FreeLibrary(hDbgHelp);
}

// ---------------------------------------------------------------------------
// Unhandled exception filter
// ---------------------------------------------------------------------------
static LONG WINAPI yuviewExceptionFilter(EXCEPTION_POINTERS *ep)
{
  // Build timestamp string without CRT date/time functions.
  // We use GetLocalTime which is documented safe here.
  SYSTEMTIME st{};
  GetLocalTime(&st);

  char ts[32]{};
  // Format: YYYYMMDD_HHmmss
  {
    auto appendPadded = [](char *buf, int &pos, WORD v, int digits)
    {
      char tmp[8];
      for (int d = digits - 1; d >= 0; --d)
      {
        tmp[d] = '0' + static_cast<char>(v % 10);
        v /= 10;
      }
      for (int i = 0; i < digits; ++i)
        buf[pos++] = tmp[i];
    };
    int pos = 0;
    appendPadded(ts, pos, st.wYear,   4);
    appendPadded(ts, pos, st.wMonth,  2);
    appendPadded(ts, pos, st.wDay,    2);
    ts[pos++] = '_';
    appendPadded(ts, pos, st.wHour,   2);
    appendPadded(ts, pos, st.wMinute, 2);
    appendPadded(ts, pos, st.wSecond, 2);
    ts[pos] = '\0';
  }

  // Build paths
  {
    const QString logDir = CrashHandler::crashLogDirectory();
    const QByteArray logDirBytes = logDir.toLocal8Bit();

    snprintf(g_crashLogPath, sizeof(g_crashLogPath),
             "%s\\yuview_crash_%s.log", logDirBytes.constData(), ts);
    snprintf(g_dumpPath, sizeof(g_dumpPath),
             "%s\\yuview_crash_%s.dmp", logDirBytes.constData(), ts);
  }

  // ---- Write crash log ----
  HANDLE hLog = CreateFileA(g_crashLogPath,
                             GENERIC_WRITE, 0, nullptr,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (hLog != INVALID_HANDLE_VALUE)
  {
    fileWrite(hLog, "=== YUView Crash Report ===\n");
    fileWrite(hLog, "Time:    ");
    fileWrite(hLog, ts);
    fileWrite(hLog, "\n");
    fileWrite(hLog, "Version: ");
    fileWrite(hLog, CrashHandler::appVersion().toUtf8().constData());
    fileWrite(hLog, "\n");

    // Exception code
    char codeBuf[20];
    uint64ToHex(static_cast<DWORD64>(ep->ExceptionRecord->ExceptionCode), codeBuf, sizeof(codeBuf));
    fileWrite(hLog, "Exception code: ");
    fileWrite(hLog, codeBuf);

    // Human-readable exception name
    switch (ep->ExceptionRecord->ExceptionCode)
    {
      case EXCEPTION_ACCESS_VIOLATION:
        fileWrite(hLog, "  (Access Violation)");
        break;
      case EXCEPTION_STACK_OVERFLOW:
        fileWrite(hLog, "  (Stack Overflow)");
        break;
      case EXCEPTION_ILLEGAL_INSTRUCTION:
        fileWrite(hLog, "  (Illegal Instruction)");
        break;
      case EXCEPTION_INT_DIVIDE_BY_ZERO:
        fileWrite(hLog, "  (Integer Divide by Zero)");
        break;
      case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        fileWrite(hLog, "  (Float Divide by Zero)");
        break;
      default:
        break;
    }
    fileWrite(hLog, "\n");

    fileWrite(hLog, "===========================\n");
    fileWrite(hLog, "Stack Trace:\n");
    writeStackTrace(hLog, ep);
    fileWrite(hLog, "===========================\n");
    fileWrite(hLog, "Minidump: ");
    fileWrite(hLog, g_dumpPath);
    fileWrite(hLog, "\n");

    CloseHandle(hLog);
  }

  // ---- Write minidump ----
  writeMiniDump(ep, g_dumpPath);

  // Allow Windows Error Reporting to also handle it (shows the crash dialog).
  return EXCEPTION_CONTINUE_SEARCH;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// CrashHandler::installPlatformHandlers  (Windows implementation)
// ---------------------------------------------------------------------------

void CrashHandler::installPlatformHandlers()
{
  SetUnhandledExceptionFilter(yuviewExceptionFilter);
}

#endif // Q_OS_WIN
