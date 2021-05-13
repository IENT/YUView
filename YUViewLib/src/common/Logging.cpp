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

#include "Logging.h"

#include <chrono>
#include <iomanip>
#include <vector>


namespace logging
{

namespace
{

std::vector<LogLevel> Priorities = {LogLevel::Debug, LogLevel::Info, LogLevel::Error};

std::string formatCurrentTime()
{
  auto now  = std::chrono::system_clock::now();
  auto ms   = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
  auto time = std::chrono::system_clock::to_time_t(now);
  auto bt   = std::localtime(&time);

  std::ostringstream ss;
  ss << std::put_time(bt, "%H:%M:%S"); // HH:MM:SS
  ss << '.' << std::setfill('0') << std::setw(3) << ms.count();

  return ss.str();
}

} // namespace

std::string formatStringVector(const std::vector<std::string> &vec)
{
  std::stringstream ss;
  ss << "[";
  for (auto it = vec.begin(); it != vec.end(); it++)
  {
    if (it != vec.begin())
      ss << ", ";
    ss << (*it);
  }
  ss << "]";
  return ss.str();
}

Logger &Logger::instance()
{
  static Logger logger;
  return logger;
}

void Logger::log(LogLevel              logLevel,
                 const std::type_info &info,
                 std::string           func,
                 unsigned              line,
                 std::string           message)
{
  {
    auto level    = static_cast<std::underlying_type_t<LogLevel>>(logLevel);
    auto minLevel = static_cast<std::underlying_type_t<LogLevel>>(this->minLogLevel);
    if (level < minLevel)
      return;
  }

  std::string component = std::string(info.name()) + "::" + func + ":" + std::to_string(line);
  this->logEntries.push_back(LogEntry({logLevel, formatCurrentTime(), component, message}));
}

void Logger::setMinLogLevel(LogLevel logLevel) { this->minLogLevel = logLevel; }

size_t Logger::getNrEntries() const
{
  auto nrEntries = this->logEntries.size();
  return nrEntries;
}

LogEntry Logger::getEntry(size_t index) const { return this->logEntries.at(index); }

} // namespace logging