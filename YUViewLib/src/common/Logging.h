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

#include <deque>
#include <sstream>

#include <common/EnumMapper.h>

#define LOG(level, message)                                                                        \
  do                                                                                               \
  {                                                                                                \
    std::stringstream ss;                                                                          \
    ss << message;                                                                                 \
    logging::Logger::instance().log(level, typeid(*this), __func__, __LINE__, ss.str());           \
  } while (false)

namespace logging
{

enum class LogLevel
{
  Trace,
  Debug,
  Info,
  Warning,
  Error
};

const auto LogLevelMapper = EnumMapper<LogLevel>({{LogLevel::Trace, "Trace"},
                                                  {LogLevel::Debug, "Debug"},
                                                  {LogLevel::Info, "Info"},
                                                  {LogLevel::Warning, "Warning"},
                                                  {LogLevel::Error, "Error"}});

struct LogEntry
{
  LogLevel    level;
  std::string time;
  std::string component;
  std::string message;
};

std::string formatStringVector(const std::vector<std::string> &vec);

class Logger
{
public:
  static Logger &instance();

  void log(LogLevel              level,
           const std::type_info &info,
           std::string           func,
           unsigned              line,
           std::string           message);

  // Logs with lower level will not me saved
  void setMinLogLevel(LogLevel logLevel);

  size_t   getNrEntries() const;
  LogEntry getEntry(size_t index) const;

private:
  Logger() = default;

  std::deque<LogEntry> logEntries;
  LogLevel             minLogLevel{LogLevel::Info};
};

} // namespace logging