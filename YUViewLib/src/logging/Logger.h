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

#include <QFile>
#include <QMutex>
#include <QString>
#include <QtMessageHandler>
#include <functional>

// ---------------------------------------------------------------------------
// Logger
//
// Installs a Qt message handler that writes all qDebug / qWarning / qCritical
// / qFatal output to a timestamped log file under
//   Windows:  %APPDATA%/YUView/logs/
//   macOS:    ~/Library/Application Support/YUView/logs/
//
// Log rotation: keeps the 5 most recent files; older ones are deleted on init.
// The log file is also limited to MAX_LOG_FILE_BYTES to guard against runaway
// output during a single session.
//
// Log categories
// Messages are classified by prefix into one of the LogCategory values.
// Per-category file-write can be turned on/off at runtime.
// A UI callback can be registered to receive every incoming line for
// live display in a log panel (independent of the file-write setting).
// ---------------------------------------------------------------------------

// Message categories – detected from the message prefix "[FFmpeg…]" etc.
enum class LogCategory
{
  App,     // general qDebug / qWarning / qCritical from application code
  FFmpeg,  // messages prefixed with "[FFmpeg"
  COUNT
};

class Logger
{
public:
  static Logger &instance();

  // Callback invoked on the calling thread for every incoming log line.
  // The panel should use QMetaObject::invokeMethod(..., Qt::QueuedConnection)
  // to forward to the UI thread.
  using UiCallback = std::function<void(LogCategory cat, const QString &line)>;

  // Install the Qt message handler and open the log file.
  void init();

  // Flush and close the log file; uninstall the message handler.
  void shutdown();

  // Per-category file-write enable / disable (default: all enabled).
  void setCategoryFileEnabled(LogCategory cat, bool enabled);
  bool isCategoryFileEnabled(LogCategory cat) const;

  // Register / clear the UI callback (thread-safe).
  void setUiCallback(UiCallback cb);
  void clearUiCallback();

  // Path to the directory that contains log files.
  QString logDirectory() const;

  // Path to the log file opened during this session (empty before init()).
  QString currentLogFilePath() const;

  // Write a raw line directly to the log (used by the crash handler which
  // cannot go through Qt's logging machinery safely).
  void writeRaw(const char *utf8Line);

private:
  Logger()  = default;
  ~Logger() { shutdown(); }

  Logger(const Logger &)            = delete;
  Logger &operator=(const Logger &) = delete;

  static void qtMessageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg);
  static LogCategory detectCategory(const QString &msg);

  void rotateOldLogs(const QString &logDir);
  void writeEntry(QtMsgType type, const QMessageLogContext &ctx, const QString &msg);

  QFile   logFile;
  QMutex  mutex;
  bool    initialised{false};
  QString logFilePath;

  bool categoryFileEnabled[static_cast<int>(LogCategory::COUNT)]{true, true};

  UiCallback uiCallback;  // protected by mutex

  // Per-session byte counter – stop writing when this exceeds the cap.
  static constexpr qint64 MAX_LOG_FILE_BYTES = 10LL * 1024 * 1024; // 10 MB
  qint64                  bytesWritten{0};

  // How many log files to keep.
  static constexpr int MAX_LOG_FILES = 5;
};
