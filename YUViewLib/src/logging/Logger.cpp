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

#include "Logger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>
#include <QStandardPaths>
#include <QTextStream>

#if !defined(Q_OS_WIN)
#include <unistd.h>
#endif
#include <cstring>

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------

Logger &Logger::instance()
{
  static Logger inst;
  return inst;
}

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------

void Logger::init()
{
  QMutexLocker lock(&mutex);
  if (initialised)
    return;

  // Determine log directory
  const QString appData =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  const QString logDir = appData + QDir::separator() + "logs";

  QDir().mkpath(logDir);
  rotateOldLogs(logDir);

  // Build timestamped file name
  const QString ts       = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
  logFilePath            = logDir + QDir::separator() + "yuview_" + ts + ".log";
  logFile.setFileName(logFilePath);

  if (!logFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append))
  {
    // Can't open log file – silently continue without file logging.
    logFilePath.clear();
  }
  else
  {
    // Write a session header
    QTextStream out(&logFile);
    out << "=== YUView Session Started ===\n";
    out << "Time:    " << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << "\n";
    out << "Version: " << qApp->applicationVersion() << "\n";
    out << "==============================\n";
    bytesWritten = logFile.size();
  }

  qInstallMessageHandler(&Logger::qtMessageHandler);
  initialised = true;
}

// ---------------------------------------------------------------------------
// shutdown
// ---------------------------------------------------------------------------

