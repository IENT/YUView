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
#include <QLoggingCategory>
#include <QProcessEnvironment>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>

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

  // Load persisted settings (fileWriteEnabled, minLevel, per-category
  // enables) so that the user's preferences take effect from the very
  // first message — before LogPanel is ever opened.
  loadSettings();

  // Determine log directory
  const QString appData =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  const QString logDir = appData + QDir::separator() + "logs";

  QDir().mkpath(logDir);
  rotateOldLogs(logDir);

  // Open the log file only if file writing is enabled.
  if (fileWriteEnabled.load(std::memory_order_seq_cst))
    openLogFile();

  // Configure QLoggingCategory filter rules so that category-based debug
  // output (qCDebug / LOG_DEBUG) actually reaches the message handler.
  // If the user set QT_LOGGING_RULES, honour it; otherwise enable yuv.* debug.
  {
    const auto env = QProcessEnvironment::systemEnvironment();
    if (env.value(QStringLiteral("QT_LOGGING_RULES")).isEmpty())
      QLoggingCategory::setFilterRules(QStringLiteral("yuv.*.debug=true\nyuv.*.info=true"));
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
// openLogFile – create/open the timestamped log file and write a header
// ---------------------------------------------------------------------------

void Logger::openLogFile()
{
  // Caller must hold the mutex.
  const QString appData =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  const QString logDir = appData + QDir::separator() + "logs";

  const QString ts = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
  logFilePath      = logDir + QDir::separator() + "yuview_" + ts + ".log";
  logFile.setFileName(logFilePath);

  if (!logFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append))
  {
    logFilePath.clear();
    return;
  }

  QTextStream out(&logFile);
  out << "=== YUView Session Started ===\n";
  out << "Time:    " << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << "\n";
  out << "Version: " << qApp->applicationVersion() << "\n";
  out << "==============================\n";
  bytesWritten = logFile.size();
}

// ---------------------------------------------------------------------------
// loadSettings – restore persisted settings from QSettings
// ---------------------------------------------------------------------------

void Logger::loadSettings()
{
  // Caller must hold the mutex.
  QSettings settings;
  settings.beginGroup("LogPanel");

  fileWriteEnabled.store(settings.value("FileWriteEnabled", true).toBool(),
                         std::memory_order_seq_cst);

  currentMinLevel.store(
      static_cast<LogLevel>(settings.value("MinLevel", static_cast<int>(LogLevel::Info)).toInt()),
      std::memory_order_seq_cst);

  settings.beginGroup("CategoryFileEnabled");
  categoryFileEnabled[static_cast<int>(LogCategory::App)].store(
      settings.value("App", true).toBool(), std::memory_order_seq_cst);
  categoryFileEnabled[static_cast<int>(LogCategory::FFmpeg)].store(
      settings.value("FFmpeg", true).toBool(), std::memory_order_seq_cst);
  settings.endGroup();

  settings.endGroup();
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
  categoryFileEnabled[static_cast<int>(cat)].store(enabled, std::memory_order_seq_cst);
}

bool Logger::isCategoryFileEnabled(LogCategory cat) const
{
  return categoryFileEnabled[static_cast<int>(cat)].load(std::memory_order_seq_cst);
}

// ---------------------------------------------------------------------------
// Global file-write master switch
// ---------------------------------------------------------------------------

void Logger::setFileWriteEnabled(bool enabled)
{
  QMutexLocker lock(&mutex);
  if (fileWriteEnabled.load(std::memory_order_seq_cst) == enabled)
    return;
  fileWriteEnabled.store(enabled, std::memory_order_seq_cst);

  if (enabled)
  {
    // User re-enabled file logging — open a fresh log file.
    if (!logFile.isOpen())
      openLogFile();
  }
  else
  {
    // User disabled file logging — close the file immediately so that
    // no further writes are possible (even ones already in flight).
    if (logFile.isOpen())
    {
      QTextStream out(&logFile);
      out << "=== File logging disabled by user ===\n";
      logFile.flush();
      logFile.close();
      logFilePath.clear();
      bytesWritten = 0;
    }
  }
}

bool Logger::isFileWriteEnabled() const
{
  return fileWriteEnabled.load(std::memory_order_seq_cst);
}

// ---------------------------------------------------------------------------
// Minimum severity level filter
// ---------------------------------------------------------------------------

void Logger::setMinLevel(LogLevel level)
{
  QMutexLocker lock(&mutex);
  currentMinLevel.store(level, std::memory_order_seq_cst);
}

LogLevel Logger::minLevel() const
{
  return currentMinLevel.load(std::memory_order_seq_cst);
}

// ---------------------------------------------------------------------------
// cleanOldLogs – public wrapper around rotateOldLogs
// ---------------------------------------------------------------------------

void Logger::cleanOldLogs()
{
  QMutexLocker lock(&mutex);
  rotateOldLogs(logDirectory());
}

// ---------------------------------------------------------------------------
// typeToLevel – map Qt message type to our LogLevel enum
// ---------------------------------------------------------------------------

LogLevel Logger::typeToLevel(QtMsgType type)
{
  switch (type)
  {
    case QtDebugMsg:    return LogLevel::Debug;
    case QtInfoMsg:     return LogLevel::Info;
    case QtWarningMsg:  return LogLevel::Warning;
    case QtCriticalMsg: return LogLevel::Critical;
    case QtFatalMsg:    return LogLevel::Fatal;
  }
  return LogLevel::Debug;
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

  // Drop messages below the configured minimum level (before any formatting
  // or I/O – cheapest possible rejection). currentMinLevel is atomic, so this
  // read is safe without the mutex.
  if (typeToLevel(type) < currentMinLevel.load(std::memory_order_seq_cst))
    return;

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
  const QString tid  = QString("0x%1").arg(reinterpret_cast<quintptr>(QThread::currentThreadId()),
                                           8, 16, QChar('0'));
  const QString line = QString("[%1] [%2] [%3]%4 %5").arg(ts, levelStr, tid, location, msg);

  QMutexLocker lock(&mutex);

  // ---- UI callback (always fires regardless of file setting) ----
  if (uiCallback)
    uiCallback(cat, line);

  // ---- File write (gated by master switch + per-category enable + size cap) ----
  if (logFile.isOpen()
      && fileWriteEnabled.load(std::memory_order_seq_cst)
      && bytesWritten < MAX_LOG_FILE_BYTES
      && categoryFileEnabled[static_cast<int>(cat)].load(std::memory_order_seq_cst))
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
//
// Ensures at most MAX_LOG_FILES - 1 log files remain on disk after this call.
// The caller is responsible for creating the new session file (init path) or
// not (cleanOldLogs path — current session file already counts toward the
// total).  Sorts by modification time (newest first) and deletes the oldest
// entries that exceed the keep limit.
// ---------------------------------------------------------------------------

void Logger::rotateOldLogs(const QString &logDir)
{
  QDir dir(logDir);
  QFileInfoList files =
      dir.entryInfoList({"yuview_*.log"}, QDir::Files, QDir::Time | QDir::Reversed);

  // Keep at most MAX_LOG_FILES - 1 files so that, after the caller opens a new
  // session log, the total never exceeds MAX_LOG_FILES.
  while (files.size() >= MAX_LOG_FILES)
  {
    QFile::remove(files.first().absoluteFilePath());
    files.removeFirst();
  }
}
