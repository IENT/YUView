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

#include "SubByteReaderLoggingOptions.h"

namespace parser::reader
{

namespace
{

struct CheckEqualTo : Check
{
public:
  CheckEqualTo() = delete;
  CheckEqualTo(int64_t value, std::string errorIfFail) : Check(errorIfFail), value(value) {}
  CheckResult checkValue(int64_t value) const override;

private:
  int64_t value;
};

struct CheckGreater : Check
{
public:
  CheckGreater() = delete;
  CheckGreater(int64_t value, bool inclusive, std::string errorIfFail)
      : Check(errorIfFail), value(value), inclusive(inclusive)
  {
  }
  CheckResult checkValue(int64_t value) const override;

private:
  int64_t value;
  bool    inclusive;
};

struct CheckSmaller : Check
{
public:
  CheckSmaller() = delete;
  CheckSmaller(int64_t value, bool inclusive, std::string errorIfFail)
      : Check(errorIfFail), value(value), inclusive(inclusive)
  {
  }
  CheckResult checkValue(int64_t value) const override;

private:
  int64_t value;
  bool    inclusive;
};

struct CheckRange : Check
{
public:
  CheckRange() = delete;
  CheckRange(Range<int64_t> range, bool inclusive, std::string errorIfFail)
      : Check(errorIfFail), range(range), inclusive(inclusive)
  {
  }
  CheckResult checkValue(int64_t value) const override;

private:
  Range<int64_t> range;
  bool           inclusive;
};

} // namespace

CheckResult CheckEqualTo::checkValue(int64_t value) const
{
  if (value != this->value)
  {
    if (!this->errorIfFail.empty())
      return CheckResult({this->errorIfFail});
    return CheckResult({"Value should be equal to " + std::to_string(this->value)});
  }
  return {};
}

CheckResult CheckGreater::checkValue(int64_t value) const
{
  auto checkFailed =
      (this->inclusive && value < this->value) || (!this->inclusive && value <= this->value);
  if (checkFailed)
  {
    if (!this->errorIfFail.empty())
      return CheckResult({this->errorIfFail});
    return CheckResult({"Value should be greater then " + std::to_string(this->value) +
                        (this->inclusive ? " inclusive." : " exclusive.")});
  }
  return {};
}

CheckResult CheckSmaller::checkValue(int64_t value) const
{
  auto checkFailed =
      (this->inclusive && value > this->value) || (!this->inclusive && value >= this->value);
  if (checkFailed)
  {
    if (!this->errorIfFail.empty())
      return CheckResult({this->errorIfFail});
    return CheckResult({"Value should be smaller then " + std::to_string(this->value) +
                        (this->inclusive ? " inclusive." : " exclusive.")});
  }
  return {};
}

CheckResult CheckRange::checkValue(int64_t value) const
{
  auto checkFailed = (this->inclusive && (value < this->range.min || value > this->range.max)) ||
                     (!this->inclusive && (value <= this->range.min || value >= this->range.max));
  if (checkFailed)
  {
    if (!this->errorIfFail.empty())
      return CheckResult({this->errorIfFail});
    return CheckResult({"Value should be in the range of " + std::to_string(this->range.min) +
                        " to " + std::to_string(this->range.max) +
                        (this->inclusive ? " inclusive." : " exclusive.")});
  }
  return {};
}

Options &&Options::withMeaning(const std::string &meaningString)
{
  this->meaningString = meaningString;
  return std::move(*this);
}

Options &&Options::withMeaningMap(const MeaningMap &meaningMap)
{
  this->meaningMap = meaningMap;
  return std::move(*this);
}

Options &&Options::withMeaningVector(const std::vector<std::string> &meaningVector)
{
  // This is just a conveniance function. We still save the data in the map starting with an index
  // of 0
  for (unsigned i = 0; i < meaningVector.size(); i++)
    this->meaningMap[i] = meaningVector[i];
  return std::move(*this);
}

Options &&Options::withMeaningFunction(const std::function<std::string(int64_t)> &meaningFunction)
{
  this->meaningFunction = meaningFunction;
  return std::move(*this);
}

Options &&Options::withCheckEqualTo(int64_t value, const std::string &errorIfFail)
{
  this->checkList.emplace_back(std::make_unique<CheckEqualTo>(value, errorIfFail));
  return std::move(*this);
}

Options &&Options::withCheckGreater(int64_t value, bool inclusive, const std::string &errorIfFail)
{
  this->checkList.emplace_back(std::make_unique<CheckGreater>(value, inclusive, errorIfFail));
  return std::move(*this);
}

Options &&Options::withCheckSmaller(int64_t value, bool inclusive, const std::string &errorIfFail)
{
  this->checkList.emplace_back(std::make_unique<CheckSmaller>(value, inclusive, errorIfFail));
  return std::move(*this);
}

Options &&
Options::withCheckRange(Range<int64_t> range, bool inclusive, const std::string &errorIfFail)
{
  this->checkList.emplace_back(std::make_unique<CheckRange>(range, inclusive, errorIfFail));
  return std::move(*this);
}

Options &&Options::withLoggingDisabled()
{
  this->loggingDisabled = true;
  return std::move(*this);
}

} // namespace parser::reader