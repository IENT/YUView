/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut f�r Nachrichtentechnik, RWTH Aachen University, GERMANY
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

#include "common/typedef.h"

#include <functional>
#include <memory>
#include <optional>

namespace parser::reader
{

using MeaningMap = std::map<int, std::string>;

struct CheckResult
{
  explicit    operator bool() const { return this->errorMessage.empty(); }
  std::string errorMessage;
};

class Check
{
public:
  Check() = default;
  Check(std::string errorIfFail) : errorIfFail(errorIfFail){};
  virtual ~Check() = default;

  virtual CheckResult checkValue(int64_t value) const = 0;

  std::string errorIfFail;
};

struct Options
{
  Options() = default;

  [[nodiscard]] Options &&withMeaning(const std::string &meaningString);
  [[nodiscard]] Options &&withMeaningMap(const MeaningMap &meaningMap);
  [[nodiscard]] Options &&withMeaningVector(const std::vector<std::string> &meaningVector);
  [[nodiscard]] Options &&
  withMeaningFunction(const std::function<std::string(int64_t)> &meaningFunction);
  [[nodiscard]] Options &&withCheckEqualTo(int64_t value, const std::string &errorIfFail = {});
  [[nodiscard]] Options &&
  withCheckGreater(int64_t value, bool inclusive = true, const std::string &errorIfFail = {});
  [[nodiscard]] Options &&
  withCheckSmaller(int64_t value, bool inclusive = true, const std::string &errorIfFail = {});
  [[nodiscard]] Options &&
  withCheckRange(Range<int64_t> range, bool inclusive = true, const std::string &errorIfFail = {});
  [[nodiscard]] Options &&withLoggingDisabled();

  std::string                         meaningString;
  std::map<int, std::string>          meaningMap;
  std::function<std::string(int64_t)> meaningFunction;
  std::vector<std::unique_ptr<Check>> checkList;
  bool                                loggingDisabled{false};
};

} // namespace parser::reader