void Logger::shutdown()
{
  QMutexLocker lock(&mutex);
  if (!initialised)
    return;

  qInstallMessageHandler(nullptr); // restore default handler

  if (logFile.isOpen())
  {
    QTextStream out(&logFile);
    out << "=== YUView Session Ended ===\n";
    logFile.flush();
    logFile.close();
  }
  initialised = false;
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

QString Logger::logDirectory() const
{
  if (logFilePath.isEmpty())
  {
    const QString appData =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return appData + QDir::separator() + "logs";
  }
  return QFileInfo(logFilePath).absolutePath();
}

QString Logger::currentLogFilePath() const
{
  return logFilePath;
}

// ---------------------------------------------------------------------------
// writeRaw  (used by crash handler – must be async-signal-safe on POSIX)
// ---------------------------------------------------------------------------

void Logger::writeRaw(const char *utf8Line)
{
  // NOTE: On POSIX this is called from a signal handler.
  // We use the low-level POSIX write() instead of Qt I/O when the file
  // descriptor is valid.
  if (!logFile.isOpen())
    return;

#if defined(Q_OS_WIN)
  // On Windows SEH handler we are not in a signal context, so Qt I/O is fine.
  QMutexLocker lock(&mutex);
  logFile.write(utf8Line);
  logFile.write("\n");
  logFile.flush();
#else
  // POSIX: use raw write() – async-signal-safe.
  int fd = logFile.handle();
  if (fd < 0)
    return;
  ::write(fd, utf8Line, ::strlen(utf8Line));
  ::write(fd, "\n", 1);
#endif
}

// ---------------------------------------------------------------------------
// Category detection
// ---------------------------------------------------------------------------

LogCategory Logger::detectCategory(const QString &msg)
{
  if (msg.startsWith(QLatin1String("[FFmpeg")))
    return LogCategory::FFmpeg;
  return LogCategory::App;
}

// ---------------------------------------------------------------------------
// Per-category file enable / disable
// ---------------------------------------------------------------------------

void Logger::setCategoryFileEnabled(LogCategory cat, bool enabled)
{
  QMutexLocker lock(&mutex);
  categoryFileEnabled[static_cast<int>(cat)] = enabled;
}

bool Logger::isCategoryFileEnabled(LogCategory cat) const
{
  return categoryFileEnabled[static_cast<int>(cat)];
}

// ---------------------------------------------------------------------------
// UI callback
// ---------------------------------------------------------------------------

void Logger::setUiCallback(UiCallback cb)
{
  QMutexLocker lock(&mutex);
  uiCallback = std::move(cb);
}

void Logger::clearUiCallback()
{
  QMutexLocker lock(&mutex);
  uiCallback = nullptr;
}

// ---------------------------------------------------------------------------
// Qt message handler (static)
// ---------------------------------------------------------------------------

void Logger::qtMessageHandler(QtMsgType               type,
                               const QMessageLogContext &ctx,
                               const QString           &msg)
{
  Logger::instance().writeEntry(type, ctx, msg);
}

// ---------------------------------------------------------------------------
// writeEntry
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Messages that are too noisy to be useful and should be suppressed.
// Each entry is a substring that, if found in the message, causes it to be
// dropped entirely.  Add new patterns here as needed.
// ---------------------------------------------------------------------------
static const char *const SUPPRESSED_PATTERNS[] = {
  // Qt internal: emitted when a model calls dataChanged() with invalid indices.
  // Happens frequently in playlist / tree view interactions and carries no
  // actionable information for end-users.
  "dataChanged() called with an invalid index range",
};

void Logger::writeEntry(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
  // Apply suppression filter before taking the mutex (cheap path for noisy msgs).
  for (const char *pattern : SUPPRESSED_PATTERNS)
  {
    if (msg.contains(QLatin1String(pattern)))
      return;
  }

  const LogCategory cat = detectCategory(msg);

  const char *levelStr = "DEBUG   ";
  switch (type)
  {
    case QtDebugMsg:    levelStr = "DEBUG   "; break;
    case QtInfoMsg:     levelStr = "INFO    "; break;
    case QtWarningMsg:  levelStr = "WARNING "; break;
    case QtCriticalMsg: levelStr = "CRITICAL"; break;
    case QtFatalMsg:    levelStr = "FATAL   "; break;
  }

  QString location;
  if (ctx.file && ctx.line > 0)
  {
    QString file  = QString::fromUtf8(ctx.file);
    const int sep = qMax(file.lastIndexOf('/'), file.lastIndexOf('\\'));
    if (sep >= 0)
      file = file.mid(sep + 1);
    location = QString(" [%1:%2]").arg(file).arg(ctx.line);
  }

  const QString ts   = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
  const QString line = QString("[%1] [%2]%3 %4").arg(ts, levelStr, location, msg);

  QMutexLocker lock(&mutex);

  // ---- UI callback (always fires regardless of file setting) ----
  if (uiCallback)
    uiCallback(cat, line);

  // ---- File write (gated by per-category enable + size cap) ----
  if (logFile.isOpen()
      && bytesWritten < MAX_LOG_FILE_BYTES
      && categoryFileEnabled[static_cast<int>(cat)])
  {
    const QByteArray bytes = (line + '\n').toUtf8();
    logFile.write(bytes);
    logFile.flush();
    bytesWritten += bytes.size();

    if (bytesWritten >= MAX_LOG_FILE_BYTES)
    {
      const char cap[] = "[Logger] Log file size cap reached. Further output suppressed.\n";
      logFile.write(cap);
      logFile.flush();
    }
  }

  // For Fatal messages also print to stderr so the OS can capture it.
  if (type == QtFatalMsg)
  {
    fprintf(stderr, "%s\n", line.toUtf8().constData());
    fflush(stderr);
  }
}

// ---------------------------------------------------------------------------
// rotateOldLogs
// ---------------------------------------------------------------------------

void Logger::rotateOldLogs(const QString &logDir)
{
  QDir dir(logDir);
  QFileInfoList files =
      dir.entryInfoList({"yuview_*.log"}, QDir::Files, QDir::Time | QDir::Reversed);

  // Delete oldest files beyond the keep limit (MAX_LOG_FILES - 1 existing + 1 new).
  while (files.size() >= MAX_LOG_FILES)
  {
    QFile::remove(files.first().absoluteFilePath());
    files.removeFirst();
  }
}
